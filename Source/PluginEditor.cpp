#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "MeterBridge.h"
#include "BinaryData.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace
{
const juce::Colour ink (0xffcfc5aa), muted (0xffa59e8c), teal (0xff71bdae),
                   cream (0xffe3d5ad), amber (0xffedb552), dark (0xff090c0d);
constexpr float pi = juce::MathConstants<float>::pi;

juce::Font font (float size, bool bold = false, bool mono = false)
{
    return juce::Font (juce::FontOptions (mono ? juce::Font::getDefaultMonospacedFontName()
                                             : juce::Font::getDefaultSansSerifFontName(),
                                        size, bold ? juce::Font::bold : juce::Font::plain));
}

void drawLabel (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
           float size, juce::Colour colour = ink, bool bold = false,
           juce::Justification alignment = juce::Justification::centred, bool mono = false)
{
    g.setColour (colour);
    g.setFont (font (size, bold, mono));
    g.drawText (s, r, alignment, false);
}

juce::String dbText (float db)
{
    return db <= -99.0f ? juce::String::fromUTF8 ("−∞") : juce::String (db, 1);
}

void tiled (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> area)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    g.setTiledImageFill (image, 0, 0, 1.0f);
    g.fillRect (area);
}

// Reuse the approved horizontal grain, with neutral aluminium reflectance.
// Build this once per editor; never recolour the shared ImageCache texture.
juce::Image silverFinish (const juce::Image& source)
{
    auto result = source.createCopy();
    juce::Image::BitmapData pixels (result, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < pixels.height; ++y)
        for (int x = 0; x < pixels.width; ++x)
        {
            const auto c = pixels.getPixelColour (x, y);
            const float luminance = 0.2126f * c.getFloatRed() + 0.7152f * c.getFloatGreen() + 0.0722f * c.getFloatBlue();
            const auto level = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (171.0f + 190.0f * luminance));
            pixels.setPixelColour (x, y, juce::Colour (level, level, level));
        }
    return result;
}

class PanelButton final : public juce::Button
{
public:
    explicit PanelButton (juce::String title, bool navigation = false)
        : juce::Button (title), isNavigation (navigation)
    {
        setButtonText (title);
        setWantsKeyboardFocus (true);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool lit = isPower ? powerBypassed : getToggleState();
        const bool lamp = lit && ! isNavigation && ! isPower;
        g.setGradientFill (juce::ColourGradient (lamp ? juce::Colour (0xffffe09a)
                                                                                     : juce::Colour (0xff303333),
                                                0.0f, 0.0f,
                                                lamp ? juce::Colour (0xffbb8437)
                                                                      : juce::Colour (0xff101314),
                                                0.0f, (float) getHeight(), false));
        g.fillRect (r);
        g.setColour (lamp ? juce::Colour (0xffc28c41) : juce::Colour (0xff484c4c));
        g.drawRect (r, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawRect (r.reduced (2.0f), lamp ? 2.0f : 1.0f);
        if (over || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.09f : 0.035f));
            g.fillRect (r.reduced (1));
        }
        if (hasKeyboardFocus (true))
        {
            g.setColour (amber);
            g.drawRect (r.reduced (2.0f), 1.0f);
        }
        auto tr = r;
        if (isNavigation && lit)
        {
            g.setColour (amber.withAlpha (0.14f));
            g.fillEllipse (8, (float) getHeight() * 0.5f - 5, 10, 10);
            g.setColour (juce::Colour (0xffffd681));
            g.fillEllipse (11, (float) getHeight() * 0.5f - 2, 4, 4);
            tr.removeFromLeft (12);
        }
        const bool all = getButtonText() == "ALL";
        drawLabel (g, getButtonText(), tr, isNavigation ? 10.0f : (all ? 12.0f : 13.0f),
              isPower ? (lit ? muted : amber) : (lamp ? juce::Colour (0xff261d0f) : ((lit || all) ? amber : ink)),
              isNavigation, juce::Justification::centred, ! isNavigation);
    }
    bool isPower = false, powerBypassed = false;
private:
    bool isNavigation;
};

class Knob final : public juce::Slider
{
public:
    Knob (const juce::Image& texture, bool timing, bool input)
        : image (texture), isTiming (timing), isInput (input)
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
        setMouseDragSensitivity (200);
        setWantsKeyboardFocus (true);
        setScrollWheelEnabled (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    double proportionOfLengthToValue (double p) override
    {
        return juce::Slider::proportionOfLengthToValue (isTiming ? 1.0 - p : p);
    }
    double valueToProportionOfLength (double v) override
    {
        const auto p = juce::Slider::valueToProportionOfLength (v);
        return isTiming ? 1.0 - p : p;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.getNumberOfClicks() > 1)
            return;
        dragY = e.position.y;
        dragGesture = std::make_unique<juce::Slider::ScopedDragNotification> (*this);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragGesture == nullptr)
            return;
        const double delta = (double) (dragY - e.position.y) / (e.mods.isShiftDown() ? 2000.0 : 200.0);
        dragY = e.position.y;
        const auto position = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta);
        setValue (proportionOfLengthToValue (position), juce::sendNotificationSync);
    }
    void mouseUp (const juce::MouseEvent&) override { dragGesture.reset(); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        dragGesture.reset();
        const juce::Slider::ScopedDragNotification gesture (*this);
        setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto k = key.getKeyCode();
        const int direction = (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey) ? 1
                            : (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey) ? -1 : 0;
        if (direction == 0)
            return juce::Slider::keyPressed (key);
        step ((double) direction * (key.getModifiers().isShiftDown() ? 0.001 : 0.01));
        return true;
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.isInertial)
            return;
        auto delta = std::abs (wheel.deltaY) >= std::abs (wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
        if (wheel.isReversed)
            delta = -delta;
        step ((double) delta * (e.mods.isShiftDown() ? 0.02 : 0.2));
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float side = (float) juce::jmin (getWidth(), getHeight());
        const juce::Point<float> centre ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f);
        const float body = side * (isTiming ? 0.72f : 0.80f);
        const auto bounds = juce::Rectangle<float> (body, body).withCentre (centre);
        const float dotRadius = side * (isTiming ? 0.42f : 0.47f);
        for (int i = 0; i <= 18; ++i)
        {
            const float angle = (-135.0f + 270.0f * (float) i / 18.0f) * pi / 180.0f;
            const auto p = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * dotRadius;
            const float d = i % 3 == 0 ? 2.6f : 1.65f;
            g.setColour (scaleInk.withAlpha (0.9f));
            g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (bounds.translated (1.0f, 3.0f).expanded (1.0f));
        g.drawImage (image, bounds, juce::RectanglePlacement::stretchToFit);
        const float angle = (-135.0f + 270.0f * (float) valueToProportionOfLength (getValue())) * pi / 180.0f;
        const auto direction = juce::Point<float> (std::sin (angle), -std::cos (angle));
        const auto tip = centre + direction * (body * 0.435f);
        const auto tail = centre + direction * (body * 0.025f);
        const float lineWidth = isTiming ? 2.6f : 3.9f;
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine ({ tail.translated (1, 1), tip.translated (1, 1) }, lineWidth + 1.4f);
        g.setColour (cream);
        g.drawLine ({ tail, tip }, lineWidth);
        if (hasKeyboardFocus (true) || isMouseButtonDown())
        {
            // Focus describes only the physical knob, never the scale or its labels.
            g.setColour (juce::Colour (0xffd6b775));
            g.drawEllipse (bounds.reduced (body * 0.025f), 1.25f);
        }
    }
    void paintScale (juce::Graphics& g)
    {
        const float side = (float) juce::jmin (getWidth(), getHeight());
        const juce::Point<float> centre ((float) getX() + (float) getWidth() * 0.5f,
                                         (float) getY() + (float) getHeight() * 0.5f);
        if (rack || isTiming)
        {
            const int count = isTiming ? 4 : (isInput ? 7 : 5);
            for (int i = 0; i < count; ++i)
            {
                const float fraction = (float) i / (float) (count - 1);
                const float angle = (-135.0f + 270.0f * fraction) * pi / 180.0f;
                const auto p = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (side * 0.555f);
                const int number = isTiming ? 1 + i * 2 : (isInput ? -20 + i * 10 : -20 + i * 10);
                const juce::String label = ! isTiming && number > 0 ? "+" + juce::String (number) : juce::String (number);
                drawLabel (g, label, { p.x - 17, p.y - 8, 34, 16 }, isTiming ? 10.0f : 10.0f,
                      scaleInk, false, juce::Justification::centred, true);
            }
        }
    }
    void setScaleInk (juce::Colour colour)
    {
        if (scaleInk != colour) { scaleInk = colour; repaint(); }
    }
    bool rack = true;
private:
    void step (double delta)
    {
        const juce::Slider::ScopedDragNotification gesture (*this);
        const auto position = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta);
        setValue (proportionOfLengthToValue (position), juce::sendNotificationSync);
    }
    juce::Colour scaleInk = ink;
    const juce::Image& image;
    bool isTiming, isInput;
    float dragY = 0;
    std::unique_ptr<juce::Slider::ScopedDragNotification> dragGesture;
};

struct MeterChannel
{
    float display = -100.0f, held = -100.0f;
    double holdUntil = 0.0;
    void update (float value, double now, float dt)
    {
        display = juce::jmax (value, display - 24.0f * dt);
        if (value >= held || now >= holdUntil)
        {
            held = value;
            holdUntil = now + 1.0;
        }
    }
};
}

class FieldEffectEditor::Panel final : public juce::Component, private juce::Timer
{
public:
    Panel (FieldEffectProcessor& p, std::function<void (bool, int)> switchMode)
        : processor (p), onModeChange (std::move (switchMode)),
          knobImage (juce::ImageCache::getFromMemory (BinaryData::conceptknobbody_png, BinaryData::conceptknobbody_pngSize)),
          metal (juce::ImageCache::getFromMemory (BinaryData::conceptmetal_jpg, BinaryData::conceptmetal_jpgSize).rescaled (132, 84, juce::Graphics::highResamplingQuality)),
          silverMetal (silverFinish (metal)),
          paper (juce::ImageCache::getFromMemory (BinaryData::conceptvupaper_jpg, BinaryData::conceptvupaper_jpgSize).rescaled (32, 26, juce::Graphics::highResamplingQuality)),
          input (knobImage, false, true), output (knobImage, false, false),
          attack (knobImage, true, false), release (knobImage, true, false),
          rackButton ("RACK", true), dynamicButton ("DYNAMIC", true), bypass ("IN"),
          ratio4 ("4"), ratio8 ("8"), ratio12 ("12"), ratio20 ("20"), all ("ALL"),
          meterGR ("GR"), meterIN ("IN"), meterOUT ("OUT"), revD ("REV D"), revH ("REV H*")
    {
        setOpaque (true);
        auto attach = [this] (Knob& knob, const char* id, const char* name)
        {
            addAndMakeVisible (knob);
            knob.setName (name);
            knob.setTitle (name);
            knob.setDescription (juce::String (name) + ". Drag vertically. Shift for fine adjustment. Double-click to reset.");
            knob.onValueChange = [this] { repaint(); };
            sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                                         (processor.parameters, id, knob));
        };
        attach (input, "inputDb", "Input gain");
        attach (output, "outputDb", "Output gain");
        attach (attack, "attackUs", "Attack time, clockwise faster");
        attach (release, "releaseMs", "Release time, clockwise faster");
        for (auto* b : { &rackButton, &dynamicButton, &bypass, &ratio4, &ratio8, &ratio12,
                         &ratio20, &all, &meterGR, &meterIN, &meterOUT, &revD, &revH })
            addAndMakeVisible (*b);
        rackButton.onClick = [this] { onModeChange (false, 0); };
        dynamicButton.onClick = [this] { onModeChange (true, 0); };
        bypass.isPower = true;
        bypass.setClickingTogglesState (true);
        bypass.setTitle ("Bypass compressor");
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                            (processor.parameters, "bypass", bypass);
        all.setClickingTogglesState (true);
        all.setTitle ("All buttons compression mode");
        allAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                         (processor.parameters, "allButtons", all);
        auto* ratioParameter = processor.parameters.getParameter ("ratio");
        ratioAttachment = std::make_unique<juce::ParameterAttachment>
                          (*ratioParameter, [this] (float value) { ratioIndex = juce::roundToInt (value); updateButtons(); }, nullptr);
        const std::array<PanelButton*, 4> ratios { &ratio4, &ratio8, &ratio12, &ratio20 };
        for (int i = 0; i < 4; ++i)
        {
            ratios[(size_t) i]->setTitle ("Ratio " + ratios[(size_t) i]->getButtonText() + " to one");
            ratios[(size_t) i]->onClick = [this, i]
            {
                if (auto* parameter = processor.parameters.getParameter ("allButtons"))
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost (0.0f);
                    parameter->endChangeGesture();
                }
                ratioAttachment->setValueAsCompleteGesture ((float) i);
                updateButtons();
            };
        }
        const std::array<PanelButton*, 3> meterButtons { &meterGR, &meterIN, &meterOUT };
        for (int i = 0; i < 3; ++i)
            meterButtons[(size_t) i]->onClick = [this, i] { meterMode = i; updateButtons(); repaint(); };
        revisionAttachment = std::make_unique<juce::ParameterAttachment>
            (*processor.parameters.getParameter ("revision"), [this] (float) { updateButtons(); repaint(); }, nullptr);
        revD.setTitle ("Rev D circuit");
        revH.setTitle ("Rev H experimental circuit");
        revH.setDescription ("Experimental G/H input and push-pull output model. Not calibrated against hardware.");
        revH.setTooltip ("Rev H: experimental model, not calibrated against hardware");
        revD.onClick = [this] { revisionAttachment->setValueAsCompleteGesture (0.0f); };
        revH.onClick = [this] { revisionAttachment->setValueAsCompleteGesture (1.0f); };
        revisionAttachment->sendInitialUpdate();
        ratioAttachment->sendInitialUpdate();
        updateButtons();
        lastTick = juce::Time::getMillisecondCounterHiRes() * 0.001;
        previousUserBypass = userBypass();
        lastUserBypassChange = lastTick;
        startTimerHz (60);
    }

    ~Panel() override { stopTimer(); }
    bool dynamic = false;

    void resized() override
    {
        revD.setBounds (766, 6, 75, 24);
        revH.setBounds (844, 6, 80, 24);
        rackButton.setBounds (1065, 5, 71, 25);
        dynamicButton.setBounds (1139, 5, 82, 25);
        for (auto* k : { &input, &output, &attack, &release })
            k->rack = ! dynamic;
        if (dynamic)
        {
            input.setBounds (62, 350, 124, 124);
            output.setBounds (649, 350, 124, 124);
            attack.setBounds (288, 351, 92, 97);
            release.setBounds (468, 351, 92, 97);
            bypass.setBounds (1180, 398, 45, 44);
            const std::array<PanelButton*, 5> buttons { &ratio4, &ratio8, &ratio12, &ratio20, &all };
            for (int i = 0; i < 5; ++i)
                buttons[(size_t) i]->setBounds (826 + 57 * i, 409, 55, 32);
        }
        else
        {
            // The gain knob includes its ticks; the body itself occupies 80% of this box.
            input.setBounds (119, 71, 156, 156);
            output.setBounds (350, 71, 156, 156);
            attack.setBounds (590, 66, 81, 81);
            release.setBounds (590, 176, 81, 81);
            bypass.setBounds (974, 8, 63, 20);
            const std::array<PanelButton*, 5> buttons { &ratio4, &ratio8, &ratio12, &ratio20, &all };
            for (int i = 0; i < 5; ++i)
                buttons[(size_t) i]->setBounds (764, 70 + 31 * i + (i == 4 ? 4 : 0), 58, 28);
        }
        meterGR.setBounds (929, 233, 62, 27);
        meterIN.setBounds (994, 233, 62, 27);
        meterOUT.setBounds (1059, 233, 62, 27);
        for (auto* b : { &meterGR, &meterIN, &meterOUT })
            b->setVisible (! dynamic);
        updateButtons();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff282829));
        tiled (g, silverPanel ? silverMetal : metal, getLocalBounds().toFloat());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (silverPanel ? 0.18f : 0.015f), 0, 0,
                                                juce::Colours::black.withAlpha (silverPanel ? 0.08f : 0.12f), 1280, 334, false));
        g.fillAll();
        g.setColour (juce::Colour (0xff070808));
        g.fillRect (0, 35, 1280, 2);
        g.setColour (juce::Colour (0xff686965).withAlpha (0.5f));
        g.drawRect (getLocalBounds(), 1);
        const float brandX = dynamic ? 42.0f : 78.0f;
        drawLabel (g, "HT-76", { brandX, 2, 90, 31 }, 26, silverPanel ? plateInk() : cream, true, juce::Justification::centredLeft);
        drawLabel (g, "/", { brandX + 93, 3, 13, 29 }, 22, plateMuted());
        drawLabel (g, "F I E L D  E F F E C T", { brandX + 111, 5, 215, 25 }, 11.5f, plateInk(),
              false, juce::Justification::centredLeft);
        g.setColour (dark);
        g.fillRect (1062, 3, 163, 29);
        g.setColour (juce::Colour (0xff45494a));
        g.drawRect (1062, 3, 163, 29);
        if (dynamic)
            paintDynamic (g);
        else
            paintRack (g);
        for (auto* knob : { &input, &output, &attack, &release })
            knob->paintScale (g);
    }

private:
    juce::Colour plateInk() const { return silverPanel ? juce::Colour (0xff232526) : ink; }
    juce::Colour plateMuted() const { return silverPanel ? juce::Colour (0xff47494a) : muted; }

    bool userBypass() const
    {
        return processor.parameters.getRawParameterValue ("bypass")->load() > 0.5f;
    }

    bool effectiveBypass() const { return userBypass() || reflectedHostBypass; }

    void updateButtons()
    {
        const bool useH = processor.parameters.getRawParameterValue ("revision")->load() > 0.5f;
        if (silverPanel != useH)
        {
            silverPanel = useH;
            for (auto* knob : { &input, &output, &attack, &release })
                knob->setScaleInk (plateInk());
            repaint();
        }
        revD.setToggleState (! useH, juce::dontSendNotification);
        revH.setToggleState (useH, juce::dontSendNotification);
        rackButton.setToggleState (! dynamic, juce::dontSendNotification);
        dynamicButton.setToggleState (dynamic, juce::dontSendNotification);
        const bool allOn = processor.parameters.getRawParameterValue ("allButtons")->load() > 0.5f;
        const std::array<PanelButton*, 4> ratios { &ratio4, &ratio8, &ratio12, &ratio20 };
        for (int i = 0; i < 4; ++i)
            ratios[(size_t) i]->setToggleState (! allOn && i == ratioIndex, juce::dontSendNotification);
        meterGR.setToggleState (meterMode == 0, juce::dontSendNotification);
        meterIN.setToggleState (meterMode == 1, juce::dontSendNotification);
        meterOUT.setToggleState (meterMode == 2, juce::dontSendNotification);
        const bool bypassed = effectiveBypass();
        if (bypass.powerBypassed != bypassed)
        {
            bypass.powerBypassed = bypassed;
            bypass.repaint();
        }
        const auto title = dynamic ? (bypassed ? "OUT" : "IN") : (bypassed ? "BYPASS" : "ACTIVE");
        if (bypass.getButtonText() != title)
            bypass.setButtonText (title);
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float dt = (float) juce::jlimit (0.001, 0.25, now - lastTick);
        lastTick = now;
        // State may be restored by the host while this editor is already open.
        // Apply it on the message thread; layout changes never touch the DSP.
        const bool restoredDynamic = processor.editorMode.load() == 1;
        const int restoredWidth = juce::jlimit (960, 1920, processor.editorWidth.load());
        if (restoredDynamic != dynamic || (getParentComponent() != nullptr
                                           && getParentComponent()->getWidth() != restoredWidth))
            onModeChange (restoredDynamic, restoredWidth);
        const bool bypassParameter = userBypass();
        if (bypassParameter != previousUserBypass)
        {
            previousUserBypass = bypassParameter;
            lastUserBypassChange = now;
            reflectedHostBypass = false;
            consecutiveHostBypassFrames = 0;
        }
        field::MeterFrame frame;
        bool received = false;
        std::array<float, 2> intervalInput { -100, -100 }, intervalOutput { -100, -100 };
        float intervalReduction = 0;
        // The queue is bounded by MeterBridge. Frames are generated at the audio sample clock.
        while (processor.meterBridge().pop (frame))
        {
            if (hasSequence && frame.sequence != lastSequence + 1)
            {
                if (frame.sequence <= lastSequence || frame.sequence - lastSequence > history.size())
                    historyCount = 0;
                else
                    for (auto missing = lastSequence + 1; missing < frame.sequence; ++missing)
                        appendHistory (field::MeterFrame {}, false);
            }
            // A user-bypass transition can leave a fully dry frame queued. Require
            // fresh, consecutive frames after the 5 ms crossfade and a 50 ms guard.
            if (! frame.bypassed || bypassParameter)
            {
                reflectedHostBypass = false;
                consecutiveHostBypassFrames = 0;
            }
            else if (now - lastUserBypassChange > 0.05)
            {
                consecutiveHostBypassFrames = juce::jmin (2, consecutiveHostBypassFrames + 1);
                reflectedHostBypass = consecutiveHostBypassFrames >= 2;
            }
            latest = frame;
            appendHistory (frame, true);
            lastSequence = frame.sequence;
            hasSequence = true;
            for (size_t i = 0; i < 2; ++i)
            {
                intervalInput[i] = juce::jmax (intervalInput[i], frame.inputDb[i]);
                intervalOutput[i] = juce::jmax (intervalOutput[i], frame.outputDb[i]);
            }
            intervalReduction = juce::jmax (intervalReduction, frame.reductionDb);
            received = true;
        }
        if (received)
            lastFrameTime = now;
        const bool running = lastFrameTime > 0.0 && now - lastFrameTime < 0.15;
        if (! running)
        {
            reflectedHostBypass = false;
            consecutiveHostBypassFrames = 0;
        }
        for (size_t i = 0; i < 2; ++i)
        {
            inputMeters[i].update (running ? (received ? intervalInput[i] : latest.inputDb[i]) : -100.0f, now, dt);
            outputMeters[i].update (running ? (received ? intervalOutput[i] : latest.outputDb[i]) : -100.0f, now, dt);
        }
        const float targetReduction = running ? (received ? intervalReduction : latest.reductionDb) : 0.0f;
        reduction = juce::jmax (targetReduction, reduction - 30.0f * dt);
        if (targetReduction >= heldReduction || now >= reductionHoldUntil)
        {
            heldReduction = targetReduction;
            reductionHoldUntil = now + 1.0;
        }
        const float vuCoefficient = 1.0f - std::exp (-dt / 0.1303f); // 90% rise in approximately 300 ms.
        const float inTarget = running ? juce::Decibels::decibelsToGain (juce::jmax (latest.inputRmsDb[0], latest.inputRmsDb[1])) : 0.0f;
        const float outTarget = running ? juce::Decibels::decibelsToGain (juce::jmax (latest.outputRmsDb[0], latest.outputRmsDb[1])) : 0.0f;
        vuInput += (inTarget - vuInput) * vuCoefficient;
        vuOutput += (outTarget - vuOutput) * vuCoefficient;
        vuReduction += (targetReduction - vuReduction) * vuCoefficient;
        updateButtons();
        repaint();
    }

    void appendHistory (const field::MeterFrame& frame, bool valid)
    {
        history[historyWrite] = frame;
        historyValid[historyWrite] = valid;
        historyWrite = (historyWrite + 1) % history.size();
        historyCount = juce::jmin (historyCount + 1, history.size());
    }

    void screw (juce::Graphics& g, float x, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillEllipse (x - 1, y, 20, 20);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff777a79), x + 3, y + 2,
                                                juce::Colour (0xff121516), x + 13, y + 16, false));
        g.fillEllipse (x, y, 18, 18);
        g.setColour (juce::Colour (0xff050707));
        g.drawLine (x + 4, y + 12, x + 13, y + 6, 2.0f);
    }

    void paintRack (juce::Graphics& g)
    {
        for (float x : { 0.0f, 1224.0f })
        {
            g.setGradientFill (juce::ColourGradient (silverPanel ? juce::Colour (0xffa6a6a6) : juce::Colour (0xff282a2b), x, 0,
                                                    silverPanel ? juce::Colour (0xffd1d1d1) : juce::Colour (0xff313334), x + 36, 0, false));
            g.fillRect (x, 0.0f, 56.0f, 334.0f);
            g.setColour (juce::Colour (0xff070808));
            g.fillRect (x < 1.0f ? 54.0f : x, 0.0f, 2.0f, 334.0f);
            for (float y : { 40.0f, 250.0f })
            {
                g.setColour (juce::Colour (0xff777970).withAlpha (0.4f));
                g.fillRoundedRectangle (x + 9, y + 1, 31, 21, 9);
                g.setColour (juce::Colour (0xff020303));
                g.fillRoundedRectangle (x + 9, y, 31, 21, 9);
            }
        }
        screw (g, 68, 50); screw (g, 1193, 50);
        screw (g, 68, 242); screw (g, 1193, 242);
        drawLabel (g, "INPUT", { 117, 231, 160, 19 }, 13.0f, plateInk());
        drawLabel (g, "OUTPUT", { 348, 231, 160, 19 }, 13.0f, plateInk());
        drawLabel (g, "A T T A C K", { 570, 47, 122, 16 }, 10.0f, plateInk());
        drawLabel (g, "R E L E A S E", { 570, 158, 122, 16 }, 10.0f, plateInk());
        for (float y : { 144.0f, 255.0f })
        {
            drawLabel (g, "SLOW", { 585, y, 38, 14 }, 9.0f, plateMuted());
            drawLabel (g, "FAST", { 642, y, 38, 14 }, 9.0f, plateMuted());
        }
        drawLabel (g, "R A T I O", { 750, 47, 86, 16 }, 10.0f, plateInk());
        g.setColour (dark);
        g.fillRect (760, 66, 66, 165);
        g.setColour (juce::Colour (0xff535754));
        g.drawRect (760, 66, 66, 165);
        g.setColour (juce::Colour (0xff8a8262).withAlpha (0.35f));
        g.drawVerticalLine (867, 67, 224);
        paintVu (g, { 893, 66, 262, 137 });
        drawLabel (g, "M E T E R", { 929, 213, 192, 16 }, 9.0f, plateInk());
        g.setColour (dark);
        g.fillRect (925, 229, 200, 35);
        g.setColour (juce::Colour (0xff474c49));
        g.drawRect (925, 229, 200, 35);
        g.setColour (juce::Colour (0xff060707));
        g.fillRect (56, 270, 1168, 2);
        g.setColour (juce::Colour (0xff64675d).withAlpha (0.4f));
        g.drawHorizontalLine (269, 56, 1224);
        horizontalMeters (g, 83, inputMeters, teal, "IN");
        horizontalMeters (g, 666, outputMeters, cream, "OUT");
        g.setColour (juce::Colour (0xff64675d).withAlpha (0.4f));
        g.drawVerticalLine (640, 283, 313);
    }

    void horizontalMeters (juce::Graphics& g, float x, const std::array<MeterChannel, 2>& meters,
                           juce::Colour colour, const juce::String& name)
    {
        drawLabel (g, name, { x, 282, 28, 23 }, 11, plateInk(), false, juce::Justification::centred, true);
        constexpr int segments = 50;
        constexpr float width = 435.0f;
        for (size_t c = 0; c < 2; ++c)
        {
            const float y = c == 0 ? 285.0f : 300.0f;
            drawLabel (g, c == 0 ? "L" : "R", { x + 34, y - 2, 15, 12 }, 9, plateMuted(), false, juce::Justification::centred, true);
            if (silverPanel)
            {
                // Recess only the LED lane; the surrounding faceplate stays silver.
                g.setColour (dark);
                g.fillRect (x + 61, y - 1, width, 10.0f);
            }
            for (int i = 0; i < segments; ++i)
            {
                const float threshold = -60.0f + 60.0f * (float) i / (float) segments;
                g.setColour (meters[c].display > threshold ? (threshold > -1.0f ? amber : colour) : dark);
                g.fillRect (x + 62 + (float) i * width / (float) segments, y, width / (float) segments - 2.0f, 8.0f);
                if (meters[c].display > threshold)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.14f));
                    g.fillRect (x + 62 + (float) i * width / (float) segments, y, width / (float) segments - 2.0f, 1.0f);
                }
            }
        }
        for (int i = 0; i <= 5; ++i)
            drawLabel (g, juce::String (-60 + 12 * i), { x + 48 + (float) i * width / 5, 313, 28, 12 },
                  8.5f, plateMuted(), false, juce::Justification::centred, true);
        drawLabel (g, "dBFS", { x + 503, 298, 37, 13 }, 8.5f, plateMuted(), false, juce::Justification::centred, true);
        drawLabel (g, dbText (juce::jmax (meters[0].held, meters[1].held)), { x + 501, 282, 40, 15 }, 9,
              silverPanel ? plateInk() : colour, false, juce::Justification::centred, true);
    }

    void paintVu (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff070909));
        g.fillRect (bounds.expanded (3));
        g.setColour (juce::Colour (0xff65655c));
        g.drawRect (bounds.expanded (3), 1);
        g.setColour (juce::Colour (0xff0d1011));
        g.fillRect (bounds);
        const auto face = bounds.reduced (8);
        tiled (g, paper, face);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4e4b0).withAlpha (0.06f), face.getCentre(),
                                                juce::Colour (0xff372615).withAlpha (0.38f), face.getTopLeft(), true));
        g.fillRect (face);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (face.toNearestInt());
        const auto centre = juce::Point<float> (face.getCentreX(), face.getBottom() + 91.0f);
        constexpr float radius = 164.0f;
        auto point = [&] (float angle, float r) { const float a = angle * pi / 180.0f;
            return centre + juce::Point<float> (std::sin (a), -std::cos (a)) * r; };
        juce::Path arc;
        for (int i = 0; i <= 100; ++i)
        {
            const auto p = point (-35.0f + 70.0f * (float) i / 100.0f, radius);
            if (i == 0) arc.startNewSubPath (p); else arc.lineTo (p);
        }
        g.setColour (juce::Colour (0xff574b32));
        g.strokePath (arc, juce::PathStrokeType (1.1f));
        const std::array<const char*, 12> labels { "−20", "−10", "−7", "−5", "−3", "−2", "−1", "0", "+1", "+2", "+3", "" };
        for (int i = 0; i < 11; ++i)
        {
            const float a = -35.0f + 7.0f * (float) i;
            const auto p = point (a, radius);
            const auto outer = point (a, radius + 8);
            g.setColour (i > 7 ? juce::Colour (0xffa64b2c) : juce::Colour (0xff493d26));
            g.drawLine ({ p, outer }, 0.95f);
            const auto label = point (a, radius + 17);
            drawLabel (g, juce::String::fromUTF8 (labels[(size_t) i]), { label.x - 13, label.y - 6, 26, 12 }, 9,
                  i > 7 ? juce::Colour (0xffa64b2c) : juce::Colour (0xff493d26), false, juce::Justification::centred, true);
        }
        for (int i = 0; i <= 5; ++i)
        {
            const auto p = point (-35 + 14.0f * (float) i, radius - 16);
            drawLabel (g, juce::String (20 * i), { p.x - 10, p.y - 5, 20, 10 }, 7.5f,
                  juce::Colour (0xff51472c), false, juce::Justification::centred, true);
        }
        drawLabel (g, "VU", { face.getX(), face.getY() + 4, face.getWidth(), 15 }, 10,
              juce::Colour (0xff4c412b), false, juce::Justification::centred, true);
        drawLabel (g, "FIELD EFFECT", { face.getX(), face.getBottom() - 25, face.getWidth(), 12 }, 8.5f,
              juce::Colour (0xff51472c), false, juce::Justification::centred, true);
        drawLabel (g, meterMode == 0 ? "GAIN REDUCTION" : "−18 dBFS = 0 VU",
              { face.getX(), face.getBottom() - 14, face.getWidth(), 10 }, 6.8f,
              juce::Colour (0xff51472c), false, juce::Justification::centred, true);
        const float vuDb = meterMode == 0 ? -vuReduction
                          : juce::Decibels::gainToDecibels (meterMode == 1 ? vuInput : vuOutput, -100.0f) + 18.0f;
        constexpr std::array<float, 11> scale { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        float angle = -35;
        const float value = juce::jlimit (-20.0f, 3.0f, vuDb);
        for (size_t i = 1; i < scale.size(); ++i)
            if (value >= scale[i - 1])
                angle = -35.0f + 7.0f * ((float) (i - 1) + juce::jlimit (0.0f, 1.0f, (value - scale[i - 1]) / (scale[i] - scale[i - 1])));
        const auto needle = point (angle, radius + 4);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ centre.translated (2, 0), needle.translated (2, 0) }, 2);
        g.setColour (juce::Colour (0xff32362a));
        g.drawLine ({ centre, needle }, 1.7f);
    }

    void paintDynamic (juce::Graphics& g)
    {
        const juce::Rectangle<float> chart (6, 38, 899, 276), meters (910, 38, 363, 276), deck (6, 320, 1267, 182);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1013), chart.getTopLeft(),
                                                juce::Colour (0xff112024), chart.getBottomRight(), false));
        g.fillRect (chart);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff111719), meters.getTopLeft(),
                                                juce::Colour (0xff0c1114), meters.getBottomRight(), false));
        g.fillRect (meters);
        g.setColour (juce::Colours::black.withAlpha (silverPanel ? 0.035f : 0.38f));
        g.fillRect (deck);
        g.setColour (juce::Colour (0xff363c3e));
        g.drawRect (chart, 1); g.drawRect (meters, 1); g.drawRect (deck, 1);
        paintHistory (g);
        paintVerticalMeters (g);
        for (float x : { 245.0f, 605.0f, 1131.0f })
        {
            g.setColour (juce::Colour (0xff59605b).withAlpha (0.42f));
            g.drawVerticalLine ((int) x, 338, 486);
            g.setColour (dark);
            g.drawVerticalLine ((int) x + 1, 338, 486);
        }
        drawLabel (g, "I N P U T", { 64, 333, 120, 20 }, 12, plateInk());
        drawLabel (g, "A T T A C K", { 274, 333, 120, 20 }, 12, plateInk());
        drawLabel (g, "R E L E A S E", { 454, 333, 120, 20 }, 12, plateInk());
        drawLabel (g, "O U T P U T", { 651, 333, 120, 20 }, 12, plateInk());
        for (float x : { 290.0f, 470.0f })
        {
            drawLabel (g, "SLOW", { x, 450, 40, 14 }, 9, plateMuted());
            drawLabel (g, "FAST", { x + 60, 450, 40, 14 }, 9, plateMuted());
        }
        valueBox (g, 89, gainValue (input), teal);
        valueBox (g, 674, gainValue (output), teal);
        valueBox (g, 299, juce::String (attack.getValue(), 0) + " µs", teal);
        valueBox (g, 479, juce::String (release.getValue(), 0) + " ms", teal);
        drawLabel (g, "R A T I O", { 822, 365, 294, 21 }, 12, plateInk());
        g.setColour (dark);
        g.fillRect (822, 405, 293, 40);
        g.setColour (juce::Colour (0xff484d4a));
        g.drawRect (822, 405, 293, 40);
        drawLabel (g, "B Y P A S S", { 1140, 359, 127, 20 }, 11, plateInk());
        g.setColour (dark);
        g.fillRoundedRectangle (1176, 394, 53, 52, 2);
        g.setColour (juce::Colour (0xff474b49));
        g.drawRoundedRectangle (1176, 394, 53, 52, 2, 2);
        const bool bypassed = effectiveBypass();
        g.setColour (bypassed ? juce::Colour (0xff302d24) : amber.withAlpha (0.24f));
        g.fillEllipse (1197, 458, 11, 11);
        g.setColour (bypassed ? juce::Colour (0xff554c3b) : juce::Colour (0xfff9c46b));
        g.fillEllipse (1199, 460, 7, 7);
    }

    juce::String gainValue (juce::Slider& slider)
    {
        return (slider.getValue() > 0 ? "+" : "") + juce::String (slider.getValue(), 1) + " dB";
    }
    void valueBox (juce::Graphics& g, float x, const juce::String& value, juce::Colour colour)
    {
        g.setColour (juce::Colour (0xff050b0d));
        g.fillRect (x, 468.0f, 75.0f, 25.0f);
        g.setColour (juce::Colour (0xff30383a));
        g.drawRect (x, 468.0f, 75.0f, 25.0f, 1.0f);
        drawLabel (g, value, { x, 469, 75, 23 }, 14.0f, colour, false, juce::Justification::centred, true);
    }

    void paintHistory (juce::Graphics& g)
    {
        const std::array<juce::String, 3> names { "I N P U T", "O U T P U T", "R E D U C T I O N" };
        const std::array<juce::Colour, 3> colours { teal, cream, amber };
        const std::array<float, 3> legendX { 52, 139, 237 };
        for (size_t i = 0; i < 3; ++i)
        {
            g.setColour (colours[i]); g.fillRect (legendX[i], 57.0f, 15.0f, 2.0f);
            drawLabel (g, names[i], { legendX[i] + 21, 50, 130, 17 }, 9.5f, ink, false, juce::Justification::centredLeft);
        }
        const juce::Rectangle<float> plot (49, 86, 824, 198);
        for (int i = 0; i <= 5; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * ((float) (i * 12 + 6) / 66.0f);
            g.setColour (juce::Colour (0xff2b3c3f).withAlpha (0.65f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            drawLabel (g, juce::String (-12 * i), { 17, y - 7, 24, 14 }, 9, muted, false,
                  juce::Justification::centredRight, true);
        }
        for (int i = 0; i <= 6; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 6.0f;
            g.setColour (juce::Colour (0xff2b3c3f).withAlpha (0.65f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            const auto label = i == 6 ? juce::String ("now") : juce::String (-12 + i * 2) + "s";
            drawLabel (g, label, { x - 26, 290, 52, 17 }, 9, muted, false,
                  i == 6 ? juce::Justification::centredRight : juce::Justification::centred, true);
        }
        drawLabel (g, "GR", { 877, 74, 21, 13 }, 8, amber, false, juce::Justification::centred, true);
        for (int i = 0; i < 3; ++i)
            drawLabel (g, juce::String (i * 12), { 877, plot.getY() - 1 + (float) i * plot.getHeight() * 0.5f, 22, 14 },
                  8, amber, false, juce::Justification::centred, true);
        if (historyCount == 0)
            return;
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());
        std::array<juce::Path, 3> paths;
        const size_t start = (historyWrite + history.size() - historyCount) % history.size();
        float firstX = 0, lastX = 0;
        bool startPath = true;
        for (size_t i = 0; i < historyCount; ++i)
        {
            const auto index = (start + i) % history.size();
            if (! historyValid[index])
            {
                startPath = true;
                continue;
            }
            const auto& frame = history[index];
            const float x = plot.getRight() - plot.getWidth() * (float) (historyCount - 1 - i) / 719.0f;
            if (i == 0) firstX = x;
            lastX = x;
            const std::array<float, 3> ys {
                plot.getY() + plot.getHeight() * (6.0f - juce::jmax (frame.inputDb[0], frame.inputDb[1])) / 66.0f,
                plot.getY() + plot.getHeight() * (6.0f - juce::jmax (frame.outputDb[0], frame.outputDb[1])) / 66.0f,
                plot.getY() + plot.getHeight() * frame.reductionDb / 24.0f };
            for (size_t trace = 0; trace < 3; ++trace)
                if (startPath) paths[trace].startNewSubPath (x, ys[trace]);
                else paths[trace].lineTo (x, ys[trace]);
            startPath = false;
        }
        auto fill = paths[0];
        fill.lineTo (lastX, plot.getBottom()); fill.lineTo (firstX, plot.getBottom()); fill.closeSubPath();
        bool continuous = true;
        for (size_t i = 0; i < historyCount; ++i)
            continuous = continuous && historyValid[(start + i) % history.size()];
        if (continuous)
        {
            g.setColour (teal.withAlpha (0.045f));
            g.fillPath (fill);
        }
        for (size_t i = 0; i < paths.size(); ++i)
        {
            g.setColour (colours[i]);
            g.strokePath (paths[i], juce::PathStrokeType (i == 2 ? 1.35f : 1.0f, juce::PathStrokeType::curved));
        }
    }

    void paintVerticalMeters (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff343e3d).withAlpha (0.6f));
        g.drawVerticalLine (1041, 43, 307); g.drawVerticalLine (1139, 43, 307);
        drawLabel (g, "IN", { 934, 49, 83, 18 }, 12, ink, true, juce::Justification::centred, true);
        drawLabel (g, "GR", { 1055, 49, 70, 18 }, 12, ink, true, juce::Justification::centred, true);
        drawLabel (g, "OUT", { 1163, 49, 83, 18 }, 12, ink, true, juce::Justification::centred, true);
        const float top = 87, height = 180;
        auto levelY = [&] (float db) { return top + height * (6.0f - juce::jlimit (-60.0f, 6.0f, db)) / 66.0f; };
        const std::array<float, 2> starts { 956, 1186 };
        for (size_t bank = 0; bank < 2; ++bank)
        {
            const float x = starts[bank];
            const auto colour = bank == 0 ? teal : cream;
            const auto& channels = bank == 0 ? inputMeters : outputMeters;
            for (int db = 6; db >= -60; db -= 6)
                drawLabel (g, db > 0 ? "+6" : juce::String (db), { x - 31, levelY ((float) db) - 5, 22, 12 },
                      8.5f, muted, false, juce::Justification::centredRight, true);
            for (size_t channel = 0; channel < 2; ++channel)
            {
                const float bx = x + (float) channel * 42;
                drawLabel (g, channel == 0 ? "L" : "R", { bx, 67, 25, 14 }, 9, muted, false, juce::Justification::centred, true);
                g.setColour (juce::Colour (0xff050b0d)); g.fillRect (bx, top, 25.0f, height);
                const auto y = levelY (channels[channel].display);
                g.setGradientFill (juce::ColourGradient (colour.darker (0.24f), bx, 0, colour, bx + 14, 0, false));
                g.fillRect (bx, y, 25.0f, top + height - y);
                for (int s = 1; s < 36; ++s)
                {
                    g.setColour (dark.withAlpha (0.55f));
                    g.drawHorizontalLine ((int) (top + height * (float) s / 36), bx, bx + 25);
                }
                if (channels[channel].held > -60)
                {
                    g.setColour (channels[channel].held > 0 ? amber : colour.brighter (0.15f));
                    g.fillRect (bx, levelY (channels[channel].held), 25.0f, 1.5f);
                }
                drawLabel (g, dbText (channels[channel].held), { bx - 9, 274, 43, 20 }, 11,
                      channels[channel].held > 6 ? amber : colour, true, juce::Justification::centred, true);
            }
            drawLabel (g, "dBFS", { x - 40, 277, 34, 15 }, 8, muted, false, juce::Justification::centred, true);
            drawLabel (g, "PEAK", { x, 295, 67, 15 }, 9, muted, false, juce::Justification::centred, true);
        }
        constexpr float grX = 1083;
        g.setColour (juce::Colour (0xff050b0d)); g.fillRect (grX, top, 25.0f, height);
        const float grHeight = height * juce::jlimit (0.0f, 24.0f, reduction) / 24.0f;
        g.setGradientFill (juce::ColourGradient (amber.darker (0.2f), grX, 0, amber, grX + 14, 0, false));
        g.fillRect (grX, top, 25.0f, grHeight);
        for (int i = 0; i <= 4; ++i)
            drawLabel (g, juce::String (-6 * i), { grX - 32, top - 5 + height * (float) i / 4, 24, 12 },
                  8.5f, muted, false, juce::Justification::centredRight, true);
        for (int s = 1; s < 36; ++s)
        {
            g.setColour (dark.withAlpha (0.55f));
            g.drawHorizontalLine ((int) (top + height * (float) s / 36), grX, grX + 25);
        }
        drawLabel (g, "dB", { 1044, 277, 32, 15 }, 8, muted, false, juce::Justification::centred, true);
        drawLabel (g, juce::String (-heldReduction, 1), { 1070, 274, 52, 20 }, 11, amber, true, juce::Justification::centred, true);
        drawLabel (g, "PEAK", { 1070, 295, 52, 15 }, 9, muted, false, juce::Justification::centred, true);
    }

    FieldEffectProcessor& processor;
    std::function<void (bool, int)> onModeChange;
    juce::Image knobImage, metal, silverMetal, paper;
    bool silverPanel = false;
    Knob input, output, attack, release;
    PanelButton rackButton, dynamicButton, bypass, ratio4, ratio8, ratio12, ratio20, all, meterGR, meterIN, meterOUT, revD, revH;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, allAttachment;
    std::unique_ptr<juce::ParameterAttachment> ratioAttachment, revisionAttachment;
    std::array<field::MeterFrame, 720> history {};
    std::array<bool, 720> historyValid {};
    size_t historyWrite = 0, historyCount = 0;
    std::uint64_t lastSequence = 0;
    bool hasSequence = false;
    field::MeterFrame latest;
    std::array<MeterChannel, 2> inputMeters, outputMeters;
    float reduction = 0, heldReduction = 0, vuInput = 0, vuOutput = 0, vuReduction = 0;
    double lastTick = 0, lastFrameTime = 0, reductionHoldUntil = 0;
    int ratioIndex = 0, meterMode = 0;
    bool previousUserBypass = false, reflectedHostBypass = false;
    int consecutiveHostBypassFrames = 0;
    double lastUserBypassChange = 0;
};

FieldEffectEditor::FieldEffectEditor (FieldEffectProcessor& p)
    : juce::AudioProcessorEditor (p), ownerProcessor (p),
      panel (std::make_unique<Panel> (p, [this] (bool dynamic, int width) { setMode (dynamic, width); }))
{
    setOpaque (true);
    addAndMakeVisible (*panel);
    const int initialWidth = juce::jlimit (960, 1920, ownerProcessor.editorWidth.load());
    const int initialHeight = ownerProcessor.editorMode.load() == 1 ? 508 : 334;
    setSize (initialWidth, juce::roundToInt ((double) initialWidth * initialHeight / 1280.0));
    setResizable (true, true);
    setMode (ownerProcessor.editorMode.load() == 1);
}

FieldEffectEditor::~FieldEffectEditor() = default;

void FieldEffectEditor::setMode (bool dynamic, int restoredWidth)
{
    panel->dynamic = dynamic;
    ownerProcessor.editorMode.store (dynamic ? 1 : 0);
    const int designHeight = dynamic ? 508 : 334;
    const int width = juce::jlimit (960, 1920, restoredWidth > 0 ? restoredWidth
                                         : (getWidth() > 0 ? getWidth() : ownerProcessor.editorWidth.load()));
    setResizeLimits (960, juce::roundToInt (960.0 * designHeight / 1280.0),
                     1920, juce::roundToInt (1920.0 * designHeight / 1280.0));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (1280.0 / designHeight);
    setSize (width, juce::roundToInt ((double) width * designHeight / 1280.0));
    resized();
    repaint();
}

void FieldEffectEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff191a1b));
}

void FieldEffectEditor::resized()
{
    if (panel == nullptr)
        return;
    const int height = panel->dynamic ? 508 : 334;
    panel->setTransform (juce::AffineTransform());
    panel->setBounds (0, 0, 1280, height);
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / 1280.0f,
                                                    (float) getHeight() / (float) height));
    panel->resized();
    ownerProcessor.editorWidth.store (getWidth());
}
