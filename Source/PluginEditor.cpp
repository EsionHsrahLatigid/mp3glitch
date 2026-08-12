/*
  ==============================================================================
    MP3Glitch - Plugin Editor Implementation
  ==============================================================================
*/

#include "PluginEditor.h"

namespace
{
float readNormalized(juce::AudioProcessorValueTreeState &apvts,
                     const char *id) {
  if (auto *parameter = apvts.getParameter(id))
    return parameter->getValue();
  return 0.0f;
}
}

//==============================================================================
// GlitchSlider Implementation
//==============================================================================
GlitchSlider::GlitchSlider(const juce::String &labelText,
                           juce::AudioProcessorValueTreeState &apvts,
                           const juce::String &paramID) {
  ehl::juce_design::styleSlider(slider);
  addAndMakeVisible(slider);

  label.setText(labelText, juce::dontSendNotification);
  ehl::juce_design::styleLabel(label);
  label.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(label);

  attachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, paramID, slider);
}

void GlitchSlider::paint(juce::Graphics &) {}

void GlitchSlider::resized() {
  auto bounds = getLocalBounds();
  label.setBounds(bounds.removeFromTop(20));
  slider.setBounds(bounds);
}

//==============================================================================
// MP3GlitchAudioProcessorEditor Implementation
//==============================================================================
MP3GlitchAudioProcessorEditor::MP3GlitchAudioProcessorEditor(
    MP3GlitchAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      glitchAmountSlider("GLITCH", p.getAPVTS(), "glitchAmount"),
      frameCorruptionSlider("CORRUPT", p.getAPVTS(), "frameCorruption"),
      bitCrushSlider("BITCRUSH", p.getAPVTS(), "bitCrush"),
      repeatProbSlider("REPEAT", p.getAPVTS(), "repeatProb"),
      dropProbSlider("DROP", p.getAPVTS(), "dropProb"),
      quantNoiseSlider("Q-NOISE", p.getAPVTS(), "quantNoise"),
      mdctSmearSlider("MDCT", p.getAPVTS(), "mdctSmear"),
      mixSlider("MIX", p.getAPVTS(), "mix") {
  setLookAndFeel(&ehlLookAndFeel);
  setResizable(true, true);
  setResizeLimits(ehl::juce_design::Metrics::minimumWidth,
                  ehl::juce_design::Metrics::minimumHeight,
                  ehl::juce_design::Metrics::maximumWidth,
                  ehl::juce_design::Metrics::maximumHeight);

  addAndMakeVisible(parameterDisplay);
  addAndMakeVisible(glitchAmountSlider);
  addAndMakeVisible(frameCorruptionSlider);
  addAndMakeVisible(bitCrushSlider);
  addAndMakeVisible(repeatProbSlider);
  addAndMakeVisible(dropProbSlider);
  addAndMakeVisible(quantNoiseSlider);
  addAndMakeVisible(mdctSmearSlider);
  addAndMakeVisible(mixSlider);

  updateDisplay();

  setSize(ehl::juce_design::Metrics::defaultWidth,
          ehl::juce_design::Metrics::defaultHeight);
  startTimerHz(15);
}

MP3GlitchAudioProcessorEditor::~MP3GlitchAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

void MP3GlitchAudioProcessorEditor::timerCallback() {
  updateDisplay();
  repaint(ehl::juce_design::parameterDisplayArea(getLocalBounds()));
}

void MP3GlitchAudioProcessorEditor::paint(juce::Graphics &g) {
  ehl::juce_design::paintEditorChrome(g, getLocalBounds(), "MP3 GLITCH",
                                      "DATA CORRUPTION SIMULATOR");
}

void MP3GlitchAudioProcessorEditor::resized() {
  const auto bounds = getLocalBounds();
  parameterDisplay.setBounds(ehl::juce_design::parameterDisplayArea(bounds));

  auto placeSlider = [&](GlitchSlider &slider, std::size_t index) {
    slider.setBounds(ehl::juce_design::controlCell(bounds, index));
  };

  placeSlider(glitchAmountSlider, 0);
  placeSlider(frameCorruptionSlider, 1);
  placeSlider(bitCrushSlider, 2);
  placeSlider(repeatProbSlider, 3);
  placeSlider(dropProbSlider, 4);
  placeSlider(quantNoiseSlider, 5);

  auto bottom = ehl::juce_design::controlArea(bounds).removeFromBottom(92);
  const int width = (bottom.getWidth() - ehl::juce_design::Metrics::columnGap) / 2;
  mdctSmearSlider.setBounds(bottom.removeFromLeft(width));
  bottom.removeFromLeft(ehl::juce_design::Metrics::columnGap);
  mixSlider.setBounds(bottom.removeFromLeft(width));
}

void MP3GlitchAudioProcessorEditor::updateDisplay() {
  auto &apvts = audioProcessor.getAPVTS();
  parameterDisplay.setValues({readNormalized(apvts, "glitchAmount"),
                              readNormalized(apvts, "frameCorruption"),
                              readNormalized(apvts, "bitCrush"),
                              readNormalized(apvts, "repeatProb")});
}
