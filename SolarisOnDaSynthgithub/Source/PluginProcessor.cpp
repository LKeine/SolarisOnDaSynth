#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SolarisProcessor::SolarisProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& d : solaris::paramDefs())
    {
        auto* raw = apvts.getRawParameterValue (d.id);
        jassert (raw != nullptr);
        rawParams.push_back (raw);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SolarisProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto& d : solaris::paramDefs())
    {
        const juce::ParameterID pid { d.id, 1 };

        if (d.kind == solaris::Kind::Bool)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.defV > 0.5f));
        }
        else if (d.kind == solaris::Kind::Choice)
        {
            juce::StringArray choices;
            for (auto& c : d.choices)
                choices.add (c);
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, choices, (int) d.defV));
        }
        else
        {
            juce::NormalisableRange<float> range (d.minV, d.maxV);
            if (d.centre > 0.0f)
                range.setSkewForCentre (d.centre);

            const auto unit = d.unit;
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                pid, d.name, range, d.defV,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [unit] (float v, int) { return juce::String (solaris::formatValue (unit, v)); })));
        }
    }
    return layout;
}

//==============================================================================
void SolarisProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    blockSize = juce::jmax (32, samplesPerBlock);
    engine.prepare (sampleRate, blockSize);
    analyzer.prepare (sampleRate, 24.0f);
    setLatencySamples (solaris::Engine::getLatency());
}

bool SolarisProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

solaris::Settings SolarisProcessor::readSettings() const
{
    solaris::Settings s;
    const auto& defs = solaris::paramDefs();
    for (size_t i = 0; i < defs.size(); ++i)
        s.*(defs[i].field) = rawParams[i]->load();
    return s;
}

void SolarisProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 1.0)
                    bpm.store ((float) *hostBpm);

    const auto settings = readSettings();

    // Analyse
    // (on lit l'etat AVANT la demande : startAnalysis() ecrit la demande avant l'etat)
    solaris::Analyzer* ana = nullptr;
    if (analysisState.load() == analysisRunning)
    {
        if (analysisStartRequested.exchange (false))
            analyzer.start (analysisTargetSeconds.load());
        ana = &analyzer;
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, numSamples - pos);
        engine.process (L + pos, R + pos, n, settings, (double) bpm.load(), ana);
    }

    if (ana != nullptr)
    {
        analysisProgress.store (analyzer.getProgress());
        if (analyzer.isDone())
            analysisState.store (analysisDone, std::memory_order_release);
    }

    gainReduction.store (engine.getGainReductionDb());
    const auto& lv = engine.getLevels();
    for (int ch = 0; ch < 2; ++ch)
    {
        atomicMax (inPeak[ch], lv.inPeak[ch]);
        atomicMax (outPeak[ch], lv.outPeak[ch]);
    }
    autoGainDb.store (lv.autoGainDb);
    const auto t = engine.getReverbTimes();
    decaySec.store (t.decaySec);
    predelayMs.store (t.predelayMs);
}

//==============================================================================
void SolarisProcessor::startAnalysis (int scope)
{
    analysisScope.store (scope);
    analysisTargetSeconds.store (solaris::analysisSeconds (readSettings()));
    analysisProgress.store (0.0f);
    analysisStartRequested.store (true);
    analysisState.store (analysisRunning, std::memory_order_release);
}

void SolarisProcessor::cancelAnalysis()
{
    analysisState.store (analysisIdle, std::memory_order_release);
}

void SolarisProcessor::setParam (const char* id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

juce::String SolarisProcessor::applyAnalysisResult()
{
    if (getAnalysisState() != analysisDone)
        return {};

    // Le thread audio n'ecrit plus dans l'analyseur une fois l'etat "Done"
    const auto report = analyzer.makeReport();
    analysisState.store (analysisIdle, std::memory_order_release);

    const auto scope = analysisScope.load() == 1 ? solaris::Scope::Compressor : solaris::Scope::All;
    const auto current = readSettings();
    std::string summary;
    const auto suggested = solaris::suggestSettings (report, current, scope, (double) bpm.load(), summary);

    for (auto& d : solaris::paramDefs())
    {
        const bool inScope = scope == solaris::Scope::All || juce::String (d.id).startsWith ("comp");
        const float v = suggested.*(d.field);
        if (inScope && std::abs (v - current.*(d.field)) > 1.0e-4f)
            setParam (d.id, v);
    }
    return juce::String (summary);
}

//==============================================================================
juce::StringArray SolarisProcessor::getPresetNames()
{
    juce::StringArray names;
    for (auto& p : solaris::presets())
        names.add (p.name);
    return names;
}

void SolarisProcessor::applySettings (const solaris::Settings& s)
{
    for (auto& d : solaris::paramDefs())
        if (! solaris::isGlobalParam (d.id))
            setParam (d.id, s.*(d.field));
}

void SolarisProcessor::applyPreset (int index)
{
    applySettings (solaris::presetSettings (index));
}

//==============================================================================
void SolarisProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == currentSlot)
        return;

    slots[currentSlot] = readSettings();
    slotValid[currentSlot] = true;
    if (! slotValid[slot])
    {
        slots[slot] = slots[currentSlot];   // B demarre comme une copie de A
        slotValid[slot] = true;
    }
    currentSlot = slot;
    applySettings (slots[slot]);
}

void SolarisProcessor::copyCurrentToOtherSlot()
{
    const int other = 1 - currentSlot;
    slots[other] = readSettings();
    slotValid[other] = true;
}

//==============================================================================
juce::File SolarisProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("SolarisOnDaSynth").getChildFile ("Presets");
}

juce::StringArray SolarisProcessor::getUserPresetNames()
{
    juce::StringArray names;
    for (auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.solaris"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool SolarisProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    const auto s = readSettings();
    juce::XmlElement xml ("SolarisPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (auto& d : solaris::paramDefs())
    {
        if (solaris::isGlobalParam (d.id))
            continue;
        auto* e = xml.createNewChildElement ("param");
        e->setAttribute ("id", d.id);
        e->setAttribute ("value", (double) (s.*(d.field)));
    }

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;
    return xml.writeTo (folder.getChildFile (name + ".solaris"));
}

bool SolarisProcessor::loadUserPreset (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (getUserPresetFolder().getChildFile (name + ".solaris"));
    if (xml == nullptr || ! xml->hasTagName ("SolarisPreset"))
        return false;

    auto s = solaris::defaultSettings();   // un parametre absent (ancien preset) reprend sa valeur par defaut
    for (auto* e : xml->getChildWithTagNameIterator ("param"))
        solaris::setById (s, e->getStringAttribute ("id").toRawUTF8(), (float) e->getDoubleAttribute ("value"));
    applySettings (s);
    return true;
}

bool SolarisProcessor::deleteUserPreset (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (name + ".solaris").deleteFile();
}

//==============================================================================
void SolarisProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // A/B sauvegarde avec le projet Ableton
    auto* ab = new juce::XmlElement ("SolarisAB");
    ab->setAttribute ("current", currentSlot);
    for (int k = 0; k < 2; ++k)
    {
        if (! slotValid[k])
            continue;
        auto* slot = ab->createNewChildElement ("slot");
        slot->setAttribute ("index", k);
        for (auto& d : solaris::paramDefs())
            slot->setAttribute (d.id, (double) (slots[k].*(d.field)));
    }
    xml->addChildElement (ab);
    copyXmlToBinary (*xml, destData);
}

void SolarisProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    if (auto* ab = xml->getChildByName ("SolarisAB"))
    {
        currentSlot = juce::jlimit (0, 1, ab->getIntAttribute ("current"));
        slotValid[0] = slotValid[1] = false;
        for (auto* slot : ab->getChildWithTagNameIterator ("slot"))
        {
            const int k = juce::jlimit (0, 1, slot->getIntAttribute ("index"));
            slots[k] = solaris::defaultSettings();
            for (auto& d : solaris::paramDefs())
                if (slot->hasAttribute (d.id))
                    slots[k].*(d.field) = (float) slot->getDoubleAttribute (d.id);
            slotValid[k] = true;
        }
        xml->removeChildElement (ab, true);
    }
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SolarisProcessor::createEditor()
{
    return new SolarisEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolarisProcessor();
}
