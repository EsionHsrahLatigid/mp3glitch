#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

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

void fillDeterministicBlock(juce::AudioBuffer<float>& audio, std::vector<float>& drySamples)
{
    drySamples.resize(static_cast<size_t>(audio.getNumChannels() * audio.getNumSamples()));

    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = static_cast<float>(
                0.23 * std::sin(0.013 * static_cast<double>(sample + 17 * channel))
                + 0.07 * std::cos(0.031 * static_cast<double>(sample + 5 * channel)));
            audio.setSample(channel, sample, value);
            drySamples[static_cast<size_t>(channel * audio.getNumSamples() + sample)] = value;
        }
    }
}

void configureTransformOnly(juce::AudioProcessorValueTreeState& apvts, float mix)
{
    setParameter(apvts, "glitchAmount", 0.0f);
    setParameter(apvts, "frameCorruption", 0.0f);
    setParameter(apvts, "bitCrush", 0.0f);
    setParameter(apvts, "repeatProb", 0.0f);
    setParameter(apvts, "dropProb", 0.0f);
    setParameter(apvts, "quantNoise", 0.0f);
    setParameter(apvts, "mdctSmear", 1.0f);
    setParameter(apvts, "mix", mix);
}

bool checkFiniteBlock(const juce::AudioBuffer<float>& audio, const char* message)
{
    bool passed = true;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            passed &= check(std::isfinite(audio.getSample(channel, sample)), message);

    return passed;
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

    {
        MP3GlitchAudioProcessor dryProcessor;
        auto& dryApvts = dryProcessor.getAPVTS();
        dryProcessor.prepareToPlay(48000.0, 256);
        setParameter(dryApvts, "glitchAmount", 1.0f);
        setParameter(dryApvts, "frameCorruption", 1.0f);
        setParameter(dryApvts, "bitCrush", 1.0f);
        setParameter(dryApvts, "repeatProb", 1.0f);
        setParameter(dryApvts, "dropProb", 1.0f);
        setParameter(dryApvts, "quantNoise", 1.0f);
        setParameter(dryApvts, "mdctSmear", 1.0f);
        setParameter(dryApvts, "mix", 0.0f);

        juce::AudioBuffer<float> longBlock(2, 2048);
        std::vector<float> original;
        fillDeterministicBlock(longBlock, original);
        dryProcessor.processBlock(longBlock, midi);

        for (int channel = 0; channel < longBlock.getNumChannels(); ++channel)
            for (int sample = 0; sample < longBlock.getNumSamples(); ++sample)
                passed &= check(std::abs(longBlock.getSample(channel, sample)
                                     - original[static_cast<size_t>(channel * longBlock.getNumSamples() + sample)])
                                    < 0.000001f,
                                "mix=0 should preserve dry samples for blocks larger than prepare hint");
    }

    {
        MP3GlitchAudioProcessor wetProcessor;
        MP3GlitchAudioProcessor mixedProcessor;
        wetProcessor.prepareToPlay(48000.0, 256);
        mixedProcessor.prepareToPlay(48000.0, 256);
        configureTransformOnly(wetProcessor.getAPVTS(), 1.0f);
        configureTransformOnly(mixedProcessor.getAPVTS(), 0.25f);

        juce::AudioBuffer<float> wetBlock(2, 2048);
        juce::AudioBuffer<float> mixedBlock(2, 2048);
        std::vector<float> original;
        fillDeterministicBlock(wetBlock, original);
        mixedBlock.makeCopyOf(wetBlock);

        wetProcessor.processBlock(wetBlock, midi);
        mixedProcessor.processBlock(mixedBlock, midi);

        for (int channel = 0; channel < mixedBlock.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < mixedBlock.getNumSamples(); ++sample)
            {
                const auto dry = original[static_cast<size_t>(channel * mixedBlock.getNumSamples() + sample)];
                const auto expected = dry * 0.75f + wetBlock.getSample(channel, sample) * 0.25f;
                passed &= check(std::abs(mixedBlock.getSample(channel, sample) - expected) < 0.00001f,
                                "partial mix should retain prepared dry chunks for oversized blocks");
            }
        }
        passed &= checkFiniteBlock(mixedBlock, "partial oversized block output should remain finite");
    }

    {
        MP3GlitchAudioProcessor highRateProcessor;
        auto& highRateApvts = highRateProcessor.getAPVTS();
        highRateProcessor.prepareToPlay(192000.0, 512);
        setParameter(highRateApvts, "glitchAmount", 1.0f);
        setParameter(highRateApvts, "frameCorruption", 0.0f);
        setParameter(highRateApvts, "bitCrush", 0.0f);
        setParameter(highRateApvts, "repeatProb", 0.0f);
        setParameter(highRateApvts, "dropProb", 0.0f);
        setParameter(highRateApvts, "quantNoise", 0.0f);
        setParameter(highRateApvts, "mdctSmear", 0.0f);
        setParameter(highRateApvts, "mix", 1.0f);

        juce::AudioBuffer<float> highRateBlock(2, 4096);
        highRateBlock.clear();
        highRateBlock.setSample(0, 0, 0.9f);
        highRateBlock.setSample(1, 0, -0.9f);
        highRateProcessor.processBlock(highRateBlock, midi);
        passed &= checkFiniteBlock(highRateBlock, "192k pre-echo block should remain finite");

        juce::AudioBuffer<float> highRateTail(2, 4096);
        highRateTail.clear();
        highRateProcessor.processBlock(highRateTail, midi);
        passed &= checkFiniteBlock(highRateTail, "192k pre-echo tail should remain finite");
    }

    if (passed)
        std::cout << "MP3 Glitch plug-in integration checks passed\n";
    return passed ? 0 : 1;
}
