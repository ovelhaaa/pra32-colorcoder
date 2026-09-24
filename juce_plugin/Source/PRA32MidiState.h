#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// -----------------------------------------------------------------------------
// Framework-free MIDI state logic shared by the JUCE wrapper and the automated
// tests. Nothing here knows about JUCE, APVTS or the host, so the rules that
// keep MIDI, the parameter state and the engine coherent can be regression
// tested without the plugin framework.
//
// Two concerns live here:
//   1. the tiny, pure rules that map MIDI Program Change / PC-by-CC to a preset
//      index (mirroring the PRA32-U2 engine semantics exactly), and
//   2. a realtime-safe mailbox that lets the audio thread hand a MIDI-CC value
//      to the message thread without ever letting an obsolete value overwrite a
//      newer GUI/host change.
// -----------------------------------------------------------------------------

namespace PRA32MidiState
{

// --- Program / preset mapping ------------------------------------------------
// The wrapper exposes the engine's 16 factory programs (PROGRAM_NUMBER_MAX + 1).
inline constexpr int kFactoryProgramCount = 16;

// Program numbers outside [0, 15] are invalid. The engine's program_change()
// silently ignores them, so the wrapper must ignore (never clamp) them as well.
inline constexpr bool isValidFactoryProgram (int program) noexcept
{
    return program >= 0 && program < kFactoryProgramCount;
}

// --- Program Change by CC (CC112..CC119) -------------------------------------

inline constexpr int kPcByCcFirst = 112;
inline constexpr int kPcByCcLast  = 119;

// Index 0..7 for CC112..CC119, or -1 when the controller is not a PC-by-CC.
inline constexpr int pcByCcProgramIndex (int controller) noexcept
{
    return (controller >= kPcByCcFirst && controller <= kPcByCcLast)
               ? controller - kPcByCcFirst
               : -1;
}

// The engine fires the internal program change only on the 0->1 transition of
// the "< 64" gate, exactly as PRA32_U2_Synth::control_change() does.
inline constexpr bool pcByCcTriggers (int previousValue, int newValue) noexcept
{
    return previousValue < 64 && newValue >= 64;
}

// -----------------------------------------------------------------------------
// Realtime-safe mailbox for MIDI-CC-driven parameter writes.
//
// The audio thread publishes the value it has just sent to the engine; the
// message thread later mirrors it into the APVTS. A generation counter plus an
// "invalidated" watermark guarantee "latest intentional change wins": once the
// audio thread detects that the host/GUI took a parameter over, every older
// MIDI value is rejected forever, so a stale mailbox entry can never overwrite
// a newer host/GUI change. No allocation, no locks, no waits.
// -----------------------------------------------------------------------------
class PendingParameterMailbox
{
public:
    static constexpr int kCapacity = 128;

    void clearAll() noexcept
    {
        for (auto& e : entries)
        {
            e.value.store (-1, std::memory_order_relaxed);
            e.preValue.store (-1, std::memory_order_relaxed);
            e.generation.store (0, std::memory_order_relaxed);
            e.invalidated.store (0, std::memory_order_relaxed);
        }
    }

    // Audio thread: remember a MIDI-CC value for `index`. `preValue` is the
    // APVTS value from before the MIDI take-over.
    void publish (int index, int value, int preValue) noexcept
    {
        auto& e = entries[(size_t) index];
        e.preValue.store (preValue, std::memory_order_relaxed);
        e.value.store (value, std::memory_order_relaxed);
        e.generation.fetch_add (1, std::memory_order_release);
    }

    // Audio thread: the APVTS/host now owns `index`. Reject everything published
    // so far; a later publish gets a newer generation and is accepted again.
    void invalidate (int index) noexcept
    {
        auto& e = entries[(size_t) index];
        e.invalidated.store (e.generation.load (std::memory_order_relaxed),
                             std::memory_order_release);
    }

    // Message thread: returns true and writes `outValue` when the pending MIDI
    // value should be mirrored into the APVTS. `currentValue` is the live APVTS
    // value; if it already moved away from both the pre-MIDI value and the MIDI
    // value, someone else owns the parameter and the request is dropped.
    bool consume (int index, int currentValue, int& outValue) noexcept
    {
        auto& e = entries[(size_t) index];

        const int value = e.value.load (std::memory_order_relaxed);

        if (value < 0)
            return false;

        const uint32_t generation = e.generation.load (std::memory_order_acquire);

        if (generation == 0
            || generation <= e.invalidated.load (std::memory_order_acquire))
        {
            clear (index);
            return false;
        }

        const int preValue = e.preValue.load (std::memory_order_relaxed);

        if (currentValue != preValue && currentValue != value)
        {
            // GUI/host automation moved the parameter after the MIDI CC was
            // queued; the newer change wins.
            invalidate (index);
            clear (index);
            return false;
        }

        clear (index);
        outValue = value;
        return true;
    }

    void clear (int index) noexcept
    {
        entries[(size_t) index].value.store (-1, std::memory_order_relaxed);
    }

private:
    struct Entry
    {
        std::atomic<int>      value       { -1 };
        std::atomic<int>      preValue    { -1 };
        std::atomic<uint32_t> generation  { 0 };
        std::atomic<uint32_t> invalidated { 0 };
    };

    std::array<Entry, (size_t) kCapacity> entries {};
};

// -----------------------------------------------------------------------------
// Sample-accurate MIDI dispatch (used by AudioProcessor::processBlock).
//
// `events` is an ordered range whose elements expose `.samplePosition` and
// `.getMessage()`. `renderRange(start, count)` renders audio, `apply(message)`
// applies a MIDI event at its exact sample position. Rendering is split so the
// event takes effect from its own sample on, never from the next block.
// No allocation, no locks.
// -----------------------------------------------------------------------------
template <typename EventRange, typename RenderRange, typename ApplyEvent>
inline void dispatchSampleAccurate (int numSamples, EventRange&& events,
                                    RenderRange&& renderRange, ApplyEvent&& applyEvent)
{
    int startSample = 0;

    for (auto&& metadata : events)
    {
        int eventPosition = metadata.samplePosition;

        if (eventPosition < 0)          eventPosition = 0;
        if (eventPosition > numSamples) eventPosition = numSamples;

        if (eventPosition > startSample)
        {
            renderRange (startSample, eventPosition - startSample);
            startSample = eventPosition;
        }

        applyEvent (metadata.getMessage());
    }

    if (startSample < numSamples)
        renderRange (startSample, numSamples - startSample);
}

} // namespace PRA32MidiState
