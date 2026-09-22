#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PRA32ValueFormatter.h"

using namespace PRA32Theme;

namespace {

const char* const kSections[6] = { "OSC", "FILTER", "ENVS", "MOD", "FX", "CHARACTER" };

const juce::Identifier uiSectionProperty  { "uiSection" };
const juce::Identifier uiKeyboardProperty { "uiKeyboard" };
const juce::Identifier uiOctaveProperty   { "uiOctave" };

} // namespace

//==============================================================================
PRA32ParameterPage::PRA32ParameterPage (PRA32ColorcoderAudioProcessor& p,
                                        juce::String sectionName)
    : processor (p), section (std::move (sectionName))
{
    if (section == "OSC")              buildOsc();
    else if (section == "FILTER")      buildFilter();
    else if (section == "ENVS")        buildEnvs();
    else if (section == "MOD")         buildMod();
    else if (section == "FX")          buildFx();
    else if (section == "CHARACTER"
          || section == "COLOR")       buildCharacter();
    else                               buildGeneric();
}

//==============================================================================
ModulePanel* PRA32ParameterPage::addModule (const juce::String& title)
{
    auto* panel = panels.add (new ModulePanel (title, {}, sectionAccent (section)));
    moduleById[title] = panel;
    addAndMakeVisible (panel);
    return panel;
}

juce::Component* PRA32ParameterPage::addParam (const juce::String& id)
{
    const auto* info = SynthParameters::find (id);

    if (info == nullptr)
        return nullptr;

    auto& apvts = processor.getAPVTS();
    juce::Component* control = nullptr;

    switch (info->kind)
    {
        case PRA32ControlKind::SteppedSelector:
            control = controls.add (new SteppedSelector (*info, apvts));
            break;

        case PRA32ControlKind::Toggle:
            control = controls.add (new ToggleSwitch (*info, apvts));
            break;

        default:
            control = controls.add (new ParameterKnob (*info, apvts));
            break;
    }

    controlById[id] = control;
    return control;
}

void PRA32ParameterPage::addToPanel (ModulePanel& panel, const juce::String& id)
{
    if (const auto* info = SynthParameters::find (id))
        if (auto* control = addParam (id))
            panel.addControl (*control, info->visualWeight);
}

void PRA32ParameterPage::addToPanel (ModulePanel& panel,
                                     std::initializer_list<const char*> ids)
{
    for (auto* id : ids)
        addToPanel (panel, id);
}

juce::Component* PRA32ParameterPage::find (const juce::String& id) const
{
    auto it = controlById.find (id);
    return it == controlById.end() ? nullptr : it->second;
}

static ModulePanel* pageModule (const std::map<juce::String, ModulePanel*>& modules,
                                const juce::String& name)
{
    auto it = modules.find (name);
    return it == modules.end() ? nullptr : it->second;
}

//==============================================================================
void PRA32ParameterPage::buildGeneric()
{
    std::map<juce::String, ModulePanel*> modules;
    std::map<juce::String, int> counts;

    for (const auto& p : SynthParameters::getParameters())
    {
        if (p.section != section)
            continue;

        auto it = modules.find (p.subsection);

        if (it == modules.end())
        {
            auto* panel = addModule (p.subsection);
            modules[p.subsection] = panel;
            it = modules.find (p.subsection);
        }

        addToPanel (*it->second, p.id);
        counts[p.subsection]++;
    }

    for (auto& entry : modules)
        entry.second->setPreferredColumns (juce::jlimit (2, 6, counts[entry.first]));
}

void PRA32ParameterPage::buildOsc()
{
    auto& osc1 = *addModule ("OSCILLATOR 1");
    osc1.setAutoLayout (false);
    addToPanel (osc1, { "osc1Wave", "sawWMode", "osc1Shape", "osc1Morph", "oscDrift" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("osc1Wave")))
        selector->setGridColumns (3);
    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("sawWMode")))
        selector->setGridColumns (1);

    auto& osc2 = *addModule ("OSCILLATOR 2");
    osc2.setAutoLayout (false);
    addToPanel (osc2, { "osc2Wave", "osc2Coarse", "osc2Pitch" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("osc2Wave")))
        selector->setGridColumns (3);

    auto& mixer = *addModule ("MIXER");
    mixer.setAutoLayout (false);
    addToPanel (mixer, { "oscMix", "subOsc" });
}

void PRA32ParameterPage::buildFilter()
{
    auto& filter = *addModule ("FILTER NETWORK");
    filter.setAutoLayout (false);
    addParam ("filterCutoff");
    addParam ("filterReso");

    auto* graph = graphs.add (new FilterResponseGraph (processor.getAPVTS(),
                                                       sectionAccent (section)));
    filter.addControl (*graph);

    auto& shaping = *addModule ("FILTER SHAPING");
    shaping.setAutoLayout (false);
    addToPanel (shaping, { "filterMode", "egFltAmt", "filterKeyTrk", "bthFltAmt", "relEqDcy" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("filterMode")))
        selector->setGridColumns (1);
}

void PRA32ParameterPage::buildEnvs()
{
    auto makeEnvelope = [this] (const juce::String& title, const char* a, const char* d,
                                const char* s, const char* r)
    {
        auto& module = *addModule (title);
        module.setAutoLayout (false);

        auto* env = envelopes.add (new EnvelopePanel (processor.getAPVTS(),
                                                      *SynthParameters::find (a),
                                                      *SynthParameters::find (d),
                                                      *SynthParameters::find (s),
                                                      *SynthParameters::find (r),
                                                      sectionAccent (section)));
        module.addControl (*env);
    };

    makeEnvelope ("MOD ENVELOPE", "egAttack", "egDecay", "egSustain", "egRelease");
    makeEnvelope ("AMP ENVELOPE", "ampAttack", "ampDecay", "ampSustain", "ampRelease");
}

void PRA32ParameterPage::buildMod()
{
    auto& lfo = *addModule ("MODULATION LFO");
    lfo.setAutoLayout (false);
    addToPanel (lfo, { "lfoWave", "lfoRate", "lfoFadeTime", "lfoDepth",
                       "lfoOscDst", "lfoFltAmt", "lfoOscAmt" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("lfoWave")))
        selector->setGridColumns (3);

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("lfoOscDst")))
        selector->setGridColumns (3);
}

void PRA32ParameterPage::buildFx()
{
    auto& chorus = *addModule ("CHORUS");
    addToPanel (chorus, { "chorusMix", "choRate", "choDepth" });
    chorus.setPreferredColumns (1);

    auto& delay = *addModule ("DELAY");
    delay.setAutoLayout (false);
    addToPanel (delay, { "delayTime", "delayMode", "delayDepth", "delayFeedback" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("delayMode")))
        selector->setGridColumns (2);
}

void PRA32ParameterPage::buildCharacter()
{
    auto& voice = *addModule ("VOICE");
    voice.setAutoLayout (false);
    addToPanel (voice, { "portaMode", "voiceAsgnMode", "bthAmpMod" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("portaMode")))
        selector->setGridColumns (3);
    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("voiceAsgnMode")))
        selector->setGridColumns (1);
    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("bthAmpMod")))
        selector->setGridColumns (1);

    auto& character = *addModule ("VOICE CHARACTER");
    addToPanel (character, { "egVelSens", "ampVelSens", "aftTlfoAmt" });
    character.setPreferredColumns (3);

    auto& perf = *addModule ("PERFORMANCE");
    addToPanel (perf, { "pbRange", "portaTime" });
    perf.setPreferredColumns (2);

    auto& pitch = *addModule ("PITCH MODULATION");
    pitch.setAutoLayout (false);
    addToPanel (pitch, { "egOscAmt", "egOscDst" });

    if (auto* selector = dynamic_cast<SteppedSelector*> (find ("egOscDst")))
        selector->setGridColumns (3);

    auto& output = *addModule ("OUTPUT");
    addToPanel (output, { "pan", "ampGain", "ampExpnt" });
    output.setPreferredColumns (3);
}

//==============================================================================
void PRA32ParameterPage::paint (juce::Graphics& g)
{
    g.fillAll (chassis);
}

void PRA32ParameterPage::layoutStack (juce::Rectangle<int> area, int gap)
{
    const int n = panels.size();
    if (n == 0)
        return;

    const int panelHeight = (area.getHeight() - (n - 1) * gap) / n;
    int y = area.getY();

    for (auto* panel : panels)
    {
        panel->setBounds (area.getX(), y, area.getWidth(), panelHeight);
        y += panelHeight + gap;
    }
}

void PRA32ParameterPage::layoutOsc (juce::Rectangle<int> area)
{
    const int gap = 8;

    const int topHeight = area.getHeight() * 68 / 100;
    auto topArea = area.removeFromTop (topHeight);
    area.removeFromTop (gap);
    auto mixerArea = area;

    const int halfW = (topArea.getWidth() - gap) / 2;
    auto osc1Area = topArea.removeFromLeft (halfW);
    topArea.removeFromLeft (gap);
    auto osc2Area = topArea;

    if (auto* m = pageModule (moduleById, "OSCILLATOR 1")) m->setBounds (osc1Area);
    if (auto* m = pageModule (moduleById, "OSCILLATOR 2")) m->setBounds (osc2Area);
    if (auto* m = pageModule (moduleById, "MIXER"))        m->setBounds (mixerArea);

    // Layout OSCILLATOR 1 internally
    {
        auto inner = juce::Rectangle<int> (osc1Area.getWidth(), osc1Area.getHeight()).reduced (6, 4);
        inner.removeFromTop (16);

        const int topRowH = inner.getHeight() * 44 / 100;
        auto topRow = inner.removeFromTop (topRowH);
        auto botRow = inner;

        const int wWave = topRow.getWidth() * 62 / 100;
        if (auto* c = find ("osc1Wave")) c->setBounds (topRow.removeFromLeft (wWave).reduced (4, 4));
        if (auto* c = find ("sawWMode")) c->setBounds (topRow.reduced (4, 4));

        const int knobW = botRow.getWidth() / 3;
        if (auto* c = find ("osc1Shape")) c->setBounds (botRow.removeFromLeft (knobW).reduced (4, 4));
        if (auto* c = find ("osc1Morph")) c->setBounds (botRow.removeFromLeft (knobW).reduced (4, 4));
        if (auto* c = find ("oscDrift")) c->setBounds (botRow.reduced (4, 4));
    }

    // Layout OSCILLATOR 2 internally
    {
        auto inner = juce::Rectangle<int> (osc2Area.getWidth(), osc2Area.getHeight()).reduced (6, 4);
        inner.removeFromTop (16);

        const int topRowH = inner.getHeight() * 44 / 100;
        auto topRow = inner.removeFromTop (topRowH);
        auto botRow = inner;

        const int waveW = juce::jmin (topRow.getWidth() * 70 / 100, 280);
        auto waveBounds = topRow.withSizeKeepingCentre (waveW, topRow.getHeight()).reduced (4, 4);
        if (auto* c = find ("osc2Wave")) c->setBounds (waveBounds);

        const int knobW = botRow.getWidth() / 2;
        if (auto* c = find ("osc2Coarse")) c->setBounds (botRow.removeFromLeft (knobW).reduced (8, 4));
        if (auto* c = find ("osc2Pitch"))  c->setBounds (botRow.reduced (8, 4));
    }

    // Layout MIXER internally
    {
        auto inner = juce::Rectangle<int> (mixerArea.getWidth(), mixerArea.getHeight()).reduced (6, 4);
        inner.removeFromTop (16);

        const int knobW = inner.getWidth() / 2;
        if (auto* c = find ("oscMix")) c->setBounds (inner.removeFromLeft (knobW).reduced (12, 4));
        if (auto* c = find ("subOsc")) c->setBounds (inner.reduced (12, 4));
    }
}

void PRA32ParameterPage::layoutFilter (juce::Rectangle<int> area)
{
    const int gap = 8;

    auto filterBounds = area.removeFromLeft (area.getWidth() * 62 / 100);
    area.removeFromLeft (gap);
    auto shapingBounds = area;

    if (auto* m = pageModule (moduleById, "FILTER NETWORK")) m->setBounds (filterBounds);
    if (auto* m = pageModule (moduleById, "FILTER SHAPING")) m->setBounds (shapingBounds);

    // Children of the FILTER NETWORK panel use panel-local coordinates.
    {
        auto inner = juce::Rectangle<int> (filterBounds.getWidth(), filterBounds.getHeight())
                         .reduced (6, 4);
        inner.removeFromTop (16);

        auto topRow = inner.removeFromTop (juce::jmin (inner.getHeight() * 52 / 100, 200));
        const int half = topRow.getWidth() / 2;

        auto leftBox  = topRow.removeFromLeft (half).reduced (8, 0);
        auto rightBox = topRow.reduced (8, 0);

        if (auto* c = find ("filterCutoff")) c->setBounds (leftBox);
        if (auto* c = find ("filterReso"))   c->setBounds (rightBox);

        inner.removeFromTop (6);

        if (graphs.size() > 0)
            graphs[0]->setBounds (inner.reduced (6, 0));
    }

    // Children of the FILTER SHAPING panel
    {
        auto inner = juce::Rectangle<int> (shapingBounds.getWidth(), shapingBounds.getHeight())
                         .reduced (6, 4);
        inner.removeFromTop (16);

        auto row1 = inner.removeFromTop (inner.getHeight() * 40 / 100);
        const int wMode = row1.getWidth() * 46 / 100;
        if (auto* c = find ("filterMode")) c->setBounds (row1.removeFromLeft (wMode).reduced (4, 4));
        if (auto* c = find ("egFltAmt"))   c->setBounds (row1.reduced (4, 4));

        auto row2 = inner.removeFromTop (inner.getHeight() * 48 / 100);
        const int halfKnob = row2.getWidth() / 2;
        if (auto* c = find ("filterKeyTrk")) c->setBounds (row2.removeFromLeft (halfKnob).reduced (4, 4));
        if (auto* c = find ("bthFltAmt"))    c->setBounds (row2.reduced (4, 4));

        if (auto* c = find ("relEqDcy")) c->setBounds (inner.reduced (8, 4));
    }
}

void PRA32ParameterPage::layoutEnvs (juce::Rectangle<int> area)
{
    const int gap = 8;
    const int half = (area.getWidth() - gap) / 2;

    auto modBounds = area.removeFromLeft (half);
    area.removeFromLeft (gap);
    auto ampBounds = area;

    if (auto* m = pageModule (moduleById, "MOD ENVELOPE")) m->setBounds (modBounds);
    if (auto* m = pageModule (moduleById, "AMP ENVELOPE")) m->setBounds (ampBounds);

    auto placeEnvelope = [] (juce::Rectangle<int> moduleBounds, EnvelopePanel* env)
    {
        if (env == nullptr) return;
        auto inner = juce::Rectangle<int> (moduleBounds.getWidth(), moduleBounds.getHeight())
                         .reduced (6, 4);
        inner.removeFromTop (16);
        env->setBounds (inner.reduced (4));
    };

    placeEnvelope (modBounds, envelopes.size() > 0 ? envelopes[0] : nullptr);
    placeEnvelope (ampBounds, envelopes.size() > 1 ? envelopes[1] : nullptr);
}

void PRA32ParameterPage::layoutMod (juce::Rectangle<int> area)
{
    if (auto* m = pageModule (moduleById, "MODULATION LFO"))
        m->setBounds (area);

    auto inner = juce::Rectangle<int> (area.getWidth(), area.getHeight()).reduced (6, 4);
    inner.removeFromTop (16);

    const int halfH = inner.getHeight() / 2;
    auto topRow = inner.removeFromTop (halfH);
    auto botRow = inner;

    const int waveW = juce::jmin (260, topRow.getWidth() * 32 / 100);
    if (auto* c = find ("lfoWave")) c->setBounds (topRow.removeFromLeft (waveW).reduced (6, 6));

    const int knobW = topRow.getWidth() / 3;
    if (auto* c = find ("lfoRate"))     c->setBounds (topRow.removeFromLeft (knobW).reduced (4, 4));
    if (auto* c = find ("lfoFadeTime")) c->setBounds (topRow.removeFromLeft (knobW).reduced (4, 4));
    if (auto* c = find ("lfoDepth"))    c->setBounds (topRow.reduced (4, 4));

    if (auto* c = find ("lfoOscDst")) c->setBounds (botRow.removeFromLeft (waveW).reduced (6, 6));

    const int botKnobW = botRow.getWidth() / 2;
    if (auto* c = find ("lfoFltAmt")) c->setBounds (botRow.removeFromLeft (botKnobW).reduced (12, 4));
    if (auto* c = find ("lfoOscAmt")) c->setBounds (botRow.reduced (12, 4));
}

void PRA32ParameterPage::layoutFx (juce::Rectangle<int> area)
{
    const int gap = 8;
    const int chorusW = juce::jmax (260, area.getWidth() * 34 / 100);

    auto chorus = area.removeFromLeft (chorusW);
    area.removeFromLeft (gap);
    auto delay = area;

    if (auto* m = pageModule (moduleById, "CHORUS")) m->setBounds (chorus);
    if (auto* m = pageModule (moduleById, "DELAY"))  m->setBounds (delay);

    auto inner = juce::Rectangle<int> (delay.getWidth(), delay.getHeight()).reduced (6, 4);
    inner.removeFromTop (16);

    const int halfH = inner.getHeight() / 2;
    auto topRow = inner.removeFromTop (halfH);
    auto botRow = inner;

    const int halfW = topRow.getWidth() / 2;
    if (auto* c = find ("delayTime"))     c->setBounds (topRow.removeFromLeft (halfW).reduced (6, 4));
    if (auto* c = find ("delayMode"))     c->setBounds (topRow.reduced (16, 20));
    if (auto* c = find ("delayDepth"))    c->setBounds (botRow.removeFromLeft (halfW).reduced (6, 4));
    if (auto* c = find ("delayFeedback")) c->setBounds (botRow.reduced (6, 4));
}

void PRA32ParameterPage::layoutCharacter (juce::Rectangle<int> area)
{
    const int gap = 8;
    const int rowHeight = (area.getHeight() - gap) / 2;

    auto topRow = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    auto botRow = area;

    // --- Row 1: VOICE (58%) & VOICE CHARACTER (42%) ---
    const int voiceW = topRow.getWidth() * 58 / 100;
    auto voiceBounds = topRow.removeFromLeft (voiceW);
    topRow.removeFromLeft (gap);
    auto charBounds = topRow;

    if (auto* m = pageModule (moduleById, "VOICE"))           m->setBounds (voiceBounds);
    if (auto* m = pageModule (moduleById, "VOICE CHARACTER")) m->setBounds (charBounds);

    // Layout VOICE internally
    {
        auto inner = juce::Rectangle<int> (voiceBounds.getWidth(), voiceBounds.getHeight()).reduced (6, 4);
        inner.removeFromTop (16);

        const int wTotal = inner.getWidth();
        const int wPorta = wTotal * 44 / 100;
        const int wAsgn  = wTotal * 26 / 100;

        auto rPorta = inner.removeFromLeft (wPorta).reduced (4, 4);
        auto rAsgn  = inner.removeFromLeft (wAsgn).reduced (4, 4);
        auto rBth   = inner.reduced (4, 4);

        if (auto* c = find ("portaMode"))     c->setBounds (rPorta);
        if (auto* c = find ("voiceAsgnMode")) c->setBounds (rAsgn);
        if (auto* c = find ("bthAmpMod"))     c->setBounds (rBth);
    }

    // --- Row 2: PERFORMANCE (29%), PITCH MODULATION (39%), OUTPUT (32%) ---
    const int totalBotW = botRow.getWidth() - 2 * gap;
    const int perfW  = totalBotW * 29 / 100;
    const int pitchW = totalBotW * 39 / 100;

    auto perfBounds  = botRow.removeFromLeft (perfW);
    botRow.removeFromLeft (gap);
    auto pitchBounds = botRow.removeFromLeft (pitchW);
    botRow.removeFromLeft (gap);
    auto outBounds   = botRow;

    if (auto* m = pageModule (moduleById, "PERFORMANCE"))      m->setBounds (perfBounds);
    if (auto* m = pageModule (moduleById, "PITCH MODULATION")) m->setBounds (pitchBounds);
    if (auto* m = pageModule (moduleById, "OUTPUT"))           m->setBounds (outBounds);

    // Layout PITCH MODULATION internally
    {
        auto inner = juce::Rectangle<int> (pitchBounds.getWidth(), pitchBounds.getHeight()).reduced (6, 4);
        inner.removeFromTop (16);

        const int knobW = inner.getWidth() * 42 / 100;
        if (auto* c = find ("egOscAmt")) c->setBounds (inner.removeFromLeft (knobW).reduced (4, 4));
        if (auto* c = find ("egOscDst")) c->setBounds (inner.reduced (4, 4));
    }
}

void PRA32ParameterPage::resized()
{
    auto area = getLocalBounds();

    // Narrow windows reflow every section into a legible vertical stack rather
    // than shrinking controls until labels collide.
    if (area.getWidth() < 760)
    {
        layoutStack (area, 8);
        return;
    }

    if (section == "OSC")              layoutOsc (area);
    else if (section == "FILTER")      layoutFilter (area);
    else if (section == "ENVS")        layoutEnvs (area);
    else if (section == "MOD")         layoutMod (area);
    else if (section == "FX")          layoutFx (area);
    else if (section == "CHARACTER"
          || section == "COLOR")       layoutCharacter (area);
    else                               layoutStack (area, 8);
}

//==============================================================================
PRA32ColorcoderAudioProcessorEditor::PRA32ColorcoderAudioProcessorEditor (
        PRA32ColorcoderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      keyboardComponent (audioProcessor.keyboardState)
{
    setLookAndFeel (&customLookAndFeel);
    audioProcessor.addChangeListener (this);

    for (int i = 0; i < 6; ++i)
    {
        auto* page = pages.add (new PRA32ParameterPage (audioProcessor, kSections[i]));
        addChildComponent (page);
    }

    sectionSelector.setSections ({ kSections[0], kSections[1], kSections[2],
                                   kSections[3], kSections[4], kSections[5] });
    sectionSelector.onSectionChanged = [this] (int index) { showPage (index); };

    presetPrevButton.onClick = [this] ()
    {
        loadPreset ((juce::jmax (0, audioProcessor.getCurrentFactoryPreset()) + 15) % 16);
    };
    presetNextButton.onClick = [this] ()
    {
        loadPreset ((juce::jmax (0, audioProcessor.getCurrentFactoryPreset()) + 1) % 16);
    };
    presetDisplay.onClick   = [this] () { showPresetMenu(); };

    loadButton.onClick = [this] ()
    {
        requestConfirmation ("LOAD PATCH FROM JSON",
                             "Loading a JSON patch replaces the current patch. Continue?",
                             "LOAD",
                             [this] ()
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Load Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
            const auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                    audioProcessor.loadPresetFromJson (file.loadFileAsString());
            });
        });
    };

    initButton.onClick = [this] ()
    {
        requestConfirmation ("INITIALISE PATCH",
                             "This replaces the current patch with the INITIALIZATION preset. Continue?",
                             "INIT",
                             [this] () { loadPreset (0); });
    };

    saveButton.onClick = [this] ()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
        const auto flags = juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
                file.replaceWithText (audioProcessor.savePresetToJson());
        });
    };

    // Restore persisted UI state (falls back to defaults on first run).
    int startSection = 0;
    {
        const auto s = audioProcessor.getUiProperty (uiSectionProperty);
        const auto k = audioProcessor.getUiProperty (uiKeyboardProperty);
        const auto o = audioProcessor.getUiProperty (uiOctaveProperty);

        if (! s.isVoid()) startSection = juce::jlimit (0, 5, (int) s);
        keyboardVisible  = k.isVoid() ? true : (bool) k;
        keyboardBaseNote = o.isVoid() ? 36   : juce::jlimit (0, 67, (int) o);
    }

    keyboardButton.setClickingTogglesState (true);
    keyboardButton.setToggleState (keyboardVisible, juce::dontSendNotification);
    keyboardButton.onClick = [this] ()
    {
        keyboardVisible = keyboardButton.getToggleState();
        storeUiState();
        resized();
    };

    octaveDownButton.onClick = [this] ()
    {
        keyboardBaseNote = juce::jmax (0, keyboardBaseNote - 12);
        updateKeyboardRange();
        storeUiState();
    };
    octaveUpButton.onClick = [this] ()
    {
        keyboardBaseNote = juce::jmin (127 - 60, keyboardBaseNote + 12);
        updateKeyboardRange();
        storeUiState();
    };

    updateKeyboardRange();
    keyboardComponent.setKeyWidth (20.0f);
    keyboardComponent.setBlackNoteWidthProportion (0.6f);
    keyboardComponent.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colours::transparentBlack);
    keyboardComponent.setColour (juce::MidiKeyboardComponent::shadowColourId,    juce::Colours::transparentBlack);

    addAndMakeVisible (sectionSelector);
    addAndMakeVisible (presetPrevButton);
    addAndMakeVisible (presetNextButton);
    addAndMakeVisible (presetDisplay);
    addAndMakeVisible (initButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (keyboardButton);
    addAndMakeVisible (keyboardComponent);
    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);
    addChildComponent (confirmOverlay);

    audioProcessor.getAPVTS().state.addListener (this);

    showPage (startSection);
    updatePresetDisplay();

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);
}

PRA32ColorcoderAudioProcessorEditor::~PRA32ColorcoderAudioProcessorEditor()
{
    audioProcessor.getAPVTS().state.removeListener (this);
    audioProcessor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void PRA32ColorcoderAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updatePresetDisplay();
    repaint();
}

void PRA32ColorcoderAudioProcessorEditor::valueTreePropertyChanged (juce::ValueTree&,
                                                                    const juce::Identifier&)
{
    const bool edited = audioProcessor.isCurrentPatchEdited();

    if (edited != lastEdited)
    {
        lastEdited = edited;
        presetDisplay.setModified (edited);
    }
}

void PRA32ColorcoderAudioProcessorEditor::requestConfirmation (
        const juce::String& title, const juce::String& message,
        const juce::String& confirmText, std::function<void()> action)
{
    confirmOverlay.setBounds (getLocalBounds());
    confirmOverlay.ask (title, message, confirmText, std::move (action));
}

void PRA32ColorcoderAudioProcessorEditor::storeUiState()
{
    audioProcessor.setUiProperty (uiKeyboardProperty, keyboardVisible);
    audioProcessor.setUiProperty (uiOctaveProperty, keyboardBaseNote);
    audioProcessor.setUiProperty (uiSectionProperty, sectionSelector.getActive());
}

void PRA32ColorcoderAudioProcessorEditor::showPage (int index)
{
    index = juce::jlimit (0, pages.size() - 1, index);

    for (int i = 0; i < pages.size(); ++i)
        pages[i]->setVisible (i == index);

    sectionSelector.setActive (index, juce::dontSendNotification);
    audioProcessor.setUiProperty (uiSectionProperty, index);
}

void PRA32ColorcoderAudioProcessorEditor::updateKeyboardRange()
{
    keyboardComponent.setAvailableRange (keyboardBaseNote, keyboardBaseNote + 60);
}

void PRA32ColorcoderAudioProcessorEditor::loadPreset (int index)
{
    audioProcessor.loadPreset (((index % 16) + 16) % 16);
    updatePresetDisplay();
}

void PRA32ColorcoderAudioProcessorEditor::updatePresetDisplay()
{
    const int current = audioProcessor.getCurrentFactoryPreset();

    if (current >= 0)
        presetDisplay.setText (juce::String::formatted ("%02d  ", current)
                                   + PRA32ColorcoderAudioProcessor::factoryPresetName (current),
                               "PROGRAM / FACTORY");
    else
        presetDisplay.setText ("--  USER / SESSION", "MANUAL / PATCH");

    presetDisplay.setPrimaryFont (presetFont());

    const bool edited = audioProcessor.isCurrentPatchEdited();
    lastEdited = edited;
    presetDisplay.setModified (edited);
}

void PRA32ColorcoderAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const int current = audioProcessor.getCurrentFactoryPreset();

    for (int i = 0; i < 16; ++i)
        menu.addItem (i + 1,
                      juce::String::formatted ("%02d  ", i)
                          + PRA32ColorcoderAudioProcessor::factoryPresetName (i),
                      true, i == current);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetDisplay),
                        [this] (int result)
                        {
                            if (result > 0)
                                loadPreset (result - 1);
                        });
}

//==============================================================================
void PRA32ColorcoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (chassis);

    const float railW = juce::jmax (10.0f, sc (14.0f));
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    // Rack side rails with mounting screws.
    drawBrushedPanel (g, { 0.0f, 0.0f, railW, h }, rackMetal, true);
    drawBrushedPanel (g, { w - railW, 0.0f, railW, h }, rackMetal, true);

    g.setColour (rackMetalEdge.withAlpha (0.8f));
    g.drawLine (railW, 0.0f, railW, h, 1.0f);
    g.drawLine (w - railW, 0.0f, w - railW, h, 1.0f);

    const float screwY[3] = { sc (60.0f), h * 0.5f, h - sc (60.0f) };
    for (float y : screwY)
    {
        drawScrew (g, { railW * 0.5f, y }, sc (3.0f));
        drawScrew (g, { w - railW * 0.5f, y }, sc (3.0f));
    }

    // Header strip.
    auto header = getLocalBounds().removeFromTop (headerHeight);
    drawBrushedPanel (g, header.toFloat(), panel, false);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (0.0f, (float) header.getBottom(), w, (float) header.getBottom(), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (0.0f, (float) header.getBottom() - 1.0f, w, (float) header.getBottom() - 1.0f, 1.0f);

    // Serigraphed brand plate for Tonecoder TC-32.
    const float brandLeft = railW + sc (8.0f);
    const float brandTopY = sc (6.0f);
    const float brandH    = (float) headerHeight - sc (12.0f);
    const float brandW    = sc (290.0f);
    auto brandArea = juce::Rectangle<float> (brandLeft, brandTopY, brandW, brandH);

    // Discrete signal/power status lamp
    const float lampSize = sc (9.0f);
    auto lampBox = brandArea.removeFromLeft (lampSize + sc (8.0f))
                            .withSizeKeepingCentre (lampSize, lampSize);
    drawLamp (g, lampBox, amberBright, 0.85f, sc (2.5f));

    auto brandUpper = brandArea.removeFromTop (brandArea.getHeight() * 0.54f);
    auto brandLower = brandArea;

    // Model inscription "TC-32"
    const float modelTextW = sc (52.0f);
    auto modelRect = brandUpper.removeFromLeft (modelTextW);
    drawEngravedText (g, "TC-32", modelRect,
                      juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")),
                      juce::Justification::centredLeft, amberBright,
                      engraveShadow.withAlpha (0.8f));

    // Divider dot
    g.setColour (textDim.withAlpha (0.8f));
    const float dotX = modelRect.getRight() + sc (2.0f);
    const float dotY = brandUpper.getCentreY();
    g.fillEllipse (dotX, dotY - sc (1.5f), sc (3.0f), sc (3.0f));

    // Brand inscription "T O N E C O D E R"
    brandUpper.removeFromLeft (sc (9.0f));
    drawEngravedText (g, "T O N E C O D E R", brandUpper,
                      brandFont(), juce::Justification::centredLeft, textPrimary,
                      engraveShadow.withAlpha (0.85f));

    // Secondary technical inscription "FOUR-VOICE POLYPHONIC SIGNAL SYNTHESIZER"
    drawEngravedText (g, "FOUR-VOICE POLYPHONIC SIGNAL SYNTHESIZER", brandLower,
                      subBrandFont(), juce::Justification::centredLeft, textSecondary,
                      engraveShadow.withAlpha (0.7f));

    // Keyboard rack plate.
    if (keyboardVisible && ! keyboardFrame.isEmpty())
    {
        drawRaisedPlate (g, keyboardFrame.toFloat(), radiusMedium);
        const float screwR = juce::jlimit (sc (2.4f), sc (3.6f), keyboardFrame.getHeight() * 0.02f);
        drawScrewsInCorners (g, keyboardFrame.toFloat(), sc (9.0f), screwR);

        auto label = keyboardFrame.reduced (juce::roundToInt (sc (18.0f)), juce::roundToInt (sc (6.0f)))
                         .removeFromTop (juce::roundToInt (sc (14.0f)));

        const int lampSize = juce::roundToInt (sc (12.0f));
        auto lampArea = label.removeFromRight (lampSize).withSizeKeepingCentre (lampSize, lampSize);
        label.removeFromRight (juce::roundToInt (sc (6.0f)));

        drawLamp (g, lampArea.toFloat(), amber, 0.7f, sc (2.5f));
        drawEngravedText (g, "KEYBOARD", label.toFloat(), legendFont(),
                           juce::Justification::centredLeft, textSecondary,
                           engraveShadow.withAlpha (0.6f));
    }
}

void PRA32ColorcoderAudioProcessorEditor::resized()
{
    setUiScale (juce::jlimit (0.8f, 1.6f, (float) getWidth() / 960.0f));

    const int rail   = juce::jmax (14, juce::roundToInt (sc (14.0f)));
    const int margin = rail + juce::roundToInt (sc (4.0f));
    const int gap    = juce::roundToInt (sc (8.0f));

    auto r = getLocalBounds();

    // ---- Header -------------------------------------------------------------
    auto header = r.removeFromTop (headerHeight).reduced (margin, juce::roundToInt (sc (9.0f)));

    const bool compact = getWidth() < 860;
    const int utilityW = juce::roundToInt (sc (compact ? 66.0f : 80.0f));
    const int keysW    = juce::roundToInt (sc (compact ? 48.0f : 54.0f));
    auto utility = header.removeFromRight (utilityW * 3 + keysW + gap * 3);
    keyboardButton.setBounds (utility.removeFromRight (keysW));
    utility.removeFromRight (gap);
    saveButton.setBounds (utility.removeFromRight (utilityW));
    utility.removeFromRight (gap);
    loadButton.setBounds (utility.removeFromRight (utilityW));
    utility.removeFromRight (gap);
    initButton.setBounds (utility.removeFromRight (utilityW));

    // Brand reservation on left
    const int brandReservedW = juce::roundToInt (sc (compact ? 240.0f : 295.0f));
    header.removeFromLeft (brandReservedW + gap);

    // Preset group centered in remaining area
    const int presetGroupW = juce::jmin (juce::roundToInt (sc (340.0f)), header.getWidth());
    auto presetGroup = header.withSizeKeepingCentre (presetGroupW, header.getHeight());
    const int arrowW = juce::roundToInt (sc (28.0f));
    presetPrevButton.setBounds (presetGroup.removeFromLeft (arrowW));
    presetNextButton.setBounds (presetGroup.removeFromRight (arrowW));
    presetDisplay.setBounds (presetGroup.reduced (juce::roundToInt (sc (4.0f)),
                                                 juce::roundToInt (sc (1.0f))));

    // ---- Section bar --------------------------------------------------------
    sectionSelector.setBounds (r.removeFromTop (sectionBarHeight).reduced (margin,
                                                                           juce::roundToInt (sc (3.0f))));

    // ---- Keyboard -----------------------------------------------------------
    if (keyboardVisible)
    {
        const int keyboardH = juce::jlimit (juce::roundToInt (sc (52.0f)),
                                            keyboardHeight,
                                            juce::jmax (juce::roundToInt (sc (52.0f)), getHeight() / 8));

        keyboardFrame = r.removeFromBottom (keyboardH).reduced (margin, juce::roundToInt (sc (5.0f)));

        auto inner = keyboardFrame.reduced (juce::roundToInt (sc (10.0f)),
                                            juce::roundToInt (sc (6.0f)));
        inner.removeFromTop (juce::roundToInt (sc (14.0f)));
        keyboardKeys = inner;

        keyboardComponent.setVisible (true);
        constexpr int numWhiteKeys = 36;
        keyboardComponent.setKeyWidth ((float) keyboardKeys.getWidth() / (float) numWhiteKeys);
        keyboardComponent.setBounds (keyboardKeys);

        // Octave shift lives on the keyboard plate header, left of the lamp.
        auto strip = keyboardFrame.reduced (juce::roundToInt (sc (18.0f)),
                                            juce::roundToInt (sc (6.0f)))
                         .removeFromTop (juce::roundToInt (sc (14.0f)));
        strip.removeFromRight (juce::roundToInt (sc (18.0f))); // lamp + gap

        auto upArea = strip.removeFromRight (juce::roundToInt (sc (42.0f)));
        strip.removeFromRight (juce::roundToInt (sc (4.0f)));
        auto downArea = strip.removeFromRight (juce::roundToInt (sc (42.0f)));

        octaveDownButton.setVisible (true);
        octaveUpButton.setVisible (true);
        octaveDownButton.setBounds (downArea.reduced (0, 1));
        octaveUpButton.setBounds (upArea.reduced (0, 1));
    }
    else
    {
        keyboardFrame = {};
        keyboardKeys = {};
        keyboardComponent.setVisible (false);
        octaveDownButton.setVisible (false);
        octaveUpButton.setVisible (false);
    }

    // ---- Active page --------------------------------------------------------
    auto pageArea = r.reduced (margin, juce::roundToInt (sc (8.0f)));

    for (auto* page : pages)
        page->setBounds (pageArea);

    // ---- Confirmation overlay ----------------------------------------------
    confirmOverlay.setBounds (getLocalBounds());
}
