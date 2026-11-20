#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SolarisEngine.h"

//==============================================================================
// SolarisOnDaSynth : enveloppe JUCE autour du moteur solaris::Engine
// (le meme moteur sert a l'outil de rendu hors-ligne tools/solaris_render.cpp)
//==============================================================================
class SolarisProcessor : public juce::AudioProcessor
{
public:
    SolarisProcessor();
    ~SolarisProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::StringArray getPresetNames();
    void applyPreset (int index);   // thread de l'interface

    // Analyse : scope 0 = tout le plugin, 1 = compresseur seulement
    enum AnalysisState { analysisIdle = 0, analysisRunning = 1, analysisDone = 2 };
    void startAnalysis (int scope);
    void cancelAnalysis();
    int  getAnalysisState() const noexcept  { return analysisState.load (std::memory_order_acquire); }
    int  getAnalysisScope() const noexcept  { return analysisScope.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    juce::String applyAnalysisResult();     // thread de l'interface

    float getGainReductionDb() const noexcept { return gainReduction.load(); }
    float getAutoGainDb() const noexcept      { return autoGainDb.load(); }
    // Crete depuis la derniere lecture (l'interface "consomme" la valeur)
    float takeInputPeak (int ch) noexcept     { return inPeak[ch & 1].exchange (0.0f); }
    float takeOutputPeak (int ch) noexcept    { return outPeak[ch & 1].exchange (0.0f); }

    // A/B : deux jeux de reglages pour comparer
    int  getCurrentSlot() const noexcept      { return currentSlot; }
    void selectSlot (int slot);               // thread de l'interface
    void copyCurrentToOtherSlot();

    // Presets utilisateur (fichiers .solaris dans Documents/SolarisOnDaSynth/Presets)
    static juce::File getUserPresetFolder();
    static juce::StringArray getUserPresetNames();
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    static bool deleteUserPreset (const juce::String& name);
    float getDecaySec() const noexcept        { return decaySec.load(); }
    float getPredelayMs() const noexcept      { return predelayMs.load(); }
    float getBpm() const noexcept             { return bpm.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    solaris::Settings readSettings() const;
    void setParam (const char* id, float value);
    void applySettings (const solaris::Settings& s);     // sans bypass ni duree d'analyse
    static void atomicMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }

    std::vector<std::atomic<float>*> rawParams;   // meme ordre que solaris::paramDefs()
    solaris::Engine engine;
    solaris::Analyzer analyzer;
    int blockSize = 512;

    std::atomic<int> analysisState { analysisIdle }, analysisScope { 0 };
    std::atomic<bool> analysisStartRequested { false };
    std::atomic<float> analysisProgress { 0.0f };
    std::atomic<float> gainReduction { 0.0f }, decaySec { 2.0f }, predelayMs { 20.0f }, bpm { 120.0f };
    std::atomic<float> analysisTargetSeconds { 6.0f };
    std::atomic<float> autoGainDb { 0.0f }, inPeak[2] {}, outPeak[2] {};

    solaris::Settings slots[2];
    bool slotValid[2] { false, false };
    int currentSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolarisProcessor)
};
