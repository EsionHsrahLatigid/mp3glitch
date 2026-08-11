#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>

namespace
{
bool check(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "[FAIL] " << message << '\n';
    return condition;
}

void setParameter(juce::AudioProcessorValueTreeState& apvts, const char* id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    MP3GlitchAudioProcessor processor;
    bool passed = true;

    passed &= check(processor.getName() == "MP3 Glitch", "product name should be MP3 Glitch");
    passed &= check(!processor.acceptsMidi(), "processor should not accept MIDI");
    passed &= check(!processor.producesMidi(), "processor should not produce MIDI");
    passed &= check(!processor.isMidiEffect(), "processor should be an audio effect");

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    passed &= check(processor.isBusesLayoutSupported(stereo), "stereo input/output should be supported");

    auto& apvts = processor.getAPVTS();
    auto* mix = apvts.getParameter("mix");
    passed &= check(mix != nullptr, "Mix parameter should exist");
    if (mix != nullptr)
    {
        mix->setValueNotifyingHost(mix->convertTo0to1(0.25f));
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        mix->setValueNotifyingHost(mix->convertTo0to1(0.75f));
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        passed &= check(std::abs(apvts.getRawParameterValue("mix")->load() - 0.25f) < 0.001f,
                        "APVTS state should round-trip");
    }

    constexpr double sampleRate = 44100.0;
    processor.prepareToPlay(sampleRate, 2048);
    int generatedSamples = 0;
    const int blockSizes[] { 1, 18, 32, 127, 256, 575, 576, 577, 1024, 2048 };

    for (int blockIndex = 0; blockIndex < static_cast<int>(std::size(blockSizes)); ++blockIndex)
    {
        const auto blockSize = blockSizes[blockIndex];
        setParameter(apvts, "glitchAmount", blockIndex % 2 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "frameCorruption", blockIndex % 3 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "bitCrush", blockIndex % 2 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "repeatProb", blockIndex % 2 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "dropProb", blockIndex % 4 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "quantNoise", blockIndex % 2 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "mdctSmear", blockIndex % 2 == 0 ? 0.0f : 1.0f);
        setParameter(apvts, "mix", blockIndex % 2 == 0 ? 0.25f : 1.0f);

        juce::AudioBuffer<float> audio(2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.15 * std::sin(2.0 * juce::MathConstants<double>::pi
                                                                  * 220.0 * generatedSamples / sampleRate));
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, value);
            ++generatedSamples;
        }
        if (blockSize > 2)
        {
            audio.setSample(0, 0, std::numeric_limits<float>::infinity());
            audio.setSample(1, 1, std::numeric_limits<float>::quiet_NaN());
        }

        juce::MidiBuffer midi;
        processor.processBlock(audio, midi);

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                passed &= check(std::isfinite(audio.getSample(channel, sample)), "processed audio should remain finite");
    }

    processor.prepareToPlay(48000.0, 257);
    juce::AudioBuffer<float> shortBlock(2, 257);
    shortBlock.clear();
    juce::MidiBuffer midi;
    processor.processBlock(shortBlock, midi);

    if (passed)
        std::cout << "MP3 Glitch plug-in integration checks passed\n";
    return passed ? 0 : 1;
}
