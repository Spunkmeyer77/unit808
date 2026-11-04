#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Layout.h"

namespace unit808ui
{
juce::Image image (const Art& a);                 // the 2x PNG of a piece of artwork
const Ctrl* find (const char* id);                // a control in the layout table
void drawArt (juce::Graphics& g, const Art& a, const juce::AffineTransform& t = {});

// A knob from the artwork: static base, the cap turns between minAngle and maxAngle.
class Knob : public juce::Slider
{
public:
    Knob (const Ctrl& c, float minAngle, float maxAngle);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    const Ctrl& ctrl;
private:
    float minA, maxA;
    juce::Image base, cap;
};

// Selector knobs (MODE, INSTRUMENT-SELECT, AUTO FILL IN): snap to the printed positions.
// Click or drag towards a label to turn it there.
class Selector : public juce::Component
{
public:
    Selector (const Ctrl& c, juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (! labelClick) pickAt (e.position); }
    void mouseDoubleClick (const juce::MouseEvent&) override {}
    // printed position labels (panel coordinates): Alt / Shift / right-click on one calls onLabel (mute / solo)
    std::vector<juce::Rectangle<float>> labels;
    std::function<void (int index, juce::ModifierKeys)> onLabel;
    std::function<int (int index)> labelState;   // 0 normal, 1 muted (dimmed), 2 soloed (red glow)
    const Ctrl& ctrl;
    int getIndex() const { return index; }
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
    juce::Image base, cap;
    int index = 0;
    bool labelClick = false;
    void pickAt (juce::Point<float> p);
};

// An instrument name window on a sound channel: clicking it selects that instrument for writing
class ChannelLabel : public juce::Component
{
public:
    ChannelLabel (juce::Rectangle<float> r, int inst) : instrument (inst) { setBounds (r.expanded (3.0f).getSmallestIntegerContainer());
                                                                             setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick (instrument); }
    std::function<void (int)> onClick;
    std::function<bool (int)> isSelected;
    const int instrument;
};

// Toggle switches and levers: one image per position
class Switch : public juce::Component
{
public:
    // imageForValue[v] = which position image shows parameter value v
    Switch (const Ctrl& c, juce::RangedAudioParameter& p, bool horizontal, std::vector<int> imageForValue);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    const Ctrl& ctrl;
private:
    juce::ParameterAttachment att;
    juce::RangedAudioParameter& param;
    std::vector<juce::Image> imgs;
    std::vector<int> imageFor;
    int value = 0;
    bool horizontal;
    void setValue (int v);
};

// A key from the artwork (up / down images); optional LED drawn on top
class Key : public juce::Component
{
public:
    explicit Key (const Ctrl& c, const Ctrl* led = nullptr);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { down = true; repaint(); if (onPress) onPress (e.mods); }
    void mouseDrag (const juce::MouseEvent& e) override { if (onDrag) onDrag (e.getEventRelativeTo (getParentComponent()).position); }
    void mouseUp (const juce::MouseEvent&) override { down = false; repaint(); if (onRelease) onRelease(); }
    std::function<void (juce::ModifierKeys)> onPress;
    std::function<void (juce::Point<float>)> onDrag;
    std::function<void()> onRelease;
    std::function<int()> ledState;       // 0 off, 1 red, 2 blue (flam)
    std::function<bool()> heldDown;
    const Ctrl& ctrl;
private:
    const Ctrl* led;
    juce::Image up, dn, ledOff, ledOnImg, ledBlue;
    bool down = false;
};

class AboutOverlay : public juce::Component
{
public:
    AboutOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { setVisible (false); }
    bool keyPressed (const juce::KeyPress&) override { setVisible (false); return true; }
    static juce::File writeManual();
private:
    juce::TextButton manualBtn { "OPEN THE MANUAL" };
    juce::HyperlinkButton mail { "ticcoross@gmail.com", juce::URL ("mailto:ticcoross@gmail.com") };
    juce::HyperlinkButton shop { "ticcoross.gumroad.com", juce::URL ("https://ticcoross.gumroad.com/") };
    juce::Image logo;
    juce::Rectangle<float> card() const;
};
} // namespace unit808ui

class Unit808Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Unit808Editor (Unit808Processor&);
    ~Unit808Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Unit808Processor& proc;

    struct Content : public juce::Component
    {
        explicit Content (Unit808Editor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintPanel (g); }
        void mouseUp (const juce::MouseEvent& e) override;
        Unit808Editor& ed;
    } content { *this };

    juce::Image faceplate;
    std::vector<std::unique_ptr<unit808ui::Knob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAtts;
    std::vector<std::unique_ptr<unit808ui::Selector>> selectors;
    std::vector<std::unique_ptr<unit808ui::Switch>> switches;
    std::vector<std::unique_ptr<unit808ui::Key>> stepKeys;
    std::unique_ptr<unit808ui::Key> startStop, tap, clear;
    unit808ui::AboutOverlay about;
    std::vector<std::unique_ptr<unit808ui::ChannelLabel>> channelLabels;
    struct Led { const unit808ui::Ctrl* c; juce::Image off, on; };
    Led varA, varB, part1, part2;
    int lastUi = -1, lastMuteSolo = -1;
    int paintValue = -1;
    uint32_t paintedKeys = 0;

    void paintPanel (juce::Graphics&);
    void timerCallback() override;
    int stepLed (int s) const;
    void instrumentLabelClick (int inst, juce::ModifierKeys m);
    void toggleParam (const juce::String& id);
    void stepKeyDragTo (juce::Point<float>);
    void startStopMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Unit808Editor)
};
