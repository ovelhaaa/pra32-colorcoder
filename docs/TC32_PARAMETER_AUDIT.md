# TC-32 parameter coverage audit

Authority: `SynthParameters::getParameters()`; 52 parameters, unchanged IDs/ranges.
Each row is covered by automated GUI hierarchy/attachment checks, APVTS host
min/max/default writes, engine controller readback, MIDI CC/deferred host mirror,
all-parameter session and JSON roundtrips, and canonical factory-table/JSON validation.
ENVS knobs are owned by EnvelopePanel; other controls by their ModulePanel.
The GUI audit found and fixed FILTER parenting and the REL = DEC toggle's 0/1
versus 0/127 attachment mismatch. Parameters describe engine controllers, including
nonuniform enum bands; no audible equivalence is assumed between different CC values.

| APVTS / host / state ID | GUI page | Engine CC | Range | Default | Factory / JSON key |
|---|---|---:|---|---:|---|
| `osc1Wave` | OSC | 102 | 0–127 | 0 | `OSC_1_WAVE` |
| `osc1Shape` | OSC | 19 | 0–127 | 64 | `OSC_1_SHAPE` |
| `osc1Morph` | OSC | 20 | 0–127 | 0 | `OSC_1_MORPH` |
| `oscDrift` | OSC | 82 | 0–127 | 32 | `OSC_DRIFT` |
| `sawWMode` | OSC | 83 | 0–127 | 127 | `OSC_SAW_W_MODE` |
| `osc2Wave` | OSC | 104 | 0–127 | 0 | `OSC_2_WAVE` |
| `osc2Coarse` | OSC | 85 | 0–127 | 64 | `OSC_2_COARSE` |
| `osc2Pitch` | OSC | 76 | 0–127 | 72 | `OSC_2_PITCH` |
| `oscMix` | OSC | 21 | 0–127 | 64 | `MIXER_OSC_MIX` |
| `subOsc` | OSC | 23 | 0–127 | 64 | `MIXER_SUB_OSC` |
| `filterCutoff` | FILTER | 74 | 0–127 | 112 | `FILTER_CUTOFF` |
| `filterReso` | FILTER | 71 | 0–127 | 48 | `FILTER_RESO` |
| `filterMode` | FILTER | 39 | 0–127 | 0 | `FILTER_MODE` |
| `egFltAmt` | FILTER | 24 | 0–127 | 40 | `FILTER_EG_AMT` |
| `filterKeyTrk` | FILTER | 9 | 0–127 | 96 | `FILTER_KEY_TRK` |
| `bthFltAmt` | FILTER | 60 | 0–127 | 64 | `BTH_FILTER_AMT` |
| `relEqDcy` | FILTER | 105 | 0–127 | 127 | `REL_EQ_DECAY` |
| `egAttack` | ENVS | 73 | 0–127 | 96 | `EG_ATTACK` |
| `egDecay` | ENVS | 75 | 0–127 | 96 | `EG_DECAY` |
| `egSustain` | ENVS | 30 | 0–127 | 0 | `EG_SUSTAIN` |
| `egRelease` | ENVS | 72 | 0–127 | 32 | `EG_RELEASE` |
| `ampAttack` | ENVS | 52 | 0–127 | 32 | `AMP_ATTACK` |
| `ampDecay` | ENVS | 53 | 0–127 | 32 | `AMP_DECAY` |
| `ampSustain` | ENVS | 54 | 0–127 | 127 | `AMP_SUSTAIN` |
| `ampRelease` | ENVS | 55 | 0–127 | 32 | `AMP_RELEASE` |
| `egOscAmt` | CHARACTER | 89 | 0–127 | 64 | `EG_OSC_AMT` |
| `egOscDst` | CHARACTER | 8 | 0–127 | 0 | `EG_OSC_DST` |
| `lfoWave` | MOD | 33 | 0–127 | 0 | `LFO_WAVE` |
| `lfoRate` | MOD | 3 | 0–127 | 80 | `LFO_RATE` |
| `lfoFadeTime` | MOD | 56 | 0–127 | 0 | `LFO_FADE_TIME` |
| `lfoDepth` | MOD | 17 | 0–127 | 0 | `LFO_DEPTH` |
| `lfoFltAmt` | MOD | 25 | 0–127 | 76 | `LFO_FILTER_AMT` |
| `lfoOscAmt` | MOD | 13 | 0–127 | 64 | `LFO_OSC_AMT` |
| `lfoOscDst` | MOD | 103 | 0–127 | 0 | `LFO_OSC_DST` |
| `pbRange` | CHARACTER | 57 | 0–127 | 2 | `P_BEND_RANGE` |
| `portaTime` | CHARACTER | 5 | 0–127 | 48 | `PORTAMENTO` |
| `chorusMix` | FX | 93 | 0–127 | 127 | `CHORUS_MIX` |
| `choRate` | FX | 58 | 0–127 | 64 | `CHORUS_RATE` |
| `choDepth` | FX | 59 | 0–127 | 64 | `CHORUS_DEPTH` |
| `delayTime` | FX | 90 | 0–127 | 87 | `DELAY_TIME` |
| `delayDepth` | FX | 94 | 0–127 | 64 | `DELAY_LEVEL` |
| `delayFeedback` | FX | 92 | 0–127 | 64 | `DELAY_FEEDBACK` |
| `delayMode` | FX | 35 | 0–127 | 0 | `DELAY_MODE` |
| `pan` | CHARACTER | 10 | 0–127 | 64 | `PAN` |
| `ampGain` | CHARACTER | 15 | 0–127 | 100 | `AMP_GAIN` |
| `ampExpnt` | CHARACTER | 36 | 0–127 | 0 | `EG_AMP_MOD` |
| `portaMode` | CHARACTER | 18 | 0–127 | 0 | `VOICE_MODE` |
| `voiceAsgnMode` | CHARACTER | 110 | 0–127 | 0 | `VOICE_ASGN_MODE` |
| `bthAmpMod` | CHARACTER | 61 | 0–127 | 0 | `BTH_AMP_MOD` |
| `egVelSens` | CHARACTER | 62 | 0–127 | 0 | `EG_VEL_SENS` |
| `ampVelSens` | CHARACTER | 63 | 0–127 | 0 | `AMP_VEL_SENS` |
| `aftTlfoAmt` | CHARACTER | 109 | 0–127 | 0 | `AFT_T_LFO_AMT` |

Performance inputs Note On/Off, pitch bend, pressure, velocity, breath (CC2) and
sustain (CC64) are MIDI events rather than separate APVTS parameters; their routing
amounts/ranges are represented above. CC112–119 select canonical factory columns.
Embedded program-write controls CC87/106 and unsupported SysEx are ignored by the
desktop wrapper. Factory presets remain read-only and use all 52 canonical rows.
