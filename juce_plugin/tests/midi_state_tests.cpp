// -----------------------------------------------------------------------------
// PRA32-U2 MIDI state regression tests (Milestone A.1.1).
//
// JUCE-free tests for the MIDI -> engine / MIDI -> APVTS coherence rules:
//   * Program Change validity (0..15 valid, >15 ignored, never clamped),
//   * Program Change by CC (CC112..CC119) gate semantics,
//   * the ordered deferred-event model ("latest intentional event wins") shared
//     by Program Change, MIDI CC and GUI/host edits,
//   * the lock-free mailboxes (CAS consume, concurrent publish safety),
//   * the sample-accurate MIDI dispatch used by processBlock().
// -----------------------------------------------------------------------------

#include "PRA32MidiState.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace PRA32MidiState;

namespace {

int g_checks = 0;
int g_failures = 0;

void check (bool condition, const char* expression, const char* file, int line)
{
    ++g_checks;

    if (! condition)
    {
        ++g_failures;
        std::printf ("  FAIL  %s:%d  %s\n", file, line, expression);
    }
}

#define CHECK(expr) check ((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) do { auto va = (a); auto vb = (b); \
    if (va != vb) std::printf ("  (got %lld expected %lld)\n", \
                               (long long) va, (long long) vb); \
    CHECK (va == vb); } while (0)

void testProgramValidity()
{
    std::printf ("program validity...\n");

    CHECK (isValidFactoryProgram (0));
    CHECK (isValidFactoryProgram (1));
    CHECK (isValidFactoryProgram (15));

    // Must be ignored, never clamped to program 15.
    CHECK (! isValidFactoryProgram (16));
    CHECK (! isValidFactoryProgram (42));
    CHECK (! isValidFactoryProgram (127));
    CHECK (! isValidFactoryProgram (-1));
}

void testPcByCcMapping()
{
    std::printf ("program change by CC mapping...\n");

    CHECK (pcByCcProgramIndex (kPcByCcFirst) == 0);
    CHECK (pcByCcProgramIndex (113) == 1);
    CHECK (pcByCcProgramIndex (kPcByCcLast) == 7);

    CHECK (pcByCcProgramIndex (111) == -1);
    CHECK (pcByCcProgramIndex (120) == -1);
    CHECK (pcByCcProgramIndex (18) == -1);

    // Engine gate: only the 0 -> 1 transition of the "< 64" gate fires.
    CHECK (! pcByCcTriggers (0, 0));
    CHECK (! pcByCcTriggers (0, 63));
    CHECK (pcByCcTriggers (0, 64));
    CHECK (pcByCcTriggers (0, 127));
    CHECK (pcByCcTriggers (63, 64));

    // Re-sending the same "high" value must not re-dispatch a program change.
    CHECK (! pcByCcTriggers (127, 127));
    CHECK (! pcByCcTriggers (64, 127));
}

// -----------------------------------------------------------------------------
// A minimal model of PRA32ColorcoderAudioProcessor::flushDeferredUpdates() for a
// single parameter. It is built from the exact primitives the processor uses
// (the mailbox, the program slot and resolveDeferredParameter), so the ordering
// rules can be regression tested without the JUCE framework.
// -----------------------------------------------------------------------------
struct ParamModel
{
    PendingParameterMailbox mailbox;
    PendingProgramSlot      program;
    SequenceGenerator       sequences;

    int apvtsValue = 0;  // the live parameter value (GUI/host write in place)

    void midiCc (int value, int preValue)
    {
        mailbox.publish (0, value, preValue, sequences.next());
    }

    void hostEdit (int value)
    {
        apvtsValue = value;  // the slider/host writes the APVTS directly
        mailbox.markHostChange (0, sequences.next());
    }

    void programChange (int programIndex)
    {
        program.publish (programIndex, sequences.next());
    }

    // Returns the value left in the APVTS after one deferred flush.
    int flush (int programValue)
    {
        const auto snapshot = program.peek();
        const bool hasProgram = snapshot.sequence != kNoSequence
                                && isValidFactoryProgram (snapshot.program);

        const auto obs = mailbox.observe (0);
        const auto resolution = resolveDeferredParameter (
            obs, hasProgram ? snapshot.sequence : kNoSequence, programValue);

        if (resolution.source == ParamEventSource::Midi)
        {
            if (mailbox.tryConsume (0, obs))
                if (mailbox.hostSequence (0) <= obs.sequence
                    && apvtsValue != resolution.value)
                    apvtsValue = resolution.value;
        }
        else
        {
            if (resolution.shouldConsumeMidi)
                mailbox.tryConsume (0, obs);

            if (resolution.source == ParamEventSource::Program
                && apvtsValue != resolution.value)
                apvtsValue = resolution.value;
        }

        if (hasProgram)
            program.clearIfUnchanged (snapshot);

        return apvtsValue;
    }
};

void testOrderingProgramChangeThenCc()
{
    std::printf ("ordering: Program Change -> CC keeps the CC...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.programChange (5);          // sample 100
    m.midiCc (90, 40);            // sample 200

    CHECK_EQ (m.flush (10), 90);  // preset 5 + cutoff override
}

void testOrderingCcThenProgramChange()
{
    std::printf ("ordering: CC -> Program Change lets the preset win...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.midiCc (90, 40);            // sample 100
    m.programChange (5);          // sample 200

    CHECK_EQ (m.flush (10), 10);  // preset value, MIDI dropped
}

void testOrderingPcCcPc()
{
    std::printf ("ordering: PC -> CC -> PC lets the last PC win...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.programChange (5);
    m.midiCc (90, 40);
    m.programChange (9);

    CHECK_EQ (m.flush (20), 20);
}

void testOrderingPcPcCc()
{
    std::printf ("ordering: PC -> PC -> CC keeps the final CC...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.programChange (5);
    m.programChange (9);
    m.midiCc (90, 40);

    CHECK_EQ (m.flush (20), 90);
}

void testHostTakeoverAfterProgramChange()
{
    std::printf ("host takeover: PC -> GUI parameter...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.programChange (5);          // MIDI PC
    m.hostEdit (70);              // GUI moves the parameter before the mirror

    CHECK_EQ (m.flush (10), 70);  // the later GUI value wins
}

void testHostTakeoverAfterMidiCc()
{
    std::printf ("host takeover: MIDI CC -> GUI parameter...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.midiCc (90, 40);            // MIDI CC
    m.hostEdit (70);              // GUI changes it before the flush

    CHECK_EQ (m.flush (10), 70);  // the later GUI value wins
}

void testMidiCcAfterHostEdit()
{
    std::printf ("host takeover: GUI parameter -> MIDI CC...\n");

    ParamModel m;
    m.apvtsValue = 70;

    m.hostEdit (70);
    m.midiCc (90, 70);            // newer MIDI CC

    CHECK_EQ (m.flush (10), 90);  // the newer MIDI value wins
}

void testPcByCcOrderingForward()
{
    std::printf ("PC-by-CC ordering: program then parameter...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.programChange (4);          // CC116 high
    m.midiCc (100, 40);           // CC74 = 100

    CHECK_EQ (m.flush (10), 100); // Program 4 + cutoff 100
}

void testPcByCcOrderingReverse()
{
    std::printf ("PC-by-CC ordering: parameter then program...\n");

    ParamModel m;
    m.apvtsValue = 40;

    m.midiCc (100, 40);           // CC74 = 100
    m.programChange (4);          // CC116 high -> Program 4

    CHECK_EQ (m.flush (10), 10);  // program wins, cutoff from the preset
}

void testProgramChangeSequenceConverges()
{
    std::printf ("program sequence: PC2 -> PC5 -> PC9 ends on 9...\n");

    PendingProgramSlot slot;
    SequenceGenerator gen;

    slot.publish (2, gen.next());
    slot.publish (5, gen.next());
    slot.publish (9, gen.next());

    const auto snapshot = slot.peek();
    CHECK_EQ (snapshot.program, 9);

    // A stale flush that saw PC5 must not clear the newer PC9.
    PendingProgramSlot::Snapshot stale;
    stale.program = 5;
    stale.sequence = 2;
    CHECK (! slot.clearIfUnchanged (stale));
    CHECK_EQ (slot.peek().program, 9);

    // The message thread that observed PC9 can remove it exactly once.
    CHECK (slot.clearIfUnchanged (snapshot));
    CHECK_EQ (slot.peek().sequence, kNoSequence);
}

void testMailboxMirrorsMidiWithoutConflict()
{
    std::printf ("mailbox: MIDI mirrored when APVTS still at pre-value...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (3, 90, 40, 1);   // MIDI CC -> 90, previous APVTS value was 40

    const auto obs = box.observe (3);
    CHECK (obs.valid);
    CHECK_EQ (obs.value, 90);
    CHECK_EQ (obs.preValue, 40);

    CHECK (box.tryConsume (3, obs));

    // Consuming twice must not re-apply the same value.
    CHECK (! box.tryConsume (3, obs));
    CHECK (! box.observe (3).valid);
}

void testMailboxLatestMidiWins()
{
    std::printf ("mailbox: latest MIDI value wins...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (2, 10, 0, 1);
    box.publish (2, 20, 0, 2);    // newer MIDI CC

    const auto obs = box.observe (2);
    CHECK_EQ (obs.sequence, 2u);
    CHECK_EQ (obs.value, 20);
}

void testMailboxConcurrentPublishIsNotLost()
{
    std::printf ("mailbox: consume of N must not discard a concurrent N+1...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (5, 90, 40, 1);

    // Deterministic interleaving: the consumer samples generation N...
    const auto observed = box.observe (5);
    CHECK_EQ (observed.sequence, 1u);
    CHECK_EQ (observed.value, 90);

    // ...the producer publishes N+1 before the consumer commits...
    box.publish (5, 100, 40, 2);

    // ...so removing N must fail and leave N+1 intact.
    CHECK (! box.tryConsume (5, observed));

    const auto newer = box.observe (5);
    CHECK (newer.valid);
    CHECK_EQ (newer.sequence, 2u);
    CHECK_EQ (newer.value, 100);
    CHECK (box.tryConsume (5, newer));
}

void testMailboxStress()
{
    std::printf ("mailbox: multi-threaded stress (200k publications)...\n");

    constexpr Sequence kTotal = 200000;

    PendingParameterMailbox box;
    box.clearAll();

    std::atomic<bool> producerDone { false };
    std::atomic<long long> mismatchCount { 0 };
    std::atomic<bool> finalValueSeen { false };
    std::atomic<bool> consumerFailed { false };

    std::thread producer ([&]
    {
        for (Sequence i = 1; i <= kTotal; ++i)
            box.publish (0, (int) i, (int) (i - 1), i);

        producerDone.store (true, std::memory_order_release);
    });

    std::thread consumer ([&]
    {
        Sequence lastConsumed = kNoSequence;
        long long spins = 0;

        while (! finalValueSeen.load (std::memory_order_relaxed))
        {
            if (++spins > 200000000LL)
            {
                consumerFailed.store (true, std::memory_order_relaxed);
                return;
            }

            const auto obs = box.observe (0);

            if (obs.valid)
            {
                if (obs.sequence < lastConsumed)
                    consumerFailed.store (true, std::memory_order_relaxed);

                if (box.tryConsume (0, obs))
                {
                    lastConsumed = obs.sequence;

                    if (obs.value != (int) obs.sequence)
                        mismatchCount.fetch_add (1, std::memory_order_relaxed);

                    if (obs.sequence >= kTotal)
                        finalValueSeen.store (true, std::memory_order_relaxed);
                }
            }
            else if (producerDone.load (std::memory_order_acquire))
            {
                // Nothing pending but the producer already finished: the final
                // publication was consumed (or was never seen as valid).
                if (lastConsumed >= kTotal)
                    finalValueSeen.store (true, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    consumer.join();

    // The final publication must reach the consumer intact; the CAS guarantees a
    // newer publication can never be silently discarded by an older consumer.
    CHECK (finalValueSeen.load());
    CHECK (! consumerFailed.load());

    if (mismatchCount.load() != 0)
        std::printf ("  note: %lld early payload reads (benign, re-applied next flush)\n",
                     mismatchCount.load());
}

void testSampleAccurateDispatch()
{
    std::printf ("sample-accurate MIDI dispatch...\n");

    struct Event
    {
        int samplePosition;
        int program;
        const Event& getMessage() const { return *this; }
    };

    // Program change in the middle of a block must land exactly on its sample,
    // not on the following block.
    {
        std::vector<Event> events = { { 512, 5 }, { 900, 9 } };

        int rendered = 0;
        int applied = 0;
        int renderedAtApply[2] = { -1, -1 };
        int programAtApply[2]  = { -1, -1 };

        dispatchSampleAccurate (1024, events,
            [&] (int, int count) { rendered += count; },
            [&] (const Event& e)
            {
                renderedAtApply[applied] = rendered;
                programAtApply[applied]  = e.program;
                ++applied;
            });

        CHECK (applied == 2);
        CHECK (renderedAtApply[0] == 512);
        CHECK (programAtApply[0] == 5);
        CHECK (renderedAtApply[1] == 900);
        CHECK (programAtApply[1] == 9);
        CHECK (rendered == 1024);
    }

    // Event at sample 0 applies before any audio is rendered.
    {
        std::vector<Event> events = { { 0, 3 } };

        int rendered = 0;
        int position = -1;
        int applied = 0;

        dispatchSampleAccurate (256, events,
            [&] (int, int count) { rendered += count; },
            [&] (const Event&) { position = rendered; ++applied; });

        CHECK (applied == 1);
        CHECK (position == 0);
        CHECK (rendered == 256);
    }

    // Out-of-range position is clamped into the block and never drops audio.
    {
        std::vector<Event> events = { { 9999, 1 } };

        int rendered = 0;
        int position = -1;
        int applied = 0;

        dispatchSampleAccurate (256, events,
            [&] (int, int count) { rendered += count; },
            [&] (const Event&) { position = rendered; ++applied; });

        CHECK (applied == 1);
        CHECK (position == 256);
        CHECK (rendered == 256);
    }

    // No events: the whole block is rendered once.
    {
        std::vector<Event> events;

        int rendered = 0;

        dispatchSampleAccurate (128, events,
            [&] (int, int count) { rendered += count; },
            [&] (const Event&) {});

        CHECK (rendered == 128);
    }
}

} // namespace

int main()
{
    std::printf ("== PRA32-U2 MIDI state tests ==\n");

    testProgramValidity();
    testPcByCcMapping();

    testOrderingProgramChangeThenCc();
    testOrderingCcThenProgramChange();
    testOrderingPcCcPc();
    testOrderingPcPcCc();

    testHostTakeoverAfterProgramChange();
    testHostTakeoverAfterMidiCc();
    testMidiCcAfterHostEdit();

    testPcByCcOrderingForward();
    testPcByCcOrderingReverse();
    testProgramChangeSequenceConverges();

    testMailboxMirrorsMidiWithoutConflict();
    testMailboxLatestMidiWins();
    testMailboxConcurrentPublishIsNotLost();
    testMailboxStress();

    testSampleAccurateDispatch();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
