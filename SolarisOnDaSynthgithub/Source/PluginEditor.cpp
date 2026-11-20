#include "PluginEditor.h"
#include <set>

using namespace SolarisColours;

namespace
{
    juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
SolarisLookAndFeel::SolarisLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, space0);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, dim);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, magenta);
    setColour (juce::PopupMenu::backgroundColourId, space1);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet.withAlpha (0.4f));
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

void SolarisLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float lineW = juce::jmax (2.5f, radius * 0.11f);
    const float arcR = radius - lineW;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Piste
    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (knobBody.brighter (0.25f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arc de valeur avec halo (orbite lumineuse)
    juce::Path value;
    value.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
    juce::ColourGradient grad (violet, cx - radius, cy + radius, magenta, cx + radius, cy - radius, false);
    g.setGradientFill (grad);
    g.setOpacity (0.25f);
    g.strokePath (value, juce::PathStrokeType (lineW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setOpacity (1.0f);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Corps : petite planete
    const float bodyR = radius * 0.62f;
    juce::ColourGradient body (knobBody.brighter (0.45f), cx - bodyR * 0.4f, cy - bodyR * 0.5f,
                               space0, cx + bodyR, cy + bodyR, true);
    g.setGradientFill (body);
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (violet.withAlpha (0.35f));
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Satellite (indicateur)
    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
    const float dotR = juce::jmax (2.0f, radius * 0.08f);
    const auto dot = juce::Point<float> (cx, cy) + dir * (bodyR * 0.68f);
    g.setColour (magenta.withAlpha (0.35f));
    g.fillEllipse (dot.x - dotR * 2.0f, dot.y - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
    g.setColour (text);
    g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
}

void SolarisLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    auto pill = r.removeFromLeft (juce::jmin (r.getWidth(), 34.0f)).withSizeKeepingCentre (34.0f, 16.0f);

    g.setColour (on ? violet.withAlpha (0.85f) : knobBody.brighter (0.2f));
    g.fillRoundedRectangle (pill, 8.0f);
    if (on)
    {
        g.setColour (magenta.withAlpha (0.25f));
        g.drawRoundedRectangle (pill.expanded (2.0f), 10.0f, 2.0f);
    }
    const float knobX = on ? pill.getRight() - 14.0f : pill.getX() + 2.0f;
    g.setColour (on ? text : dim);
    g.fillEllipse (knobX, pill.getY() + 2.0f, 12.0f, 12.0f);

    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (highlighted ? text : (on ? text : dim));
        g.setFont (font (12.0f));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
    }
}

void SolarisLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (space1.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (border.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (magenta);
    g.fillPath (arrow);
}

void SolarisLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    if ((bool) b.getProperties().getWithDefault ("flat", false))
    {
        const bool on = b.getToggleState();
        g.setColour (on ? violet.withAlpha (0.85f) : space1.withAlpha (highlighted ? 1.0f : 0.85f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? magenta : border.withAlpha (down ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        return;
    }
    juce::ColourGradient grad (violet, r.getX(), r.getCentreY(), magenta, r.getRight(), r.getCentreY(), false);
    g.setColour (magenta.withAlpha (highlighted ? 0.35f : 0.2f));
    g.fillRoundedRectangle (r.expanded (2.0f), r.getHeight() * 0.5f + 2.0f);
    g.setGradientFill (grad);
    g.setOpacity (down ? 0.75f : 1.0f);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setOpacity (1.0f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    addAndMakeVisible (slider);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (10.5f));
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (10.5f));
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void ChoiceBox::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    box.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 120), 26));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
{
    button.setButtonText (name);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

void Toggle::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    button.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 110), 24));
}

SyncSelector::SyncSelector (juce::AudioProcessorValueTreeState& s, const juce::String& toggleId,
                            const juce::String& choiceId, const juce::String& name)
{
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (10.5f));
    addAndMakeVisible (label);
    addAndMakeVisible (sync);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (choiceId)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, toggleId, sync);
    boxAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, choiceId, box);
}

void SyncSelector::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    r.removeFromTop (4);
    sync.setBounds (r.removeFromTop (24).withSizeKeepingCentre (juce::jmin (r.getWidth(), 110), 24));
    r.removeFromTop (6);
    box.setBounds (r.removeFromTop (26).withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 132), 26));
}

//==============================================================================
AnalysePanel::AnalysePanel (SolarisProcessor& p, int s, bool h) : proc (p), scope (s), horizontal (h)
{
    button.setButtonText (scope == 0 ? "ANALYSER LE SYNTHE" : "ANALYSER");
    button.onClick = [this]
    {
        if (proc.getAnalysisState() == SolarisProcessor::analysisRunning)
        {
            proc.cancelAnalysis();
            startedHere = false;
            status.setText ("Analyse annulee", juce::dontSendNotification);
        }
        else
        {
            proc.startAnalysis (scope);
            startedHere = true;
        }
    };
    addAndMakeVisible (button);

    status.setJustificationType (horizontal ? juce::Justification::centredLeft : juce::Justification::centred);
    status.setFont (font (11.0f));
    status.setColour (juce::Label::textColourId, lilac);
    status.setMinimumHorizontalScale (0.6f);
    status.setText (scope == 0 ? "Lance la lecture du synthe puis clique : tout se regle" : "Regle le comp",
                    juce::dontSendNotification);
    addAndMakeVisible (status);
    startTimerHz (10);
}

void AnalysePanel::resized()
{
    auto r = getLocalBounds();
    if (horizontal)
    {
        button.setBounds (r.removeFromLeft (220).withSizeKeepingCentre (220, 34));
        status.setBounds (r.withTrimmedLeft (10));
    }
    else
    {
        r.removeFromTop (14);
        button.setBounds (r.removeFromTop (30).reduced (4, 0));
        status.setBounds (r.reduced (0, 2));
    }
}

void AnalysePanel::timerCallback()
{
    const int state = proc.getAnalysisState();
    const bool mine = proc.getAnalysisScope() == scope;

    if (state == SolarisProcessor::analysisRunning && mine)
    {
        button.setButtonText ("ANNULER");
        const float p = proc.getAnalysisProgress();
        status.setText (p <= 0.0f ? "En attente du son... lance la lecture"
                                  : "Ecoute du synthe... " + juce::String (juce::roundToInt (p * 100.0f)) + " %",
                        juce::dontSendNotification);
    }
    else
    {
        button.setButtonText (scope == 0 ? "ANALYSER LE SYNTHE" : "ANALYSER");
        // Celui qui a lance l'analyse applique le resultat (le global le fait aussi apres reouverture)
        if (state == SolarisProcessor::analysisDone && mine && (startedHere || scope == 0))
        {
            status.setText (proc.applyAnalysisResult(), juce::dontSendNotification);
            startedHere = false;
        }
    }
}

//==============================================================================
GainReductionMeter::GainReductionMeter (SolarisProcessor& p) : proc (p) { startTimerHz (30); }

void GainReductionMeter::timerCallback()
{
    const float t = proc.getGainReductionDb();
    displayDb = t > displayDb ? t : displayDb + (t - displayDb) * 0.15f;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (dim);
    g.setFont (font (10.5f));
    g.drawText ("REDUCTION", r.removeFromTop (14), juce::Justification::centred);
    auto txt = r.removeFromBottom (16);
    auto bar = r.withSizeKeepingCentre (12, r.getHeight() - 6).toFloat();
    g.setColour (knobBody);
    g.fillRoundedRectangle (bar, 6.0f);
    const float f = juce::jlimit (0.0f, 1.0f, displayDb / 12.0f);
    juce::ColourGradient grad (violet, bar.getX(), bar.getY(), magenta, bar.getX(), bar.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * f), 6.0f);
    g.setColour (text);
    g.drawText ("-" + juce::String (displayDb, 1) + " dB", txt, juce::Justification::centred);
}

//==============================================================================
LevelMeter::LevelMeter (SolarisProcessor& p, bool isInput) : proc (p), input (isInput) { startTimerHz (30); }

void LevelMeter::timerCallback()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        const float peak = input ? proc.takeInputPeak (ch) : proc.takeOutputPeak (ch);
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        level[ch] = db > level[ch] ? db : juce::jmax (-60.0f, level[ch] - 1.2f);   // retombee ~36 dB/s
        if (db >= hold[ch]) { hold[ch] = db; holdFrames[ch] = 45; }
        else if (--holdFrames[ch] <= 0) hold[ch] = juce::jmax (-60.0f, hold[ch] - 1.5f);
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (dim);
    g.setFont (font (10.5f));
    g.drawText (input ? "IN" : "OUT", r.removeFromTop (14), juce::Justification::centred);
    auto txt = r.removeFromBottom (16);
    auto area = r.withSizeKeepingCentre (26, r.getHeight() - 6).toFloat();

    auto toY = [&] (float db) { return area.getBottom() - area.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };

    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = area.withWidth (11.0f).withX (area.getX() + (float) ch * 15.0f);
        g.setColour (knobBody);
        g.fillRoundedRectangle (bar, 3.0f);
        const float top = toY (level[ch]);
        juce::ColourGradient grad (violet, 0.0f, bar.getBottom(), magenta, 0.0f, toY (-6.0f), false);
        grad.addColour (1.0, juce::Colour (0xfffb7185));   // rose-rouge au-dessus de -6 dB
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar.withTop (top), 3.0f);
        g.setColour (hold[ch] > -1.0f ? juce::Colour (0xfffb7185) : text);
        g.fillRect (bar.getX(), toY (hold[ch]) - 1.0f, bar.getWidth(), 2.0f);
    }
    // graduations
    g.setColour (dim.withAlpha (0.5f));
    for (float db : { -6.0f, -12.0f, -24.0f, -48.0f })
        g.fillRect (area.getRight() + 2.0f, toY (db), 4.0f, 1.0f);

    const float maxHold = juce::jmax (hold[0], hold[1]);
    g.setColour (maxHold > -1.0f ? juce::Colour (0xfffb7185) : text);
    g.drawText (maxHold <= -59.9f ? "-inf" : juce::String (maxHold, 1), txt, juce::Justification::centred);
}

//==============================================================================
ReverbReadout::ReverbReadout (SolarisProcessor& p) : proc (p) { startTimerHz (8); }

void ReverbReadout::timerCallback()
{
    const float d = proc.getDecaySec(), p = proc.getPredelayMs(), b = proc.getBpm();
    if (d != decay || p != pre || b != bpm)
    {
        decay = d; pre = p; bpm = b;
        repaint();
    }
}

void ReverbReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (4).toFloat();
    g.setColour (space0.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (border.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    auto area = r.reduced (10.0f, 6.0f);
    const float rowH = area.getHeight() / 3.0f;
    auto line = [&] (const juce::String& k, const juce::String& v)
    {
        auto row = area.removeFromTop (rowH);
        g.setColour (dim);
        g.setFont (font (10.5f));
        g.drawText (k, row.removeFromLeft (70.0f), juce::Justification::centredLeft);
        g.setColour (lilac);
        g.setFont (font (16.0f, true));
        g.drawText (v, row, juce::Justification::centredRight);
    };
    line ("DECAY", juce::String (decay, 2) + " s");
    line ("PREDELAY", juce::String (pre, 2) + " ms");
    line ("TEMPO", juce::String (bpm, 1) + " BPM");
}

//==============================================================================
Section::Section (const juce::String& t, juce::AudioProcessorValueTreeState& s, const juce::String& powerId)
    : title (t)
{
    if (powerId.isNotEmpty())
    {
        hasPower = true;
        addAndMakeVisible (power);
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, powerId, power);
        lastState = power.getToggleState();
        startTimerHz (15);
    }
}

void Section::addItem (juce::Component* item, int weight)
{
    items.push_back (item);
    weights.push_back (weight);
    totalWeight += weight;
    addAndMakeVisible (item);
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 12.0f);
    juce::ColourGradient edge (border.withAlpha (0.8f), r.getX(), r.getY(), magenta.withAlpha (0.15f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);

    // Titre + petite etoile
    auto head = getLocalBounds().reduced (14, 8).removeFromTop (20);
    g.setColour (magenta);
    g.fillEllipse ((float) head.getX(), (float) head.getCentreY() - 2.5f, 5.0f, 5.0f);
    g.setColour (lilac);
    g.setFont (font (12.5f, true));
    g.drawText (title, head.withTrimmedLeft (11), juce::Justification::centredLeft);
}

void Section::paintOverChildren (juce::Graphics& g)
{
    if (hasPower && ! power.getToggleState())
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop (34.0f);
        g.setColour (space0.withAlpha (0.55f));
        g.fillRoundedRectangle (r.reduced (1.0f), 12.0f);
    }
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto header = r.removeFromTop (24);
    if (hasPower)
        power.setBounds (header.removeFromRight (40));
    if (items.empty())
        return;

    const int full = r.getWidth();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int w = full * weights[i] / juce::jmax (1, totalWeight);
        auto cell = (i == items.size() - 1) ? r : r.removeFromLeft (w);
        items[i]->setBounds (cell.reduced (2));
    }
}

void Section::timerCallback()
{
    const bool s = power.getToggleState();
    if (s != lastState) { lastState = s; repaint(); }
}

//==============================================================================
SolarisEditor::SolarisEditor (SolarisProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);
    auto& s = processorRef.apvts;

    // Champ d'etoiles fixe (meme ciel a chaque ouverture)
    juce::Random rng (2026);
    for (int i = 0; i < 260; ++i)
        stars.push_back ({ rng.nextFloat(), rng.nextFloat(), 0.4f + rng.nextFloat() * 1.3f, 0.15f + rng.nextFloat() * 0.7f });

    auto add = [this] (std::unique_ptr<juce::Component> c) { items.push_back (std::move (c)); return items.back().get(); };
    auto knob   = [&] (const char* id, const char* n) { return add (std::make_unique<Knob> (s, id, n)); };
    auto choice = [&] (const char* id, const char* n) { return add (std::make_unique<ChoiceBox> (s, id, n)); };
    auto toggle = [&] (const char* id, const char* n) { return add (std::make_unique<Toggle> (s, id, n)); };
    auto syncSel = [&] (const char* t, const char* c, const char* n) { return add (std::make_unique<SyncSelector> (s, t, c, n)); };

    std::set<juce::Component*> wideItems;
    auto wide = [&wideItems] (juce::Component* c) { wideItems.insert (c); return c; };

    auto section = [&] (const char* title, const char* powerId, int row, std::initializer_list<juce::Component*> comps)
    {
        auto sec = std::make_unique<Section> (title, s, powerId);
        for (auto* c : comps)
            sec->addItem (c, wideItems.count (c) > 0 ? 2 : 1);
        addAndMakeVisible (*sec);
        rows[(size_t) row].push_back (sec.get());
        sections.push_back (std::move (sec));
    };

    // Rangee 1
    section ("COUPE-BAS", "hpfOn", 0, { wide (knob ("hpfFreq", "Frequence")) });
    section ("EQ ANALOGIQUE", "eqOn", 0, { choice ("eqMode", "Mode M/S"), knob ("lowFreq", "Grave Hz"), knob ("lowGain", "Grave"),
                                           knob ("midFreq", "Medium Hz"), knob ("midGain", "Medium"), knob ("midQ", "Q"),
                                           knob ("highFreq", "Aigu Hz"), knob ("highGain", "Aigu"), knob ("eqColour", "Couleur") });
    auto* meterIn  = add (std::make_unique<LevelMeter> (processorRef, true));
    auto* meterOut = add (std::make_unique<LevelMeter> (processorRef, false));
    section ("NIVEAUX", "", 0, { meterIn, knob ("inGain", "Entree"), knob ("outGain", "Sortie"), meterOut });

    // Rangee 2
    auto* compAnalyse = add (std::make_unique<AnalysePanel> (processorRef, 1, false));
    auto* grMeter = add (std::make_unique<GainReductionMeter> (processorRef));
    section ("COMPRESSION", "compOn", 1, { choice ("compSource", "Source"), wide (compAnalyse),
                                           knob ("compThresh", "Seuil"), knob ("compRatio", "Ratio"),
                                           knob ("compAttack", "Attaque"), knob ("compRelease", "Release"),
                                           knob ("compScHpf", "Clarte"), knob ("compMix", "Mix //"),
                                           knob ("compMakeup", "Makeup"), grMeter });

    // Rangee 3
    section ("SATURATION", "satOn", 2, { choice ("satType", "Type"), knob ("satDrive", "Drive"),
                                         knob ("satMix", "Chaleur"), knob ("satBright", "Brillance"),
                                         knob ("satBrightFreq", "Brillance Hz") });
    section ("STEREO", "widthOn", 2, { knob ("width", "Largeur"), knob ("monoFreq", "Basses mono") });
    section ("DELAY", "delayOn", 2, { choice ("delaySync", "Note"), knob ("delayFb", "Feedback"),
                                      knob ("delayMix", "Niveau"), toggle ("delayPing", "Ping-pong") });

    // Rangee 4 : reverb
    auto* readout = add (std::make_unique<ReverbReadout> (processorRef));
    section ("REVERB SPATIALE", "revOn", 3, { choice ("revMode", "Mode"),
                                              knob ("revDecay", "Decay"), syncSel ("revDecaySync", "revDecayNote", "Decay sync"),
                                              knob ("revPredelay", "Predelay"), syncSel ("revPreSync", "revPreNote", "Predelay sync"),
                                              knob ("revLowCut", "Low cut"), knob ("revHighCut", "High cut"),
                                              knob ("revMod", "Modulation"), knob ("revMix", "Niveau"), wide (readout) });

    globalAnalyse = std::make_unique<AnalysePanel> (processorRef, 0, true);
    addAndMakeVisible (*globalAnalyse);

    // Presets usine + utilisateur
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userPresetIdOffset) processorRef.applyPreset (id - 1);
        else processorRef.loadUserPreset (presetBox.getText());
    };
    addAndMakeVisible (presetBox);
    refreshPresetList();
    for (auto* b : { &savePreset, &deletePreset })
    {
        b->getProperties().set ("flat", true);
        addAndMakeVisible (*b);
    }
    savePreset.onClick = [this] { promptSavePreset(); };
    deletePreset.onClick = [this] { deleteSelectedPreset(); };

    // A/B
    for (auto* b : { &slotA, &slotB, &copyAB })
    {
        b->getProperties().set ("flat", true);
        addAndMakeVisible (*b);
    }
    slotA.onClick = [this] { processorRef.selectSlot (0); };
    slotB.onClick = [this] { processorRef.selectSlot (1); };
    copyAB.onClick = [this] { processorRef.copyCurrentToOtherSlot(); };

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (autoGainButton);
    bypassAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "bypass", bypassButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "autoGain", autoGainButton);

    autoGainValue.setFont (font (12.0f, true));
    autoGainValue.setColour (juce::Label::textColourId, lilac);
    addAndMakeVisible (autoGainValue);

    anaLabel.setText ("DUREE D'ANALYSE", juce::dontSendNotification);
    anaLabel.setFont (font (10.5f));
    anaLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (anaLabel);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter ("anaLength")))
        anaLength.addItemList (c->choices, 1);
    addAndMakeVisible (anaLength);
    anaLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, "anaLength", anaLength);

    startTimerHz (10);
    setSize (1200, 920);
}

SolarisEditor::~SolarisEditor()
{
    if (saveDialog != nullptr)
        saveDialog->exitModalState (0);
    setLookAndFeel (nullptr);
}

void SolarisEditor::timerCallback()
{
    const int slot = processorRef.getCurrentSlot();
    slotA.setToggleState (slot == 0, juce::dontSendNotification);
    slotB.setToggleState (slot == 1, juce::dontSendNotification);
    copyAB.setButtonText (slot == 0 ? "A > B" : "B > A");

    const float ag = processorRef.getAutoGainDb();
    autoGainValue.setText (autoGainButton.getToggleState() ? juce::String (ag >= 0.0f ? "+" : "") + juce::String (ag, 1) + " dB" : "off",
                           juce::dontSendNotification);
}

void SolarisEditor::refreshPresetList (const juce::String& selectUserPreset)
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Presets usine");
    const auto factory = SolarisProcessor::getPresetNames();
    for (int i = 0; i < factory.size(); ++i)
        presetBox.addItem (factory[i], i + 1);

    const auto user = SolarisProcessor::getUserPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("Mes presets");
        for (int i = 0; i < user.size(); ++i)
        {
            presetBox.addItem (user[i], userPresetIdOffset + i);
            if (user[i] == selectUserPreset)
                presetBox.setSelectedId (userPresetIdOffset + i, juce::dontSendNotification);
        }
    }
}

void SolarisEditor::promptSavePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Sauver le preset", "Nom du preset :", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", presetBox.getSelectedId() >= userPresetIdOffset ? presetBox.getText() : "Mon son");
    saveDialog->addButton ("Sauver", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<SolarisEditor> safe (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr)
            return;
        const auto name = juce::File::createLegalFileName (safe->saveDialog->getTextEditorContents ("name").trim());
        safe->saveDialog.reset();
        if (result == 1 && name.isNotEmpty())
        {
            if (safe->processorRef.saveUserPreset (name))
                safe->refreshPresetList (name);
            else
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle ("Erreur").withMessage ("Impossible d'enregistrer le preset.")
                                                  .withButton ("OK"), nullptr);
        }
    }), false);
}

void SolarisEditor::deleteSelectedPreset()
{
    if (presetBox.getSelectedId() < userPresetIdOffset)
        return;   // les presets usine ne se suppriment pas

    const auto name = presetBox.getText();
    juce::Component::SafePointer<SolarisEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Supprimer le preset").withMessage ("Supprimer \"" + name + "\" ?")
                                      .withButton ("Supprimer").withButton ("Annuler"),
                                  [safe, name] (int result)
                                  {
                                      if (result == 1 && SolarisProcessor::deleteUserPreset (name) && safe != nullptr)
                                          safe->refreshPresetList();
                                  });
}

void SolarisEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Fond : nuit profonde
    g.setGradientFill (juce::ColourGradient (space1, b.getCentreX(), 0.0f, space0, b.getCentreX(), b.getBottom(), false));
    g.fillAll();

    // Nebuleuses
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x557c3aed), b.getWidth() * 0.82f, b.getHeight() * 0.12f,
                                             juce::Colour (0x007c3aed), b.getWidth() * 0.82f + 420.0f, b.getHeight() * 0.12f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x40c026d3), b.getWidth() * 0.12f, b.getHeight() * 0.85f,
                                             juce::Colour (0x00c026d3), b.getWidth() * 0.12f + 480.0f, b.getHeight() * 0.85f, true));
    g.fillAll();

    // Etoiles
    for (auto& st : stars)
    {
        g.setColour (juce::Colours::white.withAlpha (st.a));
        g.fillEllipse (st.x * b.getWidth(), st.y * b.getHeight(), st.r, st.r);
    }

    // Logo : planete a anneau
    const float px = 42.0f, py = 40.0f, pr = 17.0f;
    g.setColour (magenta.withAlpha (0.25f));
    g.fillEllipse (px - pr * 1.6f, py - pr * 1.6f, pr * 3.2f, pr * 3.2f);
    g.setGradientFill (juce::ColourGradient (magenta, px - pr * 0.5f, py - pr * 0.6f, juce::Colour (0xff4c1d95), px + pr, py + pr, true));
    g.fillEllipse (px - pr, py - pr, pr * 2.0f, pr * 2.0f);
    juce::Path ring;
    ring.addEllipse (px - pr * 1.9f, py - pr * 0.45f, pr * 3.8f, pr * 0.9f);
    ring.applyTransform (juce::AffineTransform::rotation (-0.35f, px, py));
    g.setColour (lilac.withAlpha (0.85f));
    g.strokePath (ring, juce::PathStrokeType (2.0f));

    // Titre
    g.setColour (text);
    g.setFont (font (28.0f, true));
    g.drawText ("SolarisOnDaSynth", 76, 14, 330, 34, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText ("SPACE MIX ENGINE  /  EQ - SATU - COMP - STEREO - DELAY - REVERB", 78, 46, 460, 16, juce::Justification::centredLeft);
}

void SolarisEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto header = area.removeFromTop (64);
    auto presetArea = header.removeFromRight (330).withSizeKeepingCentre (330, 30);
    deletePreset.setBounds (presetArea.removeFromRight (58));
    presetArea.removeFromRight (4);
    savePreset.setBounds (presetArea.removeFromRight (64));
    presetArea.removeFromRight (6);
    presetBox.setBounds (presetArea);
    header.removeFromLeft (440);
    globalAnalyse->setBounds (header.reduced (10, 10));

    auto bar = area.removeFromTop (40).reduced (5, 6);
    slotA.setBounds (bar.removeFromLeft (36));
    bar.removeFromLeft (4);
    slotB.setBounds (bar.removeFromLeft (36));
    bar.removeFromLeft (6);
    copyAB.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (24);
    bypassButton.setBounds (bar.removeFromLeft (170));
    bar.removeFromLeft (16);
    autoGainButton.setBounds (bar.removeFromLeft (120));
    autoGainValue.setBounds (bar.removeFromLeft (80));
    anaLength.setBounds (bar.removeFromRight (90));
    bar.removeFromRight (8);
    anaLabel.setBounds (bar.removeFromRight (130));

    const int rowH = area.getHeight() / (int) rows.size();
    for (auto& row : rows)
    {
        auto r = area.removeFromTop (rowH);
        int total = 0;
        for (auto* sec : row) total += juce::jmax (1, sec->getTotalWeight());
        const int full = r.getWidth();
        for (size_t i = 0; i < row.size(); ++i)
        {
            const int w = (i == row.size() - 1) ? r.getWidth() : full * juce::jmax (1, row[i]->getTotalWeight()) / total;
            row[i]->setBounds (r.removeFromLeft (w).reduced (5));
        }
    }
}
