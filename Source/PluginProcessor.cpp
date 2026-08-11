/*
  ==============================================================================
    MP3Glitch - Real-time MP3 Data Corruption Audio Effect
    Implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// MP3GlitchProcessor Implementation
//==============================================================================
MP3GlitchProcessor::MP3GlitchProcessor() : rng(std::random_device{}()) {
  frameBufferL.resize(MP3_FRAME_SIZE * 2, 0.0f);
  frameBufferR.resize(MP3_FRAME_SIZE * 2, 0.0f);
  lastFrameL.resize(MP3_FRAME_SIZE, 0.0f);
  lastFrameR.resize(MP3_FRAME_SIZE, 0.0f);
  mdctCoeffs.resize(MP3_FRAME_SIZE, 0.0f);
}

void MP3GlitchProcessor::prepare(double sr, int samplesPerBlock) {
  juce::ignoreUnused(samplesPerBlock);
  sampleRate = sr;

  // ローパスフィルタの設定（MP3の16kHz帯域制限をシミュレート）
  auto coeffs =
      juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 16000.0f);
  lowpassFilterL.coefficients = coeffs;
  lowpassFilterR.coefficients = coeffs;

  reset();
}

void MP3GlitchProcessor::reset() {
  std::fill(frameBufferL.begin(), frameBufferL.end(), 0.0f);
  std::fill(frameBufferR.begin(), frameBufferR.end(), 0.0f);
  std::fill(lastFrameL.begin(), lastFrameL.end(), 0.0f);
  std::fill(lastFrameR.begin(), lastFrameR.end(), 0.0f);
  std::fill(preEchoDelayL.begin(), preEchoDelayL.end(), 0.0f);
  std::fill(preEchoDelayR.begin(), preEchoDelayR.end(), 0.0f);

  framePosition = 0;
  preEchoWritePos = 0;
  repeatCount = 0;

  lowpassFilterL.reset();
  lowpassFilterR.reset();
}

void MP3GlitchProcessor::process(juce::AudioBuffer<float> &buffer) {
  const int numSamples = buffer.getNumSamples();
  const int numChannels = buffer.getNumChannels();

  if (numChannels < 1)
    return;

  // ドライ信号を保存
  juce::AudioBuffer<float> dryBuffer;
  dryBuffer.makeCopyOf(buffer);

  float *leftChannel = buffer.getWritePointer(0);
  float *rightChannel = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

  for (int i = 0; i < numSamples; ++i) {
    // フレームバッファに蓄積
    frameBufferL[framePosition] = leftChannel[i];
    if (rightChannel)
      frameBufferR[framePosition] = rightChannel[i];

    framePosition++;

    // フレームが完成したら処理
    if (framePosition >= MP3_FRAME_SIZE) {
      processFrame(frameBufferL.data(), frameBufferR.data(), MP3_FRAME_SIZE);

      // 前のフレームを保存
      std::copy(frameBufferL.begin(), frameBufferL.begin() + MP3_FRAME_SIZE,
                lastFrameL.begin());
      std::copy(frameBufferR.begin(), frameBufferR.begin() + MP3_FRAME_SIZE,
                lastFrameR.begin());

      framePosition = 0;
    }

    // 処理済みサンプルを出力
    int outputPos =
        (framePosition > 0) ? framePosition - 1 : MP3_FRAME_SIZE - 1;
    leftChannel[i] = frameBufferL[outputPos];
    if (rightChannel)
      rightChannel[i] = frameBufferR[outputPos];
  }

  // バンド制限を適用（MP3の高域カット）
  if (glitchAmount > 0.3f) {
    applyBandLimiting(buffer);
  }

  // プリエコーを適用
  if (glitchAmount > 0.5f) {
    applyPreEcho(buffer);
  }

  // ドライ/ウェットミックス
  if (dryWetMix < 1.0f) {
    for (int ch = 0; ch < numChannels; ++ch) {
      float *wet = buffer.getWritePointer(ch);
      const float *dry = dryBuffer.getReadPointer(ch);

      for (int i = 0; i < numSamples; ++i) {
        wet[i] = dry[i] * (1.0f - dryWetMix) + wet[i] * dryWetMix;
      }
    }
  }
}

void MP3GlitchProcessor::processFrame(float *leftData, float *rightData,
                                      int frameSize) {
  // フレームドロップの確率チェック
  if (uniformDist(rng) < dropProbability * glitchAmount) {
    dropFrame();
    return;
  }

  // フレームリピートの確率チェック
  if (uniformDist(rng) < repeatProbability * glitchAmount) {
    repeatLastFrame();
    std::copy(lastFrameL.begin(), lastFrameL.end(), leftData);
    if (rightData)
      std::copy(lastFrameR.begin(), lastFrameR.end(), rightData);
    return;
  }

  // MDCT風の周波数領域処理
  if (mdctSmear > 0.0f) {
    simulateMDCT(leftData, frameSize);
    corruptMDCTCoefficients(mdctCoeffs);
    simulateIMDCT(leftData, frameSize);

    if (rightData) {
      simulateMDCT(rightData, frameSize);
      corruptMDCTCoefficients(mdctCoeffs);
      simulateIMDCT(rightData, frameSize);
    }
  }

  // 量子化ノイズ
  if (quantizationNoise > 0.0f) {
    applyQuantizationDistortion(leftData, frameSize);
    if (rightData)
      applyQuantizationDistortion(rightData, frameSize);
  }

  // ビットクラッシュ
  if (bitCrushAmount > 0.0f) {
    int bits =
        static_cast<int>(16.0f - bitCrushAmount * 12.0f); // 16bit -> 4bit
    float scale = std::pow(2.0f, static_cast<float>(bits));

    for (int i = 0; i < frameSize; ++i) {
      leftData[i] = std::round(leftData[i] * scale) / scale;
      if (rightData)
        rightData[i] = std::round(rightData[i] * scale) / scale;
    }
  }

  // フレーム境界でのランダムなバイト破壊をシミュレート
  if (uniformDist(rng) < frameCorruption * glitchAmount) {
    int corruptStart = static_cast<int>(uniformDist(rng) * (frameSize - 32));
    int corruptLength = static_cast<int>(uniformDist(rng) * 32) + 8;

    for (int i = corruptStart;
         i < corruptStart + corruptLength && i < frameSize; ++i) {
      // バイトレベルの破壊をシミュレート
      float corruptionType = uniformDist(rng);

      if (corruptionType < 0.3f) {
        // ビットフリップ風
        leftData[i] = -leftData[i];
      } else if (corruptionType < 0.6f) {
        // ランダムな値に置換
        leftData[i] = normalDist(rng) * 0.5f;
      } else {
        // 前のサンプルをコピー（スタッター）
        if (i > 0)
          leftData[i] = leftData[i - 1];
      }

      if (rightData) {
        if (corruptionType < 0.3f)
          rightData[i] = -rightData[i];
        else if (corruptionType < 0.6f)
          rightData[i] = normalDist(rng) * 0.5f;
        else if (i > 0)
          rightData[i] = rightData[i - 1];
      }
    }
  }
}

void MP3GlitchProcessor::simulateMDCT(float *data, int size) {
  // 簡略化したMDCT風の処理
  // 実際のMP3は576点を18点のサブバンドに分割してMDCT
  mdctCoeffs.resize(size);

  for (int k = 0; k < size; ++k) {
    float sum = 0.0f;
    for (int n = 0; n < size; ++n) {
      float phase = static_cast<float>(M_PI) / size *
                    (static_cast<float>(n) + 0.5f + size / 4.0f) *
                    (static_cast<float>(k) + 0.5f);
      sum += data[n] * std::cos(phase);
    }
    mdctCoeffs[k] = sum;
  }
}

void MP3GlitchProcessor::simulateIMDCT(float *data, int size) {
  // 逆MDCT
  for (int n = 0; n < size; ++n) {
    float sum = 0.0f;
    for (int k = 0; k < size; ++k) {
      float phase = static_cast<float>(M_PI) / size *
                    (static_cast<float>(n) + 0.5f + size / 4.0f) *
                    (static_cast<float>(k) + 0.5f);
      sum += mdctCoeffs[k] * std::cos(phase);
    }
    data[n] = sum * 2.0f / size;
  }
}

void MP3GlitchProcessor::corruptMDCTCoefficients(std::vector<float> &coeffs) {
  // MP3の量子化エラーをシミュレート
  for (size_t i = 0; i < coeffs.size(); ++i) {
    if (uniformDist(rng) < mdctSmear * glitchAmount * 0.3f) {
      float corruptType = uniformDist(rng);

      if (corruptType < 0.25f) {
        // 係数をゼロに（帯域消失）
        coeffs[i] = 0.0f;
      } else if (corruptType < 0.5f) {
        // スケールファクタエラー
        coeffs[i] *= (1.0f + normalDist(rng) * 2.0f);
      } else if (corruptType < 0.75f) {
        // 隣接係数とスワップ
        if (i + 1 < coeffs.size())
          std::swap(coeffs[i], coeffs[i + 1]);
      } else {
        // ランダムノイズ追加
        coeffs[i] += normalDist(rng) * std::abs(coeffs[i]) * 0.5f;
      }
    }
  }
}

void MP3GlitchProcessor::repeatLastFrame() {
  repeatCount++;
  // リピート回数に応じて劣化を加える
  if (repeatCount > 3) {
    for (size_t i = 0; i < lastFrameL.size(); ++i) {
      lastFrameL[i] *= 0.95f;
      lastFrameR[i] *= 0.95f;
    }
  }
}

void MP3GlitchProcessor::dropFrame() {
  // フレームドロップ時はゼロで埋める（無音グリッチ）
  std::fill(frameBufferL.begin(), frameBufferL.begin() + MP3_FRAME_SIZE, 0.0f);
  std::fill(frameBufferR.begin(), frameBufferR.begin() + MP3_FRAME_SIZE, 0.0f);
}

void MP3GlitchProcessor::applyPreEcho(juce::AudioBuffer<float> &buffer) {
  // MP3特有のプリエコーをシミュレート
  // トランジェント前に微弱な信号が漏れる現象
  const int preEchoDelay = static_cast<int>(0.02 * sampleRate); // 20ms
  const float preEchoLevel = 0.15f * glitchAmount;

  float *leftChannel = buffer.getWritePointer(0);
  float *rightChannel =
      buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

  for (int i = 0; i < buffer.getNumSamples(); ++i) {
    int readPos = (preEchoWritePos - preEchoDelay + preEchoDelayL.size()) %
                  preEchoDelayL.size();

    leftChannel[i] += preEchoDelayL[readPos] * preEchoLevel;
    preEchoDelayL[preEchoWritePos] = leftChannel[i];

    if (rightChannel) {
      rightChannel[i] += preEchoDelayR[readPos] * preEchoLevel;
      preEchoDelayR[preEchoWritePos] = rightChannel[i];
    }

    preEchoWritePos = (preEchoWritePos + 1) % preEchoDelayL.size();
  }
}

void MP3GlitchProcessor::applyBandLimiting(juce::AudioBuffer<float> &buffer) {
  // MP3の高域制限をシミュレート
  float *leftChannel = buffer.getWritePointer(0);
  for (int i = 0; i < buffer.getNumSamples(); ++i) {
    leftChannel[i] = lowpassFilterL.processSample(leftChannel[i]);
  }

  if (buffer.getNumChannels() > 1) {
    float *rightChannel = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      rightChannel[i] = lowpassFilterR.processSample(rightChannel[i]);
    }
  }
}

void MP3GlitchProcessor::applyQuantizationDistortion(float *data, int size) {
  // MP3の量子化ノイズをシミュレート
  // 低ビットレートで顕著になる
  const float noiseLevel = quantizationNoise * glitchAmount * 0.02f;

  for (int i = 0; i < size; ++i) {
    data[i] += normalDist(rng) * noiseLevel;

    // 小さな信号は量子化でゼロになりやすい
    if (std::abs(data[i]) < 0.001f &&
        uniformDist(rng) < quantizationNoise * 0.5f) {
      data[i] = 0.0f;
    }
  }
}

//==============================================================================
// MP3GlitchAudioProcessor Implementation
//==============================================================================
MP3GlitchAudioProcessor::MP3GlitchAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  glitchAmountParam = apvts.getRawParameterValue("glitchAmount");
  frameCorruptionParam = apvts.getRawParameterValue("frameCorruption");
  bitCrushParam = apvts.getRawParameterValue("bitCrush");
  repeatProbParam = apvts.getRawParameterValue("repeatProb");
  dropProbParam = apvts.getRawParameterValue("dropProb");
  quantNoiseParam = apvts.getRawParameterValue("quantNoise");
  mdctSmearParam = apvts.getRawParameterValue("mdctSmear");
  mixParam = apvts.getRawParameterValue("mix");
}

MP3GlitchAudioProcessor::~MP3GlitchAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
MP3GlitchAudioProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"glitchAmount", 1}, "Glitch Amount",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"frameCorruption", 1}, "Frame Corruption",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"bitCrush", 1}, "Bit Crush",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"repeatProb", 1}, "Frame Repeat",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"dropProb", 1}, "Frame Drop",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.05f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"quantNoise", 1}, "Quantization Noise",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mdctSmear", 1}, "MDCT Smear",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"mix", 1}, "Dry/Wet Mix",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

  return {params.begin(), params.end()};
}

void MP3GlitchAudioProcessor::updateParameters() {
  glitchProcessor.setGlitchAmount(*glitchAmountParam);
  glitchProcessor.setFrameCorruption(*frameCorruptionParam);
  glitchProcessor.setBitCrush(*bitCrushParam);
  glitchProcessor.setRepeatProbability(*repeatProbParam);
  glitchProcessor.setDropProbability(*dropProbParam);
  glitchProcessor.setQuantizationNoise(*quantNoiseParam);
  glitchProcessor.setMDCTSmear(*mdctSmearParam);
  glitchProcessor.setMix(*mixParam);
}

//==============================================================================
const juce::String MP3GlitchAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool MP3GlitchAudioProcessor::acceptsMidi() const { return false; }
bool MP3GlitchAudioProcessor::producesMidi() const { return false; }
bool MP3GlitchAudioProcessor::isMidiEffect() const { return false; }
double MP3GlitchAudioProcessor::getTailLengthSeconds() const { return 0.1; }

int MP3GlitchAudioProcessor::getNumPrograms() { return 1; }
int MP3GlitchAudioProcessor::getCurrentProgram() { return 0; }
void MP3GlitchAudioProcessor::setCurrentProgram(int) {}
const juce::String MP3GlitchAudioProcessor::getProgramName(int) { return {}; }
void MP3GlitchAudioProcessor::changeProgramName(int, const juce::String &) {}

//==============================================================================
void MP3GlitchAudioProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock) {
  glitchProcessor.prepare(sampleRate, samplesPerBlock);
}

void MP3GlitchAudioProcessor::releaseResources() { glitchProcessor.reset(); }

bool MP3GlitchAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

void MP3GlitchAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                           juce::MidiBuffer &) {
  juce::ScopedNoDenormals noDenormals;

  updateParameters();
  glitchProcessor.process(buffer);
}

//==============================================================================
bool MP3GlitchAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *MP3GlitchAudioProcessor::createEditor() {
  return new MP3GlitchAudioProcessorEditor(*this);
}

//==============================================================================
void MP3GlitchAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void MP3GlitchAudioProcessor::setStateInformation(const void *data,
                                                  int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new MP3GlitchAudioProcessor();
}
