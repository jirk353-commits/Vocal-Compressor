#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace vcui
{
using APVTS = juce::AudioProcessorValueTreeState;

namespace col
{
    const juce::Colour panelTop  { 0xfff1f0ec };
    const juce::Colour panelBot  { 0xffdcdad4 };
    const juce::Colour label     { 0xff5b5e63 };
    const juce::Colour value     { 0xff1d1f22 };
    const juce::Colour orange    { 0xfff39c12 };
    const juce::Colour orangeHi  { 0xffffb83d };
    const juce::Colour navy      { 0xff1b2d48 };
    const juce::Colour screen    { 0xff1e3048 };
    const juce::Colour grid      { 0xff3a4f6c };
    const juce::Colour blue      { 0xff1f8fd6 };   // upward / gain
    const juce::Colour gold      { 0xffe6a51c };   // compression
    const juce::Colour red       { 0xff962a2a };   // VOX total compression
    const juce::Colour compTitle { 0xffc98a1e };
    const juce::Colour voxTitle  { 0xffa52a2a };
}

juce::Font uiFont (float size, bool bold = true);

/** Base for every draggable control: relative drag, double-click reset, wheel, host gestures. */
class ParamDrag : public juce::Component
{
public:
    ParamDrag (juce::RangedAudioParameter&, bool vertical);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<float (float)> normToAxis, axisToNorm;   // axis 0..1 = top..bottom (vertical) or left..right
    std::function<void (float, float)> onUserChange;       // (old, new) real value while the user drags
    float getNorm() const { return norm; }
    float getValue() const { return param.convertFrom0to1 (norm); }
    juce::String getText() const { return param.getText (norm, 8); }
    juce::RangedAudioParameter& param;
protected:
    virtual juce::Rectangle<float> travel() const { return getLocalBounds().toFloat(); }
    float axis() const { return normToAxis (norm); }
    bool vertical;
private:
    void setFromNorm (float n);
    float norm = 0, startAxis = 0;
    std::unique_ptr<juce::ParameterAttachment> att;
};

/** Vertical meter (1 or 2 bars) with a fader handle superimposed. */
class MeterFader : public ParamDrag
{
public:
    MeterFader (juce::RangedAudioParameter&, float rangeDb, int bars, bool showScale);
    void paint (juce::Graphics&) override;
    void setLevels (float a, float b) { if (std::abs (a - lv[0]) > 0.05f || std::abs (b - lv[1]) > 0.05f) { lv[0] = a; lv[1] = b; repaint(); } }
protected:
    juce::Rectangle<float> travel() const override;
private:
    juce::Rectangle<float> barArea() const;
    float range; int bars; bool scale;
    float lv[2] { -100, -100 };
};

/** The big centre box: fader + gain-change display. */
class BoxFader : public ParamDrag
{
public:
    enum Mode { compMode, voxMode };
    BoxFader (juce::RangedAudioParameter&, Mode, const juce::String& title);
    void paint (juce::Graphics&) override;
    void setChange (float db, float peakHold) { if (std::abs (db - change) > 0.02f || peakHold != peak) { change = db; peak = peakHold; repaint(); } }
    juce::Rectangle<float> screenArea() const;
    std::function<void()> onPeakClick;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
protected:
    juce::Rectangle<float> travel() const override { return screenArea(); }
private:
    float dbToAxis (float db) const;
    Mode mode; juce::String title;
    float change = 0, peak = 0; bool peakClick = false;
};

class HSlider : public ParamDrag
{
public:
    HSlider (juce::RangedAudioParameter&, const juce::String& label);
    void paint (juce::Graphics&) override;
protected:
    juce::Rectangle<float> travel() const override;
private:
    juce::String label;
};

class Knob : public ParamDrag
{
public:
    Knob (juce::RangedAudioParameter&, const juce::String& label, const juce::String& minText, const juce::String& maxText);
    void paint (juce::Graphics&) override;
private:
    juce::String label, minT, maxT;
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

/** Pill button whose text shows the current choice; click flips between the two choices. */
class ChoicePill : public juce::TextButton
{
public:
    explicit ChoicePill (juce::RangedAudioParameter&);
private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
};

class OnPill : public juce::TextButton
{
public:
    OnPill (APVTS&, const juce::String& id, const juce::String& text);
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

/** Peak-hold helper for the readouts. */
struct Hold
{
    float v = -100, t = 0;
    float update (float x, float decayFloor = -100) { if (x >= v || t > 60) { v = x; t = 0; } else ++t; return std::max (v, decayFloor); }
    void reset() { v = -100; t = 0; }
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (VocalCompProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    static constexpr int W = 1020, H = 700;
private:
    void timerCallback() override;
    void renderTexture();
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, const juce::String& sub, juce::Colour titleCol, bool on);
    void column (juce::Graphics&, int x, int w, int y0, const juce::String& top, const juce::String& topValue, const juce::String& bottom, const juce::String& bottomValues);

    VocalCompProcessor& proc;
    LookAndFeel lnf;
    juce::Image texture;
    MeterFader cThresh, cGain, vGate, vGain;
    BoxFader cRatio, vComp;
    HSlider cAttack, cRelease;
    Knob cMix, cTrim;
    ChoicePill cArc, cBehav, cChar, order;
    OnPill cOn, vOn, vLink;
    juce::ComboBox presetBox;
    Hold hIn[2], hOut[2], hVOut[2], hEnergy, hVPeak;
    float cPeak = 0; int cPeakT = 0;
    float cChangeSm = 0, vGrSm = 0;
    float shown[12] {};
    juce::String readout[8];
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
