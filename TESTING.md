# Tonecoder TC-32 0.9.0-beta.1 testing

This is a release candidate until mandatory CI and pluginval pass. Read BUILD.txt
for the exact commit; do not report only the package name.

Copy the entire `Tonecoder TC-32.vst3` bundle to
`C:\Program Files\Common Files\VST3\` (or your DAW's user VST3 directory),
then rescan plugins. Run the supplied Standalone executable for an initial audio/MIDI
smoke test. No installer is required.

Please test REAPER, Ableton Live, Cubase, Studio One and FL Studio on Windows x64.
Use 44.1, 48, 88.2, 96, 176.4 and 192 kHz where the DAW supports them, and buffers
1/16/32/64/128/256/512/1024/2048 where available. Compare pitch, envelopes, LFO
and delay timing; listen for clicks at buffer boundaries and during automation.

Exercise four-note chords, a fifth note, repeated and overlapping notes, sustain,
All Notes Off (CC123), All Sound Off (CC120), pitch bend, velocity, channel pressure,
poly aftertouch and breath (CC2). Switch POLY/MONO/LEGATO/LEGATO+PORTA with held
notes. Test Program Change and low/high transitions on CC112–119.

Open every page, especially FILTER cutoff/resonance. Automate controls, save and
reload the DAW session, and verify factory edits remain marked modified. Load/save
JSON patches; malformed documents should leave the current patch intact. Reopen
the editor repeatedly, change device rate/buffer and stop/restart transport.

Report bugs at https://github.com/ovelhaaa/pra32-colorcoder/issues with:
- package version and BUILD.txt SHA;
- Windows version, CPU/audio interface and driver;
- DAW name/version, sample rate and block size;
- exact reproduction steps, expected/actual result;
- affected preset/JSON/session and logs or short audio example where useful.

Do not include personal information in shared sessions or logs.
