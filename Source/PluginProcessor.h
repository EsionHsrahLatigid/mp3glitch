/*
  ==============================================================================
    MP3Glitch - Real-time MP3 Data Corruption Audio Effect
    macOS VST3/AU Plugin using JUCE 8
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include <cmath>

//==============================================================================
// MP3風のグリッチエフェクトを生成するDSPクラス
//==============================================================================
class MP3GlitchProcessor
{
public:
    MP3GlitchProcessor();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    
    // パラメータ
    void setGlitchAmount(float amount) { glitchAmount = amount; }
    void setFrameCorruption(float amount) { frameCorruption = amount; }
    void setBitCrush(float amount) { bitCrushAmount = amount; }
    void setRepeatProbability(float prob) { repeatProbability = prob; }
    void setDropProbability(float prob) { dropProbability = prob; }
    void setQuantizationNoise(float amount) { quantizationNoise = amount; }
    void setMDCTSmear(float amount) { mdctSmear = amount; }
    void setMix(float mix) { dryWetMix = mix; }
    
private:
    // MP3エンコーダが使用するMDCT風の処理をシミュレート
    void simulateMDCT(float* data, int size);
    void simulateIMDCT(float* data, int size);
    void corruptMDCTCoefficients(std::vector<float>& coeffs);
    
    // フレームベースの破壊
    void processFrame(float* leftData, float* rightData, int frameSize);
    void repeatLastFrame();
    void dropFrame();
    
    // MP3特有のアーティファクト
    void applyPreEcho(juce::AudioBuffer<float>& buffer);
    void applyBandLimiting(juce::AudioBuffer<float>& buffer);
    void applyQuantizationDistortion(float* data, int size);
    
    double sampleRate = 44100.0;
    // MP3フレームサイズ (576 samples for Layer III)
    static constexpr int MP3_FRAME_SIZE = 576;
    
    // パラメータ
    float glitchAmount = 0.5f;
    float frameCorruption = 0.3f;
    float bitCrushAmount = 0.0f;
    float repeatProbability = 0.1f;
    float dropProbability = 0.05f;
    float quantizationNoise = 0.3f;
    float mdctSmear = 0.5f;
    float dryWetMix = 1.0f;
    
    // バッファ
    std::vector<float> frameBufferL;
    std::vector<float> frameBufferR;
    std::vector<float> lastFrameL;
    std::vector<float> lastFrameR;
    std::vector<float> mdctCoeffs;
    juce::AudioBuffer<float> dryBuffer;
    int framePosition = 0;
    int repeatCount = 0;
    
    // 乱数生成
    std::mt19937 rng;
    std::uniform_real_distribution<float> uniformDist{0.0f, 1.0f};
    std::normal_distribution<float> normalDist{0.0f, 1.0f};
    
    // フィルタ（バンド制限用）
    juce::dsp::IIR::Filter<float> lowpassFilterL;
    juce::dsp::IIR::Filter<float> lowpassFilterR;
    
    // Pre-echo用ディレイ
    std::array<float, 2048> preEchoDelayL{};
    std::array<float, 2048> preEchoDelayR{};
    int preEchoWritePos = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MP3GlitchProcessor)
};

//==============================================================================
class MP3GlitchAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MP3GlitchAudioProcessor();
    ~MP3GlitchAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateParameters();
    
    juce::AudioProcessorValueTreeState apvts;
    MP3GlitchProcessor glitchProcessor;
    
    std::atomic<float>* glitchAmountParam = nullptr;
    std::atomic<float>* frameCorruptionParam = nullptr;
    std::atomic<float>* bitCrushParam = nullptr;
    std::atomic<float>* repeatProbParam = nullptr;
    std::atomic<float>* dropProbParam = nullptr;
    std::atomic<float>* quantNoiseParam = nullptr;
    std::atomic<float>* mdctSmearParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MP3GlitchAudioProcessor)
};
