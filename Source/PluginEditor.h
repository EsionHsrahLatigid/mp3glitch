/*
  ==============================================================================
    MP3Glitch - Plugin Editor Header
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
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
    
    // スライダー
    GlitchSlider glitchAmountSlider;
    GlitchSlider frameCorruptionSlider;
    GlitchSlider bitCrushSlider;
    GlitchSlider repeatProbSlider;
    GlitchSlider dropProbSlider;
    GlitchSlider quantNoiseSlider;
    GlitchSlider mdctSmearSlider;
    GlitchSlider mixSlider;
    
    // グリッチビジュアライザー用
    std::vector<float> visualizerData;
    int visualizerPhase = 0;
    float currentGlitchLevel = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MP3GlitchAudioProcessorEditor)
};
