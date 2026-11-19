#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Rebel.h"

class ChopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChopLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hi, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hi, bool down) override;
    juce::Image rustDisc;
};

// Segmented row of chunky rusty buttons bound to a choice parameter (MODE / SHAPE / QUALITY)
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    Selector (juce::RangedAudioParameter& p, juce::StringArray labels, juce::StringArray subLabels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    int getIndex() const { return index; }
    std::function<void()> onChange;
private:
    int segmentAt (juce::Point<int>) const;
    juce::StringArray labels, subs;
    juce::ParameterAttachment att;
    int index = 0, hover = -1;
};

// The CHOP SHOP saw blade: spins harder the more it chops
class SawBlade : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void advance (float chopDb, float dt);
private:
    float angle = 0.0f, heat = 0.0f;
};

// Transfer curve of the current mode/shape/color, with the live drive level marked
class CurveDisplay : public juce::Component
{
public:
    explicit CurveDisplay (ChopShopProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void setLevel (float drive) { level = drive; }
private:
    ChopShopProcessor& proc;
    float level = 0.0f;
};

// IN / CHOP / OUT meters
class MeterPanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float in = 0, out = 0, chop = 0, hold = 0;
    bool scraps = false;
};

// In-window modal dialog (message, confirm, or text entry), drawn over the editor
class ChopDialog : public juce::Component
{
public:
    ChopDialog (const juce::String& title, const juce::String& message, bool withTextField,
                const juce::String& initialText, juce::StringArray buttons,
                std::function<void (int buttonIndex, const juce::String& text)> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}
private:
    void finish (int index);
    juce::String title, message;
    juce::TextEditor field;
    bool hasField;
    juce::OwnedArray<juce::TextButton> buttons;
    std::function<void (int, const juce::String&)> onClose;
    juce::Rectangle<int> panel;
};

class ChopShopEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChopShopEditor (ChopShopProcessor&);
    ~ChopShopEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // used by the headless snapshot test
    juce::TextButton& getSaveButton() { return saveButton; }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Plate { juce::String title; juce::Rectangle<int> bounds; bool hot = false; };

    Knob& makeKnob (const juce::String& id, const juce::String& text);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void timerCallback() override;
    void buildBackdrop();

    void refreshPresetBox();
    void stepPreset (int delta);
    void promptSave();
    void confirmDelete();
    void showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                     juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult);
    void showMessage (const juce::String& title, const juce::String& text);

    ChopShopProcessor& proc;
    ChopLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *inputKnob = nullptr, *ceilKnob = nullptr, *colorKnob = nullptr, *mixKnob = nullptr, *outKnob = nullptr;
    Selector modeSel, shapeSel, osSel;
    juce::ToggleButton gainMatchButton { "GAIN MATCH" }, scrapsButton { "SCRAPS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gainMatchAtt, scrapsAtt;
    SawBlade saw;
    juce::TooltipWindow tooltips { this, 700 };
    CurveDisplay curve;
    MeterPanel meters;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    juce::Array<juce::File> userFiles;
    int lastPresetVersion = -1;
    std::unique_ptr<ChopDialog> dialog;
    static constexpr int kUserIdBase = 1001;

    std::vector<Plate> plates;
    juce::Rectangle<int> logoArea, badgeArea, crestArea, meterArea, presetArea, stripeArea, linkArea;
    int linkTick = 0;
    juce::Image backdrop, logoImage;
    float inMeter = 0, outMeter = 0, chopMeter = 0, chopHold = 0, flicker = 0;
    int holdTimer = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopShopEditor)
    cdrebel::Button rebel { proc.apvts };   // REBEL: The Master hands off
};
