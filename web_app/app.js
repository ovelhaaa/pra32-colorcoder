let audioContext;
let synthNode;
let isAudioInitialized = false;

const startBtn = document.getElementById('start-btn');
const startOverlay = document.getElementById('start-overlay');
const statusDiv = document.getElementById('status-text');

// Helper to fetch the wasm binary
async function loadWasm() {
    const response = await fetch('synth.wasm');
    const buffer = await response.arrayBuffer();
    return buffer;
}

startBtn.addEventListener('click', async () => {
    if (isAudioInitialized) return;

    try {
        startBtn.textContent = 'INITIALIZING...';
        startBtn.style.borderColor = '#555';
        startBtn.style.color = '#555';
        startBtn.style.cursor = 'default';

        audioContext = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: 48000
        });

        await audioContext.audioWorklet.addModule('synth-processor.js');

        synthNode = new AudioWorkletNode(audioContext, 'synth-processor', {
            outputChannelCount: [2]
        });

        synthNode.connect(audioContext.destination);

        const wasmBytes = await loadWasm();

        // Wait for processor to say it's loaded
        synthNode.port.onmessage = async (e) => {
            if (e.data.type === 'wasmLoaded') {
                isAudioInitialized = true;
                statusDiv.textContent = 'ONLINE';
                statusDiv.classList.add('active');
                const dot = document.getElementById('status-dot');
                if (dot) dot.classList.add('active');

                startBtn.textContent = '◼ AUDIO ENGINE ACTIVE';
                startBtn.disabled = true;

                startOverlay.style.display = 'none';

                setupMidi();
                const presetsLoaded = await loadPresets();
                setupControls(presetsLoaded);
            }
        };

        // Send WASM to the worklet
        synthNode.port.postMessage({
            type: 'loadWasm',
            wasmBytes: wasmBytes
        });

    } catch (err) {
        console.error(err);
        statusDiv.textContent = 'Error: ' + err.message;
        startBtn.textContent = 'RETRY';
    }
});

// --- MIDI AND CONTROLS ---

function sendCC(cc, value) {
    if (synthNode) {
        synthNode.port.postMessage({ type: 'controlChange', cc, value });
    }
}

function sendNoteOn(note, velocity = 100) {
    if (synthNode) {
        synthNode.port.postMessage({ type: 'noteOn', note, velocity });
    }
}

function sendNoteOff(note) {
    if (synthNode) {
        synthNode.port.postMessage({ type: 'noteOff', note });
    }
}

let factoryPresets = null;

async function loadPresets() {
    try {
        const response = await fetch('data/presets.json');
        const data = await response.json();

        // Store both "current" values (index 0) and factory preset bank (index 1).
        factoryPresets = {};
        for (const key in data) {
            if (!key.startsWith('_')) {
                const paramName = key.trim();
                const currentValues = Array.isArray(data[key]?.[0]) ? data[key][0] : [];
                const presetBank = Array.isArray(data[key]?.[1]) ? data[key][1] : [];
                factoryPresets[paramName] = {
                    current: currentValues,
                    presets: presetBank,
                };
            }
        }

        return true;
    } catch (e) {
        console.error('Failed to load presets', e);
        return false;
    }
}

const synthParameters = [
    // OSC Tab
    { id: 'osc1Wave', label: 'OSC 1 WAVE', cc: 24, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },
    { id: 'osc1Shape', label: 'OSC 1 SHAPE', cc: 70, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },
    { id: 'osc1Morph', label: 'OSC 1 MORPH', cc: 104, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },
    { id: 'osc2Wave', label: 'OSC 2 WAVE', cc: 25, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },
    { id: 'osc2Coarse', label: 'OSC 2 COARSE', cc: 20, min: 0, max: 127, val: 64, tab: 'OSC', type: 'h' },
    { id: 'osc2Pitch', label: 'OSC 2 PITCH', cc: 21, min: 0, max: 127, val: 64, tab: 'OSC', type: 'h' },
    { id: 'oscMix', label: 'OSC MIX', cc: 22, min: 0, max: 127, val: 64, tab: 'OSC', type: 'v' },
    { id: 'subOsc', label: 'SUB OSC', cc: 23, min: 0, max: 127, val: 0, tab: 'OSC', type: 'v' },
    { id: 'oscDrift', label: 'OSC DRIFT', cc: 26, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },
    { id: 'sawWMode', label: 'SAW W MODE', cc: 27, min: 0, max: 127, val: 0, tab: 'OSC', type: 'h' },

    // FILTER Tab
    { id: 'filterCutoff', label: 'CUTOFF', cc: 74, min: 0, max: 127, val: 127, tab: 'FILTER', type: 'h' },
    { id: 'filterReso', label: 'RESO', cc: 71, min: 0, max: 127, val: 0, tab: 'FILTER', type: 'h' },
    { id: 'filterMode', label: 'MODE', cc: 28, min: 0, max: 127, val: 0, tab: 'FILTER', type: 'h' },
    { id: 'egFltAmt', label: 'EG -> FLT', cc: 29, min: 0, max: 127, val: 64, tab: 'FILTER', type: 'h' },
    { id: 'filterKeyTrk', label: 'KEY TRK', cc: 30, min: 0, max: 127, val: 64, tab: 'FILTER', type: 'h' },
    { id: 'bthFltAmt', label: 'BTH -> FLT', cc: 31, min: 0, max: 127, val: 64, tab: 'FILTER', type: 'h' },
    { id: 'relEqDcy', label: 'REL EQ DCY', cc: 88, min: 0, max: 127, val: 0, tab: 'FILTER', type: 'h' },

    // ENVS Tab
    { id: 'egOscAmt', label: 'EG -> OSC', cc: 105, min: 0, max: 127, val: 64, tab: 'ENVS', type: 'h' },
    { id: 'egAttack', label: 'EG ATTACK', cc: 73, min: 0, max: 127, val: 0, tab: 'ENVS', type: 'v' },
    { id: 'egDecay', label: 'EG DECAY', cc: 75, min: 0, max: 127, val: 64, tab: 'ENVS', type: 'v' },
    { id: 'egSustain', label: 'EG SUSTAIN', cc: 33, min: 0, max: 127, val: 127, tab: 'ENVS', type: 'v' },
    { id: 'egRelease', label: 'EG RELEASE', cc: 72, min: 0, max: 127, val: 64, tab: 'ENVS', type: 'v' },
    { id: 'ampAttack', label: 'AMP ATTACK', cc: 106, min: 0, max: 127, val: 0, tab: 'ENVS', type: 'v' },
    { id: 'ampDecay', label: 'AMP DECAY', cc: 107, min: 0, max: 127, val: 64, tab: 'ENVS', type: 'v' },
    { id: 'ampSustain', label: 'AMP SUSTAIN', cc: 108, min: 0, max: 127, val: 127, tab: 'ENVS', type: 'v' },
    { id: 'ampRelease', label: 'AMP RELEASE', cc: 109, min: 0, max: 127, val: 64, tab: 'ENVS', type: 'v' },

    // MOD Tab
    { id: 'lfoWave', label: 'LFO WAVE', cc: 14, min: 0, max: 127, val: 0, tab: 'MOD', type: 'h' },
    { id: 'lfoRate', label: 'LFO RATE', cc: 76, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'lfoFltAmt', label: 'LFO -> FLT', cc: 15, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'lfoOscAmt', label: 'LFO -> OSC', cc: 16, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'lfoFadeTime', label: 'LFO FADE', cc: 17, min: 0, max: 127, val: 0, tab: 'MOD', type: 'h' },
    { id: 'pbRange', label: 'PB RANGE', cc: 85, min: 0, max: 127, val: 2, tab: 'MOD', type: 'h' },
    { id: 'coarseTune', label: 'COARSE TUNE', cc: 86, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'fineTune', label: 'FINE TUNE', cc: 87, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'portaTime', label: 'PORTA TIME', cc: 5, min: 0, max: 127, val: 0, tab: 'MOD', type: 'h' },
    { id: 'modRate', label: 'MOD RATE', cc: 110, min: 0, max: 127, val: 64, tab: 'MOD', type: 'h' },
    { id: 'modDepth', label: 'MOD DEPTH', cc: 111, min: 0, max: 127, val: 0, tab: 'MOD', type: 'h' },

    // FX Tab
    { id: 'choRate', label: 'CHO RATE', cc: 93, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },
    { id: 'choDepth', label: 'CHO DEPTH', cc: 114, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },
    { id: 'delayTime', label: 'DLY TIME', cc: 112, min: 0, max: 127, val: 64, tab: 'FX', type: 'h' },
    { id: 'delayDepth', label: 'DLY DEPTH', cc: 113, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },
    { id: 'pan', label: 'PAN', cc: 10, min: 0, max: 127, val: 64, tab: 'FX', type: 'h' },
    { id: 'volume', label: 'VOLUME', cc: 7, min: 0, max: 127, val: 100, tab: 'FX', type: 'v' },
    { id: 'ampExpnt', label: 'AMP EXP', cc: 116, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },
    { id: 'ampGain', label: 'AMP GAIN', cc: 117, min: 0, max: 127, val: 64, tab: 'FX', type: 'h' },
    { id: 'pannerType', label: 'PAN TYPE', cc: 118, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },
    { id: 'choType', label: 'CHO TYPE', cc: 119, min: 0, max: 127, val: 0, tab: 'FX', type: 'h' },

    // LO-FI Tab
    { id: 'lfoWaveForm', label: 'LFO FORM', cc: 89, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'portaMode', label: 'PRTA MODE', cc: 90, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'pwmRate', label: 'PWM RATE', cc: 115, min: 0, max: 127, val: 64, tab: 'LO-FI', type: 'h' },
    { id: 'lfoSync', label: 'LFO SYNC', cc: 120, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'noiseMode', label: 'NOISE MODE', cc: 121, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'distMode', label: 'DIST MODE', cc: 122, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'bitCrush', label: 'BITCRUSH', cc: 123, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' },
    { id: 'vcfType', label: 'VCF TYPE', cc: 124, min: 0, max: 127, val: 0, tab: 'LO-FI', type: 'h' }
];

const ccToParam = new Map(synthParameters.map(p => [p.cc, p]));

function setupControls(presetsLoaded) {
    const tabsContainer = document.getElementById('tabsContainer');
    const tabContentsContainer = document.getElementById('tabContentsContainer');

    const idToPresetKey = {
        osc1Wave: 'OSC_1_WAVE',
        osc1Shape: 'OSC_1_SHAPE',
        osc1Morph: 'OSC_1_MORPH',
        osc2Wave: 'OSC_2_WAVE',
        osc2Coarse: 'OSC_2_COARSE',
        osc2Pitch: 'OSC_2_PITCH',
        oscMix: 'MIXER_OSC_MIX',
        subOsc: 'MIXER_SUB_OSC',
        oscDrift: 'OSC_DRIFT',
        sawWMode: 'OSC_SAW_W_MODE',
        filterCutoff: 'FILTER_CUTOFF',
        filterReso: 'FILTER_RESO',
        filterMode: 'FILTER_MODE',
        egFltAmt: 'FILTER_EG_AMT',
        filterKeyTrk: 'FILTER_KEY_TRK',
        bthFltAmt: 'BTH_FILTER_AMT',
        relEqDcy: 'REL_EQ_DECAY',
        egOscAmt: 'EG_OSC_AMT',
        egAttack: 'EG_ATTACK',
        egDecay: 'EG_DECAY',
        egSustain: 'EG_SUSTAIN',
        egRelease: 'EG_RELEASE',
        ampAttack: 'AMP_ATTACK',
        ampDecay: 'AMP_DECAY',
        ampSustain: 'AMP_SUSTAIN',
        ampRelease: 'AMP_RELEASE',
        lfoWave: 'LFO_WAVE',
        lfoRate: 'LFO_RATE',
        lfoFltAmt: 'LFO_FILTER_AMT',
        lfoOscAmt: 'LFO_OSC_AMT',
        lfoFadeTime: 'LFO_FADE_TIME',
        pbRange: 'P_BEND_RANGE',
        choRate: 'CHORUS_RATE',
        choDepth: 'CHORUS_DEPTH',
        delayTime: 'DELAY_TIME',
        delayDepth: 'DELAY_LEVEL',
        pan: 'PAN',
        ampGain: 'AMP_GAIN',
        portaTime: 'PORTAMENTO'
    };

    tabsContainer.innerHTML = '';
    tabContentsContainer.innerHTML = '';

    // Group parameters by tab
    const tabsMap = new Map();
    synthParameters.forEach(param => {
        if (!tabsMap.has(param.tab)) {
            tabsMap.set(param.tab, []);
        }
        tabsMap.get(param.tab).push(param);
    });

    let firstTab = true;

    const mobileMediaQuery = window.matchMedia('(max-width: 900px)');

    tabsMap.forEach((params, tabName) => {
        // Create Tab Button
        const tabBtn = document.createElement('div');
        tabBtn.className = `pra-tab-btn ${firstTab ? 'active' : ''}`;
        tabBtn.textContent = tabName;
        tabBtn.dataset.tabTarget = tabName;

        // Create Panel
        const tabContent = document.createElement('div');
        // If not mobile, all panels should be active/visible initially
        const isPanelActive = !mobileMediaQuery.matches || firstTab;
        tabContent.className = `pra-panel ${isPanelActive ? 'active' : ''}`;
        tabContent.id = `tab-${tabName}`;

        const panelHeader = document.createElement('div');
        panelHeader.className = 'pra-panel-header';
        panelHeader.textContent = tabName;
        tabContent.appendChild(panelHeader);

        const panelControls = document.createElement('div');
        panelControls.className = 'pra-panel-controls';
        tabContent.appendChild(panelControls);

        tabBtn.addEventListener('click', () => {
            if (mobileMediaQuery.matches) {
                document.querySelectorAll('.pra-tab-btn').forEach(btn => btn.classList.remove('active'));
                document.querySelectorAll('.pra-panel').forEach(content => content.classList.remove('active'));
                tabBtn.classList.add('active');
                tabContent.classList.add('active');
            }
        });

        tabsContainer.appendChild(tabBtn);

        // Populate Tab Content
        params.forEach(param => {
            if (param.type === 'h') {
                // Render as knob
                const g = document.createElement('div');
                g.className = 'pra-knob-group';
                g.dataset.id = param.id;
                const size = 52;

                const fmtLabel = param.label.replace(' ', '\n');

                g.innerHTML = `<div class="knob-svg-wrap">${buildKnobSVG(param.val, param.min, param.max, size)}</div>
                  <div class="pra-knob-label">${fmtLabel}</div>
                  <div class="pra-knob-value" id="kv-${param.id}">${param.val}</div>`;

                let dragging = false, startY = 0, startVal = 0;

                const updateKnobVisuals = (newVal) => {
                    param.val = newVal;
                    g.querySelector('.knob-svg-wrap').innerHTML = buildKnobSVG(newVal, param.min, param.max, size);
                    document.getElementById('kv-' + param.id).textContent = newVal;
                    sendCC(param.cc, newVal);
                };

                const onMove = e => {
                  if (!dragging) return;
                  const clientY = e.touches ? e.touches[0].clientY : e.clientY;
                  const dy = startY - clientY;
                  const range = param.max - param.min;
                  const newVal = Math.min(param.max, Math.max(param.min, Math.round(startVal + dy * range / 100)));
                  updateKnobVisuals(newVal);
                };

                const onEnd = () => { dragging = false; g.classList.remove('active'); };

                g.addEventListener('mousedown', e => { dragging = true; startY = e.clientY; startVal = param.val; g.classList.add('active'); });
                g.addEventListener('touchstart', e => { dragging = true; startY = e.touches[0].clientY; startVal = param.val; g.classList.add('active'); e.preventDefault(); }, { passive: false });

                window.addEventListener('mousemove', onMove);
                window.addEventListener('touchmove', onMove, { passive: false });
                window.addEventListener('mouseup', onEnd);
                window.addEventListener('touchend', onEnd);

                panelControls.appendChild(g);
                sendCC(param.cc, param.val);
            } else {
                // Render as slider
                const row = document.createElement('div');
                row.className = 'pra-slider-row';
                row.innerHTML = `<span class="pra-slider-name">${param.label}</span>
                  <input type="range" class="pra-slider" min="${param.min}" max="${param.max}" value="${param.val}" step="1" id="sl-${param.id}">
                  <span class="pra-slider-val" id="slv-${param.id}">${param.val}</span>`;

                const inp = row.querySelector('input');
                const valDisp = row.querySelector(`#slv-${param.id}`);

                inp.addEventListener('input', () => {
                    const newVal = parseInt(inp.value);
                    valDisp.textContent = newVal;
                    param.val = newVal;
                    sendCC(param.cc, newVal);
                });

                panelControls.appendChild(row);
                sendCC(param.cc, param.val);
            }
        });

        tabContentsContainer.appendChild(tabContent);
        firstTab = false;
    });

    // CHAOS Button
    const chaosBtn = document.getElementById('circuitBendBtn');
    if (chaosBtn) {
        chaosBtn.addEventListener('click', () => {
            synthParameters.forEach(param => {
                const randomVal = Math.floor(Math.random() * (param.max - param.min + 1)) + param.min;
                param.val = randomVal;
                updateSlider(param.id, randomVal);
                sendCC(param.cc, randomVal);
            });
            // Update preset select to custom
            const presetSelect = document.getElementById('presetSelect');
            if (presetSelect) presetSelect.value = "-1";
        });
    }

    // Preset selection
    const presetSelect = document.getElementById('presetSelect');
    if (presetSelect && presetsLoaded && factoryPresets) {
        // Find how many presets exist by checking an arbitrary preset array
        const sampleParamData = Object.values(factoryPresets)[0];
        if (sampleParamData && sampleParamData.presets) {
            const numPresets = sampleParamData.presets.length;

            const presetNames = [
                "00 · Initialization", "01 · Sync Lead", "02 · Synth Brass", "03 · Pluck Synth",
                "04 · Mono Synth", "05 · Synth Bass 1", "06 · Synth Bass 2", "07 · Synth Bass 3",
                "08 · Ethereal Pad", "09 · Gritty Bass", "10 · Chiptune Lead", "11 · Percussive Pluck",
                "12 · Classic Sweep", "13 · Dark Drone", "14 · Noise Percussion", "15 · Bell Lead"
            ];

            for (let i = 0; i < numPresets; i++) {
                const option = document.createElement('option');
                option.value = i;
                option.textContent = presetNames[i] || `Preset ${i + 1}`;
                presetSelect.appendChild(option);
            }
        }

        presetSelect.addEventListener('change', (e) => {
            const presetIndex = parseInt(e.target.value);

            if (presetIndex < 0) {
                // Restore to current/init value from presets.json first array.
                synthParameters.forEach(param => {
                    const paramKeyName = idToPresetKey[param.id] || param.label.replace(/ /g, '_');
                    const presetData = factoryPresets[paramKeyName];
                    if (presetData && presetData.current[0] !== undefined) {
                        const newValue = presetData.current[0];
                        param.val = newValue;
                        updateSlider(param.id, newValue);
                        sendCC(param.cc, newValue);
                    }
                });
                return;
            }

            synthParameters.forEach(param => {
                const paramKeyName = idToPresetKey[param.id] || param.label.replace(/ /g, '_');
                const presetData = factoryPresets[paramKeyName];
                if (presetData && presetData.presets[presetIndex] !== undefined) {
                    const newValue = presetData.presets[presetIndex];
                    param.val = newValue;
                    updateSlider(param.id, newValue);
                    sendCC(param.cc, newValue);
                }
            });
        });
    }

    // Keyboard
    let isMobileLayout = mobileMediaQuery.matches;
    let octaveOffset = 0;
    const activeNotes = {};
    const octaveMin = -2;
    const octaveMax = 3;
    const baseNote = 60; // C4
    const keyboardDiv = document.getElementById('keyboard');
    const keyPattern = ['white', 'black', 'white', 'black', 'white', 'white', 'black', 'white', 'black', 'white', 'black', 'white'];
    const desktopKeys = ['a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k'];
    let keys = [];

    function getNoteValue(noteOffset) {
        return baseNote + noteOffset + (octaveOffset * 12);
    }

    const activePointerNotes = new Map();

    const octaveControls = document.getElementById('octave-controls');
    const octaveDisplay = document.getElementById('octave-display');
    const octaveDownBtn = document.getElementById('octave-down');
    const octaveUpBtn = document.getElementById('octave-up');
    const updateOctaveDisplay = () => {
        octaveDisplay.textContent = `C${4 + octaveOffset}`;
        octaveDownBtn.disabled = octaveOffset <= octaveMin;
        octaveUpBtn.disabled = octaveOffset >= octaveMax;
    };

    const refreshKeyNotes = () => {
            document.querySelectorAll('.pra-key').forEach((el) => {
                el.classList.remove('active');
                el.classList.remove('pressed');
                if (el.dataset.noteOffset) {
                    const noteOffset = parseInt(el.dataset.noteOffset, 10);
                    if (activePointerNotes.has(el)) {
                        sendNoteOff(activePointerNotes.get(el));
                        activePointerNotes.delete(el);
                    }
                    delete activeNotes[noteOffset];
                    el.dataset.activeNote = '';
                    el.dataset.note = getNoteValue(noteOffset);
                }
            });
            updateOctaveDisplay();
    };

    const updateOctaveControlsVisibility = () => {
        if (!octaveControls) return;
        // octave controls can always be visible in new design
        octaveControls.style.display = 'flex';
    };

    const buildKeyboard = () => {
        keyboardDiv.innerHTML = '';
        keyboardDiv.style.position = 'relative';
        const handleRelease = (e) => {
            e.preventDefault();
            const keyEl = e.currentTarget;
            const activeNote = parseInt(keyEl.dataset.activeNote, 10);
            if (!Number.isNaN(activeNote)) {
                sendNoteOff(activeNote);
                activePointerNotes.delete(keyEl);
                keyEl.dataset.activeNote = '';
                keyEl.classList.remove('pressed');
                keyEl.classList.remove('active');
            }
        };

        const WHITE_NOTES = [0,2,4,5,7,9,11];
        const BLACK_OFFSETS = { 1: 0.6, 3: 1.6, 6: 3.6, 8: 4.6, 10: 5.6 };

        const isMobile = isMobileLayout;
        const OCTAVES = isMobile ? 2 : 4;

        const keyW = isMobile ? 32 : 40;
        const blackW = isMobile ? 20 : 24;
        const whiteH = 80;
        const blackH = 50;

        const totalWhites = WHITE_NOTES.length * OCTAVES;
        keyboardDiv.style.width = (totalWhites * keyW) + 'px';
        keyboardDiv.style.height = whiteH + 'px';
        keyboardDiv.style.margin = '0 auto';

        for (let oct = 0; oct < OCTAVES; oct++) {
            // White keys
            WHITE_NOTES.forEach((noteInOct, wi) => {
                const noteOffset = oct * 12 + noteInOct;
                const k = document.createElement('div');
                k.className = 'pra-key white';
                k.style.cssText = `position:absolute;left:${(oct*7+wi)*keyW}px;top:0;width:${keyW-1}px;height:${whiteH}px;`;

                k.dataset.noteOffset = noteOffset;
                k.dataset.note = getNoteValue(noteOffset);
                k.dataset.activeNote = '';

                const handleRelease = (e) => {
                    e.preventDefault();
                    const activeNote = parseInt(k.dataset.activeNote, 10);
                    if (!Number.isNaN(activeNote)) {
                        sendNoteOff(activeNote);
                        activePointerNotes.delete(k);
                        k.dataset.activeNote = '';
                        k.classList.remove('pressed');
                        k.classList.remove('active');
                    }
                };

                k.addEventListener('pointerdown', (e) => {
                    e.preventDefault();
                    const activeNote = getNoteValue(noteOffset);
                    k.dataset.activeNote = `${activeNote}`;
                    activePointerNotes.set(k, activeNote);
                    sendNoteOn(activeNote);
                    k.classList.add('pressed');
                    k.classList.add('active');
                });
                k.addEventListener('pointerup', handleRelease);
                k.addEventListener('pointercancel', handleRelease);
                k.addEventListener('pointerleave', handleRelease);

                keyboardDiv.appendChild(k);
            });

            // Black keys
            Object.entries(BLACK_OFFSETS).forEach(([noteInOctStr, offset]) => {
                const noteInOct = parseInt(noteInOctStr);
                const noteOffset = oct * 12 + noteInOct;
                const k = document.createElement('div');
                k.className = 'pra-key black';
                const leftPx = (oct * 7 + offset) * keyW + keyW/2 - blackW/2;
                k.style.cssText = `position:absolute;left:${leftPx}px;top:0;width:${blackW}px;height:${blackH}px;`;

                k.dataset.noteOffset = noteOffset;
                k.dataset.note = getNoteValue(noteOffset);
                k.dataset.activeNote = '';

                const handleRelease = (e) => {
                    e.preventDefault();
                    const activeNote = parseInt(k.dataset.activeNote, 10);
                    if (!Number.isNaN(activeNote)) {
                        sendNoteOff(activeNote);
                        activePointerNotes.delete(k);
                        k.dataset.activeNote = '';
                        k.classList.remove('pressed');
                        k.classList.remove('active');
                    }
                };

                k.addEventListener('pointerdown', (e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    const activeNote = getNoteValue(noteOffset);
                    k.dataset.activeNote = `${activeNote}`;
                    activePointerNotes.set(k, activeNote);
                    sendNoteOn(activeNote);
                    k.classList.add('pressed');
                    k.classList.add('active');
                });
                k.addEventListener('pointerup', handleRelease);
                k.addEventListener('pointercancel', handleRelease);
                k.addEventListener('pointerleave', handleRelease);

                keyboardDiv.appendChild(k);
            });
        }
    };

    buildKeyboard();
    updateOctaveControlsVisibility();

    if (octaveControls && octaveDisplay && octaveDownBtn && octaveUpBtn) {
        updateOctaveDisplay();

        octaveDownBtn.addEventListener('click', () => {
            if (octaveOffset > octaveMin) {
                octaveOffset -= 1;
                refreshKeyNotes();
            }
        });

        octaveUpBtn.addEventListener('click', () => {
            if (octaveOffset < octaveMax) {
                octaveOffset += 1;
                refreshKeyNotes();
            }
        });
    }

    // PC Keyboard mapping
    const keyMap = {};
    desktopKeys.forEach((key, index) => {
        keyMap[key] = index;
    });

    const activeNotes = {};
    mobileMediaQuery.addEventListener('change', (e) => {
        isMobileLayout = e.matches;

        // Reset panels to default desktop state if switching to desktop
        if (!isMobileLayout) {
            document.querySelectorAll('.pra-panel').forEach(content => content.classList.add('active'));
        } else {
            // Restore tab selection for mobile
            const activeTab = document.querySelector('.pra-tab-btn.active');
            if (activeTab) {
                document.querySelectorAll('.pra-panel').forEach(content => content.classList.remove('active'));
                const targetPanel = document.getElementById(`tab-${activeTab.dataset.tabTarget}`);
                if (targetPanel) targetPanel.classList.add('active');
            }
        }

        refreshKeyNotes();
        buildKeyboard();
        updateOctaveControlsVisibility();
        updateOctaveDisplay();
    });

    window.addEventListener('keydown', (e) => {
        if (e.repeat) return;
        const noteOffset = keyMap[e.key];
        if (noteOffset !== undefined) {
            const note = getNoteValue(noteOffset);
            sendNoteOn(note);
            activeNotes[noteOffset] = note;
            const el = document.querySelector(`.pra-key[data-note-offset="${noteOffset}"]`);
            if (el) {
                el.classList.add('active');
                el.classList.add('pressed');
            }
        }
    });

    window.addEventListener('keyup', (e) => {
        const noteOffset = keyMap[e.key];
        const activeNote = activeNotes[noteOffset];
        if (noteOffset !== undefined && activeNote !== undefined) {
            sendNoteOff(activeNote);
            delete activeNotes[noteOffset];
            const el = document.querySelector(`.pra-key[data-note-offset="${noteOffset}"]`);
            if (el) {
                el.classList.remove('active');
                el.classList.remove('pressed');
            }
        }
    });
}

// --- WEB MIDI API ---

function setupMidi() {
    if (navigator.requestMIDIAccess) {
        navigator.requestMIDIAccess().then(onMIDISuccess, onMIDIFailure);
    } else {
        statusDiv.textContent += ' | Web MIDI API not supported';
    }
}

function onMIDISuccess(midiAccess) {
    const inputs = midiAccess.inputs.values();
    for (let input = inputs.next(); input && !input.done; input = inputs.next()) {
        input.value.onmidimessage = onMIDIMessage;
    }
    midiAccess.onstatechange = (e) => {
        if (e.port.state === 'connected' && e.port.type === 'input') {
            e.port.onmidimessage = onMIDIMessage;
        }
    };
}

function onMIDIFailure() {
    console.log('Could not access your MIDI devices.');
}

function onMIDIMessage(message) {
    const command = message.data[0] >> 4;
    const channel = message.data[0] & 0xf;
    const data1 = message.data[1];
    const data2 = message.data[2];

    if (command === 9) { // Note On
        if (data2 > 0) {
            sendNoteOn(data1, data2);
            highlightKey(data1, true);
        } else {
            sendNoteOff(data1);
            highlightKey(data1, false);
        }
    } else if (command === 8) { // Note Off
        sendNoteOff(data1);
        highlightKey(data1, false);
    } else if (command === 11) { // Control Change
        sendCC(data1, data2);

        // Update UI if we have a matching control
        const param = ccToParam.get(data1);
        if (param) {
            updateSlider(param.id, data2);
        }
    }
}

function updateSlider(id, value) {
    // If it's a DOM event we already updated the visual state,
    // but this function handles external MIDI or CHAOS/preset changes
    const slider = document.getElementById(`sl-${id}`);
    if (slider) {
        slider.value = value;
        const valDisplay = document.getElementById(`slv-${id}`);
        if (valDisplay) valDisplay.textContent = value;
        return;
    }

    // Check if it's a knob
    const kv = document.getElementById(`kv-${id}`);
    if (kv) {
        kv.textContent = value;
        const g = kv.closest('.pra-knob-group');
        if (g) {
            const def = synthParameters.find(p => p.id === id);
            if (def) {
                def.val = value;
                const wrap = g.querySelector('.knob-svg-wrap');
                if (wrap) {
                    wrap.innerHTML = buildKnobSVG(value, def.min, def.max, 52);
                }
            }
        }
    }
}

function buildKnobSVG(val, min, max, size) {
  const norm = (val - min) / (max - min);
  const startAngle = -220 * Math.PI / 180;
  const endAngle = 40 * Math.PI / 180;
  const angle = startAngle + norm * (endAngle - startAngle);
  const cx = size / 2, cy = size / 2, r = size / 2 - 5;
  const trackA1x = cx + r * Math.cos(startAngle);
  const trackA1y = cy + r * Math.sin(startAngle);
  const trackA2x = cx + r * Math.cos(endAngle);
  const trackA2y = cy + r * Math.sin(endAngle);
  const px = cx + r * Math.cos(angle);
  const py = cy + r * Math.sin(angle);
  const arcStartX = cx + r * Math.cos(startAngle);
  const arcStartY = cy + r * Math.sin(startAngle);
  const largeArc = norm > 0.72 ? 1 : 0;
  return `<svg width="${size}" height="${size}" viewBox="0 0 ${size} ${size}" class="pra-knob-svg">
    <circle cx="${cx}" cy="${cy}" r="${r}" fill="#1A1A1A" stroke="#222" stroke-width="1"/>
    <path d="M ${trackA1x} ${trackA1y} A ${r} ${r} 0 1 1 ${trackA2x} ${trackA2y}" fill="none" stroke="#252525" stroke-width="3" stroke-linecap="round"/>
    <path d="M ${arcStartX} ${arcStartY} A ${r} ${r} 0 ${largeArc} 1 ${px} ${py}" fill="none" stroke="#E8A020" stroke-width="3" stroke-linecap="round"/>
    <circle cx="${px}" cy="${py}" r="2.5" fill="#E8A020"/>
    <circle cx="${cx}" cy="${cy}" r="${r * 0.35}" fill="#222"/>
  </svg>`;
}

function highlightKey(note, active) {
    const el = document.querySelector(`.pra-key[data-note="${note}"]`);
    if (el) {
        if (active) {
            el.classList.add('active');
            el.classList.add('pressed');
        } else {
            el.classList.remove('active');
            el.classList.remove('pressed');
        }
    }
}
