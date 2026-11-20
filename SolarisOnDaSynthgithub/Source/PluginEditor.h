#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Theme spatial : nuit profonde, nebuleuses mauves, accents violet / magenta
namespace SolarisColours
{
    inline const juce::Colour space0   { 0xff07030f };
    inline const juce::Colour space1   { 0xff170a2c };
    inline const juce::Colour panel    { 0xb81a0f30 };
    inline const juce::Colour border   { 0xff7c3aed };
    inline const juce::Colour violet   { 0xffa855f7 };
    inline const juce::Colour lilac    { 0xffc4b5fd };
    inline const juce::Colour magenta  { 0xffe879f9 };
    inline const juce::Colour text     { 0xffede9fe };
    inline const juce::Colour dim      { 0xff9d8cc4 };
    inline const juce::Colour knobBody { 0xff2a1748 };
}

class SolarisLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SolarisLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name);
    void resized() override;
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name);
    void resized() override;
private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name);
    void resized() override;
private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Interrupteur "Sync BPM" + choix de la note (decay / predelay)
class SyncSelector : public juce::Component
{
public:
    SyncSelector (juce::AudioProcessorValueTreeState&, const juce::String& toggleId,
                  const juce::String& choiceId, const juce::String& name);
    void resized() override;
private:
    juce::Label label;
    juce::ToggleButton sync { "Sync BPM" };
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachment;
};

// Bouton d'analyse (global dans l'en-tete, ou compresseur seul)
class AnalysePanel : public juce::Component, private juce::Timer
{
public:
    AnalysePanel (SolarisProcessor&, int scope, bool horizontal);
    void resized() override;
private:
    void timerCallback() override;
    SolarisProcessor& proc;
    const int scope;
    const bool horizontal;
    bool startedHere = false;
    juce::TextButton button;
    juce::Label status;
};

class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    explicit GainReductionMeter (SolarisProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    SolarisProcessor& proc;
    float displayDb = 0.0f;
};

// Vumetre stereo (entree ou sortie) avec maintien de crete
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter (SolarisProcessor&, bool isInput);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    SolarisProcessor& proc;
    const bool input;
    float level[2] {}, hold[2] {};
    int holdFrames[2] {};
};

// Affiche les temps de reverb calcules (comme le "reverb calculator")
class ReverbReadout : public juce::Component, private juce::Timer
{
public:
    explicit ReverbReadout (SolarisProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    SolarisProcessor& proc;
    float decay = 0.0f, pre = 0.0f, bpm = 0.0f;
};

//==============================================================================
class Section : public juce::Component, private juce::Timer
{
public:
    Section (const juce::String& title, juce::AudioProcessorValueTreeState&, const juce::String& powerParamId);
    void addItem (juce::Component* item, int weight = 1);
    int getTotalWeight() const { return totalWeight; }
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    bool hasPower = false, lastState = true;
    std::vector<juce::Component*> items;
    std::vector<int> weights;
    int totalWeight = 0;
};

//==============================================================================
class SolarisEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SolarisEditor (SolarisProcessor&);
    ~SolarisEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetList (const juce::String& selectUserPreset = {});
    void promptSavePreset();
    void deleteSelectedPreset();

    SolarisProcessor& processorRef;
    SolarisLookAndFeel lookAndFeel;

    // Barre d'outils : A/B, bypass, auto gain, duree d'analyse
    juce::TextButton slotA { "A" }, slotB { "B" }, copyAB { "A > B" };
    juce::ToggleButton bypassButton { "BYPASS (original)" }, autoGainButton { "AUTO GAIN" };
    juce::Label autoGainValue, anaLabel;
    juce::ComboBox anaLength;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> anaLengthAttachment;

    // Presets
    juce::TextButton savePreset { "Sauver" }, deletePreset { "Suppr." };
    std::unique_ptr<juce::AlertWindow> saveDialog;
    static constexpr int userPresetIdOffset = 1000;

    struct Star { float x, y, r, a; };
    std::vector<Star> stars;

    std::unique_ptr<AnalysePanel> globalAnalyse;
    juce::ComboBox presetBox;
    std::vector<std::unique_ptr<juce::Component>> items;
    std::vector<std::unique_ptr<Section>> sections;
    std::array<std::vector<Section*>, 4> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolarisEditor)
};
