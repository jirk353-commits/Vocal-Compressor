#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace vcui
{
using APVTS = juce::AudioProcessorValueTreeState;

namespace col
{
    const juce::Colour plateTop { 0xff3d4249 };
    const juce::Colour plateBot { 0xff1f2226 };
    const juce::Colour text     { 0xfff4f1ea };
    const juce::Colour textDim  { 0xffc5cad1 };
    const juce::Colour gold     { 0xffffb52e };   // COMP / compression
    const juce::Colour blue     { 0xff47b4ff };   // upward gain
    const juce::Colour red      { 0xffff5a47 };   // VOX
    const juce::Colour mint     { 0xff5ff0b4 };
    const juce::Colour rose     { 0xffff6b8a };
    const juce::Colour lcd      { 0xff0a0d10 };
}

juce::Font uiFont (float size, bool bold = true, float kerning = 0.0f);
void embossed (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, juce::Colour, juce::Justification);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

class Knob : public juce::Component
{
public:
    /** arcFrom: normalised position the value arc starts from (0 = left end, 0.5 = centre). */
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour accent, bool big = false, float arcFrom = 0.0f, const juce::String& suffix = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
    std::function<juce::Colour()> dynamicAccent;
private:
    juce::String title, suffix; bool big;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour led);
    void resized() override;
private:
    void update (int);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
};

class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS&, const juce::String& id, const juce::String& text, juce::Colour led);
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

/** LED-segment bar meters. */
class BarMeter : public juce::Component
{
public:
    enum Kind { level, gainChange, reduction };
    BarMeter (Kind, const juce::String& name, juce::Colour);
    void paint (juce::Graphics&) override;
    void setValue (float v);
    void resetPeak() { peak = 0; peakT = 0; peakLevel = -100; }
    void mouseUp (const juce::MouseEvent&) override { resetPeak(); repaint(); }
    float limiter = 0;     // for level meters: limiter activity (dB) shown as a light
    bool showLimiter = false;
private:
    float toPos (float v) const;
    Kind kind; juce::String name; juce::Colour colour;
    float val = 0, peak = 0, peakLevel = -100; int peakT = 0;
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (VocalCompProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1120, H = 660;
private:
    void timerCallback() override;
    void renderTexture();
    void drawWell (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour accent);

    VocalCompProcessor& proc;
    LookAndFeel lnf;
    juce::Image texture;
    Knob cThresh, cRatio, cGain, cAttack, cRelease, cMix, cTrim, vGate, vComp, vGain;
    ChoiceButtons cArc, cBehav, cChar, order;
    Toggle cOn, vOn, vLink;
    BarMeter cIn, cChange, cOut, vIn, vGr, vOut;
    juce::ComboBox presetBox;
    float lastVoxComp = 0;
};
} // namespace vcui

class VocalCompEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalCompEditor (VocalCompProcessor&);
    void resized() override;
private:
    vcui::MainPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompEditor)
};
