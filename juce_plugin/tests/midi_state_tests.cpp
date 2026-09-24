// -----------------------------------------------------------------------------
// PRA32-U2 MIDI state regression tests (Milestone A.1).
//
// JUCE-free tests for the MIDI -> engine / MIDI -> APVTS coherence rules:
//   * Program Change validity (0..15 valid, >15 ignored, never clamped),
//   * Program Change by CC (CC112..CC119) gate semantics,
//   * the realtime mailbox that stops a stale MIDI CC value from overwriting a
//     newer GUI/host change,
//   * the sample-accurate MIDI dispatch used by processBlock().
// -----------------------------------------------------------------------------

#include "PRA32MidiState.h"

#include <cstdio>
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

    // Re-sending the same "high" value must not re-dispatche a program change.
    CHECK (! pcByCcTriggers (127, 127));
    CHECK (! pcByCcTriggers (64, 127));
}

void testMailboxMirrorsMidiWithoutConflict()
{
    std::printf ("mailbox: MIDI mirrored when APVTS still at pre-value...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (3, 90, 40);   // MIDI CC -> 90, previous APVTS value was 40

    int out = -1;
    CHECK (box.consume (3, 40, out));
    CHECK (out == 90);

    // Consuming twice must not re-apply the same value.
    CHECK (! box.consume (3, 90, out));
}

void testMailboxHostWinsOverStaleMidi()
{
    std::printf ("mailbox: newer GUI/host change wins...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (5, 90, 40);          // MIDI CC queued (engine already at 90)

    // Before the flush, the host/GUI moved the parameter to 100.
    int out = -1;
    CHECK (! box.consume (5, 100, out));

    // The stale MIDI value must never be applied later either.
    CHECK (! box.consume (5, 40, out));
}

void testMailboxInvalidation()
{
    std::printf ("mailbox: audio-thread invalidation rejects stale values...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (7, 90, 40);
    box.invalidate (7);               // audio thread saw the host take over

    int out = -1;
    CHECK (! box.consume (7, 40, out));

    // A later MIDI CC gets a fresh generation and is accepted again.
    box.publish (7, 55, 40);
    CHECK (box.consume (7, 40, out));
    CHECK (out == 55);
}

void testMailboxLatestMidiWins()
{
    std::printf ("mailbox: latest MIDI value wins...\n");

    PendingParameterMailbox box;
    box.clearAll();

    box.publish (2, 10, 0);
    box.publish (2, 20, 0);           // newer MIDI CC

    int out = -1;
    CHECK (box.consume (2, 0, out));
    CHECK (out == 20);
}

void testMailboxMidiAfterHostWins()
{
    std::printf ("mailbox: MIDI change after a GUI/host change wins...\n");

    PendingParameterMailbox box;
    box.clearAll();

    // The host/GUI already moved the parameter to 70 and the audio thread has
    // adopted it, so the pre-MIDI value for the next CC is 70.
    box.publish (9, 55, 70);

    int out = -1;
    CHECK (box.consume (9, 70, out));
    CHECK (out == 55);                // the newer MIDI change is mirrored
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
    testMailboxMirrorsMidiWithoutConflict();
    testMailboxHostWinsOverStaleMidi();
    testMailboxInvalidation();
    testMailboxLatestMidiWins();
    testMailboxMidiAfterHostWins();
    testSampleAccurateDispatch();

    std::printf ("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
