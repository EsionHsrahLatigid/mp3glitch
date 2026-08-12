/*
  ==============================================================================
    MP3Glitch - Plugin Editor Header
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <ehl/juce_design/EhlDesign.h>

#include "PluginProcessor.h"

//==============================================================================
class GlitchSlider : public juce::Component
{
public:
    GlitchSlider(const juce::String& labelText, juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchSlider)
};

//==============================================================================
class MP3GlitchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit MP3GlitchAudioProcessorEditor(MP3GlitchAudioProcessor&);
    ~MP3GlitchAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
private:
    void timerCallback() override;
    
    MP3GlitchAudioProcessor& audioProcessor;
    ehl::juce_design::LookAndFeel ehlLookAndFeel;
    ehl::juce_design::ParameterDisplay parameterDisplay {
        ehl::juce_design::DisplayKind::bitcrusher
    };
    
    // スライダー
    GlitchSlider glitchAmountSlider;
    GlitchSlider frameCorruptionSlider;
    GlitchSlider bitCrushSlider;
    GlitchSlider repeatProbSlider;
    GlitchSlider dropProbSlider;
    GlitchSlider quantNoiseSlider;
    GlitchSlider mdctSmearSlider;
    GlitchSlider mixSlider;
    
    void updateDisplay();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MP3GlitchAudioProcessorEditor)
};
