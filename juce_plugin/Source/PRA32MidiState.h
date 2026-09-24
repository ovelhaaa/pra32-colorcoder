#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

// -----------------------------------------------------------------------------
// Framework-free MIDI state logic shared by the JUCE wrapper and the automated
// tests. Nothing here knows about JUCE, APVTS or the host, so the rules that
// keep MIDI, the parameter state and the engine coherent can be regression
// tested without the plugin framework.
//
// Three concerns live here:
//   1. the tiny, pure rules that map MIDI Program Change / PC-by-CC to a preset
//      index (mirroring the PRA32-U2 engine semantics exactly),
//   2. a single monotonic event sequence plus the pure "latest intentional event
//      wins" rule shared by MIDI CC, Program Change and GUI/host edits, and
//   3. realtime-safe mailboxes that let the audio thread hand work to the
//      message thread without ever letting an obsolete value overwrite a newer
//      change.
// -----------------------------------------------------------------------------

namespace PRA32MidiState
{

// --- Program / preset mapping ------------------------------------------------
// The wrapper exposes 26 factory presets (8 user + 18 factory). This is a
// wrapper-level table: a preset is applied by writing every parameter to the
// APVTS, so it is independent of the embedded engine's 16-program ROM table.
inline constexpr int kFactoryProgramCount = 26;

// Program numbers outside [0, 25] are invalid. The wrapper must ignore (never
// clamp) them, mirroring the engine's program_change() bounds.
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
// Ordered deferred-event model.
//
// Every intentional event that can change parameter state -- a MIDI CC, a
// Program Change, or a GUI/host edit -- is tagged with a strictly increasing
// sequence number drawn from one monotonic counter. "Latest intentional event
// wins" then reduces to a comparison, so no older operation can ever overwrite
// a newer one.
//
// Memory model:
//   * The generator uses a relaxed fetch_add: we only need uniqueness and
//     monotonicity here, not ordering against unrelated memory.
//   * Publishers write the payload with relaxed stores and then release-publish
//     the sequence. Consumers acquire-read the sequence before touching the
//     payload it protects. That release/acquire pair is the only synchronisation
//     edge needed: no mutex, no allocation, no waiting, no spinning.
// -----------------------------------------------------------------------------
using Sequence = std::uint64_t;
inline constexpr Sequence kNoSequence = 0;

class SequenceGenerator
{
public:
    Sequence next() noexcept
    {
        return counter.fetch_add (1, std::memory_order_relaxed) + 1;
    }

    Sequence peek() const noexcept
    {
        return counter.load (std::memory_order_relaxed);
    }

private:
    std::atomic<Sequence> counter { 0 };
};

// The source that owns a parameter's final value.
enum class ParamEventSource { None, Host, Midi, Program };

// Resolves "latest intentional event wins" for a single parameter. A single
// global generator makes the three candidates distinct, so a strict comparison
// is total; kNoSequence means "no such event".
inline ParamEventSource resolveParamEventSource (Sequence hostSequence,
                                                 Sequence midiSequence,
                                                 Sequence programSequence) noexcept
{
    const Sequence best = std::max ({ hostSequence, midiSequence, programSequence });

    if (best == kNoSequence)     return ParamEventSource::None;
    if (midiSequence == best)    return ParamEventSource::Midi;
    if (programSequence == best) return ParamEventSource::Program;
    return ParamEventSource::Host;
}

// -----------------------------------------------------------------------------
// Single-slot, lock-free mailbox for MIDI-CC-driven parameter writes.
//
// The audio thread publishes the value it has just sent to the engine; the
// message thread later mirrors it into the APVTS. Consuming is a two-phase
// operation: `observe()` samples the current publication, `tryConsume()` removes
// it only if it is still *exactly* the observed generation. If the producer
// republished in between, the compare-exchange fails and the newer publication
// is left completely intact -- a consumer can never discard a value it did not
// read.
//
// A per-parameter `hostSequence` watermark records the last GUI/host change. It
// lives here so the ordering rule has all three candidates in one place.
// -----------------------------------------------------------------------------
class PendingParameterMailbox
{
public:
    static constexpr int kCapacity = 128;

    struct Observation
    {
        bool     valid        = false;
        int      value        = -1;
        int      preValue     = -1;
        Sequence sequence     = kNoSequence;
        Sequence hostSequence = kNoSequence;
    };

    void clearAll() noexcept
    {
        for (auto& e : entries)
        {
            e.sequence.store (kNoSequence, std::memory_order_relaxed);
            e.hostSequence.store (kNoSequence, std::memory_order_relaxed);
            e.value.store (-1, std::memory_order_relaxed);
            e.preValue.store (-1, std::memory_order_relaxed);
        }
    }

    // Producer (audio thread): payload first, then release-publish the sequence.
    void publish (int index, int value, int preValue, Sequence sequence) noexcept
    {
        auto& e = entries[(size_t) index];
        e.preValue.store (preValue, std::memory_order_relaxed);
        e.value.store (value, std::memory_order_relaxed);
        e.sequence.store (sequence, std::memory_order_release);
    }

    // Producer (any thread): a GUI/host edit took `index` over.
    void markHostChange (int index, Sequence sequence) noexcept
    {
        entries[(size_t) index].hostSequence.store (sequence, std::memory_order_release);
    }

    Sequence hostSequence (int index) const noexcept
    {
        return entries[(size_t) index].hostSequence.load (std::memory_order_acquire);
    }

    // Consumer (message thread): acquire-read the sequence before the payload.
    Observation observe (int index) const noexcept
    {
        const auto& e = entries[(size_t) index];

        Observation obs;
        obs.hostSequence = e.hostSequence.load (std::memory_order_acquire);
        obs.sequence     = e.sequence.load (std::memory_order_acquire);

        if (obs.sequence != kNoSequence)
        {
            obs.value    = e.value.load (std::memory_order_relaxed);
            obs.preValue = e.preValue.load (std::memory_order_relaxed);
            obs.valid    = obs.value >= 0;
        }

        return obs;
    }

    // Consumer: remove exactly the observed publication. Returns false (and
    // preserves the newer publication) if the producer republished since `obs`.
    bool tryConsume (int index, const Observation& obs) noexcept
    {
        if (! obs.valid || obs.sequence == kNoSequence)
            return false;

        auto& e = entries[(size_t) index];
        Sequence expected = obs.sequence;

        return e.sequence.compare_exchange_strong (expected, kNoSequence,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
    }

private:
    struct Entry
    {
        std::atomic<Sequence> sequence     { kNoSequence };
        std::atomic<Sequence> hostSequence { kNoSequence };
        std::atomic<int>      value        { -1 };
        std::atomic<int>      preValue     { -1 };
    };

    std::array<Entry, (size_t) kCapacity> entries {};
};

// -----------------------------------------------------------------------------
// Single-slot, lock-free Program Change mailbox. Only the most recent program
// matters for the deferred APVTS/UI mirror: the engine has already applied every
// intermediate program at its own sample. `clearIfUnchanged` refuses to remove a
// slot that was republished while the mirror was in flight.
// -----------------------------------------------------------------------------
class PendingProgramSlot
{
public:
    struct Snapshot
    {
        int      program  = -1;
        Sequence sequence = kNoSequence;
    };

    void publish (int program, Sequence sequence) noexcept
    {
        value.store (program, std::memory_order_relaxed);
        seq.store (sequence, std::memory_order_release);
    }

    Snapshot peek() const noexcept
    {
        Snapshot s;
        s.sequence = seq.load (std::memory_order_acquire);
        s.program  = value.load (std::memory_order_relaxed);
        return s;
    }

    bool clearIfUnchanged (const Snapshot& snapshot) noexcept
    {
        if (snapshot.sequence == kNoSequence)
            return false;

        Sequence expected = snapshot.sequence;
        return seq.compare_exchange_strong (expected, kNoSequence,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire);
    }

    void clear() noexcept
    {
        seq.store (kNoSequence, std::memory_order_relaxed);
        value.store (-1, std::memory_order_relaxed);
    }

private:
    std::atomic<int>      value { -1 };
    std::atomic<Sequence> seq   { kNoSequence };
};

// -----------------------------------------------------------------------------
// The pure decision the message thread makes for one parameter. Kept here so the
// processor and the regression tests share exactly the same rule:
//   * MIDI wins      -> apply the MIDI value, drop its mailbox entry.
//   * Program wins   -> apply the preset value, drop any stale MIDI entry.
//   * Host wins      -> keep the APVTS value, drop any stale MIDI entry.
// -----------------------------------------------------------------------------
struct DeferredResolution
{
    ParamEventSource source            = ParamEventSource::None;
    int              value             = -1;
    bool             shouldConsumeMidi = false;
    bool             shouldWriteValue  = false;
};

inline DeferredResolution resolveDeferredParameter (const PendingParameterMailbox::Observation& obs,
                                                    Sequence programSequence,
                                                    int programValue) noexcept
{
    DeferredResolution r;
    r.source = resolveParamEventSource (obs.hostSequence, obs.sequence, programSequence);

    if (r.source == ParamEventSource::Midi)
    {
        r.shouldConsumeMidi = true;
        r.shouldWriteValue  = true;
        r.value             = obs.value;
    }
    else if (r.source == ParamEventSource::Program)
    {
        r.shouldConsumeMidi = obs.valid;
        r.shouldWriteValue  = true;
        r.value             = programValue;
    }
    else
    {
        r.shouldConsumeMidi = obs.valid;
    }

    return r;
}

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
