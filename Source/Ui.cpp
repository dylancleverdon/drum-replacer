#include "Ui.h"

namespace ui
{
using namespace juce;

LookAndFeel::LookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, colours::background);
    setColour (Label::textColourId, colours::text);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, colours::shaper.withAlpha (0.4f));
    setColour (TextEditor::backgroundColourId, colours::background);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, colours::shaper);
    setColour (CaretComponent::caretColourId, colours::text);
    setColour (TextButton::buttonColourId, colours::shaper);
    setColour (TextButton::textColourOffId, colours::background);
    setColour (TextButton::textColourOnId, colours::background);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::panelEdge);
}

void LookAndFeel::setAccent (Slider& slider, Colour accent)
{
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setColour (Slider::trackColourId, accent);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
}

void LookAndFeel::setBipolar (Slider& slider, bool bipolar)
{
    slider.getProperties().set ("bipolar", bipolar);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float startAngle, float endAngle, Slider& slider)
{
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineWidth = jmax (3.0f, radius * 0.11f);
    const auto arcRadius = radius - lineWidth * 0.5f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool bipolar = slider.getProperties()["bipolar"];
    const auto stroke = PathStrokeType (lineWidth, PathStrokeType::curved, PathStrokeType::rounded);

    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (track, stroke);

    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    if (std::abs (angle - from) > 0.01f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, jmin (from, angle), jmax (from, angle), true);
        const auto accent = slider.findColour (Slider::rotarySliderFillColourId);
        g.setColour (slider.isEnabled() ? accent : accent.withSaturation (0.0f));
        g.strokePath (value, stroke);
    }

    const auto bodyRadius = arcRadius - lineWidth * 1.7f;
    const auto body = Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (ColourGradient (Colour (0xff343a44), centre.x, body.getY(),
                                       Colour (0xff1c1f25), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (colours::panelEdge.brighter (0.15f));
    g.drawEllipse (body, 1.0f);

    Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (bodyRadius * 0.3f, angle));
    pointer.lineTo (centre.getPointOnCircumference (bodyRadius * 0.82f, angle));
    g.setColour (colours::text);
    g.strokePath (pointer, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
}

void LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float minSliderPos, float maxSliderPos, Slider::SliderStyle style, Slider& slider)
{
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto track = Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 2.0f, (float) width, 4.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (track, 2.0f);

    g.setColour (slider.findColour (Slider::trackColourId));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    g.setColour (colours::text);
    g.fillEllipse (Rectangle<float> (13.0f, 13.0f).withCentre ({ sliderPos, track.getCentreY() }));
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour& background, bool highlighted, bool down)
{
    auto colour = background;

    if (down)
        colour = colour.brighter (0.25f);
    else if (highlighted)
        colour = colour.brighter (0.12f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat().reduced (0.5f), 4.0f);
}

Label* LookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (FontOptions (12.5f));
    label->setJustificationType (Justification::centred);
    label->setColour (Label::outlineColourId, Colours::transparentBlack);
    label->setColour (Label::backgroundColourId, Colours::transparentBlack);
    label->setColour (Label::outlineWhenEditingColourId, colours::panelEdge);
    return label;
}

Font LookAndFeel::getTextButtonFont (TextButton&, int buttonHeight)
{
    return FontOptions (jmin (13.0f, (float) buttonHeight * 0.6f), Font::bold);
}

void LookAndFeel::drawButtonText (Graphics& g, TextButton& button, bool highlighted, bool)
{
    const bool rightAligned = button.getProperties()["rightAligned"];
    auto colour = button.findColour (button.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);

    if (highlighted && rightAligned)
        colour = colour.brighter (0.4f);

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (colour.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (6, 0),
                      rightAligned ? Justification::centredRight : Justification::centred, 1);
}

//==============================================================================
void EnvelopeDisplay::push (const killroom::MeterFrame& frame)
{
    history[(size_t) head] = frame;
    head = (head + 1) % historySize;
}

void EnvelopeDisplay::setThreshold (float db)
{
    if (! juce::exactlyEqual (db, thresholdDb))
    {
        thresholdDb = db;
        repaint();
    }
}

void EnvelopeDisplay::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    auto inner = area.reduced (12.0f, 10.0f);
    inner.removeFromRight (26.0f); // scale labels
    const auto levelArea = inner.removeFromTop (inner.getHeight() * 0.64f);
    inner.removeFromTop (8.0f);
    const auto gainArea = inner;

    constexpr float topDb = 6.0f, bottomDb = -60.0f, gainRangeDb = 24.0f;
    auto levelY = [&] (float db) { return jmap (jlimit (bottomDb, topDb, db), bottomDb, topDb, levelArea.getBottom(), levelArea.getY()); };
    auto gainY = [&] (float db) { return gainArea.getCentreY() - jlimit (-gainRangeDb, gainRangeDb, db) / gainRangeDb * gainArea.getHeight() * 0.5f; };
    auto xAt = [&] (int i) { return levelArea.getX() + (float) i * levelArea.getWidth() / (float) (historySize - 1); };
    auto frameAt = [&] (int i) -> const killroom::MeterFrame& { return history[(size_t) ((head + i) % historySize)]; };

    // Grid
    g.setFont (FontOptions (10.0f));

    for (auto db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const auto y = levelY (db);
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine (roundToInt (y), levelArea.getX(), levelArea.getRight());
        g.setColour (colours::dimText.withAlpha (0.7f));
        g.drawText (String (roundToInt (db)), Rectangle<float> (levelArea.getRight() + 4.0f, y - 6.0f, 24.0f, 12.0f), Justification::centredLeft);
    }

    g.setColour (colours::panelEdge);
    g.drawHorizontalLine (roundToInt (gainArea.getCentreY()), gainArea.getX(), gainArea.getRight());
    g.setColour (colours::dimText.withAlpha (0.7f));
    g.drawText ("+24", Rectangle<float> (gainArea.getRight() + 4.0f, gainArea.getY() - 2.0f, 26.0f, 12.0f), Justification::centredLeft);
    g.drawText ("-24", Rectangle<float> (gainArea.getRight() + 4.0f, gainArea.getBottom() - 10.0f, 26.0f, 12.0f), Justification::centredLeft);

    // Level history
    Path input, output, outputEdge;
    input.startNewSubPath (xAt (0), levelArea.getBottom());
    output.startNewSubPath (xAt (0), levelArea.getBottom());

    for (int i = 0; i < historySize; ++i)
    {
        const auto& f = frameAt (i);
        input.lineTo (xAt (i), levelY (f.inputDb));
        output.lineTo (xAt (i), levelY (f.outputDb));

        if (i == 0)
            outputEdge.startNewSubPath (xAt (i), levelY (f.outputDb));
        else
            outputEdge.lineTo (xAt (i), levelY (f.outputDb));
    }

    input.lineTo (xAt (historySize - 1), levelArea.getBottom());
    output.lineTo (xAt (historySize - 1), levelArea.getBottom());
    input.closeSubPath();
    output.closeSubPath();

    g.setColour (colours::dimText.withAlpha (0.28f));
    g.fillPath (input);
    g.setColour (colours::output.withAlpha (0.28f));
    g.fillPath (output);
    g.setColour (colours::output.withAlpha (0.9f));
    g.strokePath (outputEdge, PathStrokeType (1.2f));

    // Threshold
    const auto thresholdY = levelY (thresholdDb);
    const float dashes[] = { 4.0f, 4.0f };
    g.setColour (colours::text.withAlpha (0.45f));
    g.drawDashedLine ({ levelArea.getX(), thresholdY, levelArea.getRight(), thresholdY }, dashes, 2, 1.0f);

    // Gain history: room killer reduction as a filled area, overall gain as a line
    Path kill, gain;
    kill.startNewSubPath (xAt (0), gainArea.getCentreY());

    for (int i = 0; i < historySize; ++i)
    {
        const auto& f = frameAt (i);
        kill.lineTo (xAt (i), gainY (f.killDb));

        if (i == 0)
            gain.startNewSubPath (xAt (i), gainY (f.gainDb));
        else
            gain.lineTo (xAt (i), gainY (f.gainDb));
    }

    kill.lineTo (xAt (historySize - 1), gainArea.getCentreY());
    kill.closeSubPath();

    g.setColour (colours::kill.withAlpha (0.35f));
    g.fillPath (kill);
    g.setColour (colours::shaper);
    g.strokePath (gain, PathStrokeType (1.4f));

    // Legend
    g.setFont (FontOptions (10.0f, Font::bold));
    auto legend = Rectangle<float> (levelArea.getX() + 4.0f, levelArea.getY() + 2.0f, 60.0f, 12.0f);
    g.setColour (colours::dimText);
    g.drawText ("INPUT", legend, Justification::centredLeft);
    g.setColour (colours::output);
    g.drawText ("OUTPUT", legend.translated (42.0f, 0.0f), Justification::centredLeft);
    g.setColour (colours::text.withAlpha (0.6f));
    g.drawText ("THRESHOLD", legend.translated (98.0f, 0.0f).withWidth (70.0f), Justification::centredLeft);
    g.setColour (colours::shaper);
    g.drawText ("GAIN", Rectangle<float> (gainArea.getX() + 4.0f, gainArea.getY(), 40.0f, 12.0f), Justification::centredLeft);
    g.setColour (colours::kill);
    g.drawText ("ROOM KILL", Rectangle<float> (gainArea.getX() + 36.0f, gainArea.getY(), 70.0f, 12.0f), Justification::centredLeft);
}
} // namespace ui
