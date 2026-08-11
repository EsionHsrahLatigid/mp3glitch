/*
  ==============================================================================
    MP3Glitch - Plugin Editor Implementation
  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
// GlitchSlider Implementation
//==============================================================================
GlitchSlider::GlitchSlider(const juce::String &labelText,
                           juce::AudioProcessorValueTreeState &apvts,
                           const juce::String &paramID) {
  slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
  slider.setColour(juce::Slider::rotarySliderFillColourId,
                   juce::Colour(0xff00ff88));
  slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                   juce::Colour(0xff333333));
  slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff0088));
  slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff00ff88));
  slider.setColour(juce::Slider::textBoxOutlineColourId,
                   juce::Colours::transparentBlack);
  addAndMakeVisible(slider);

  label.setText(labelText, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  label.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
  label.setFont(juce::Font(juce::FontOptions(12.0f)));
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
  addAndMakeVisible(glitchAmountSlider);
  addAndMakeVisible(frameCorruptionSlider);
  addAndMakeVisible(bitCrushSlider);
  addAndMakeVisible(repeatProbSlider);
  addAndMakeVisible(dropProbSlider);
  addAndMakeVisible(quantNoiseSlider);
  addAndMakeVisible(mdctSmearSlider);
  addAndMakeVisible(mixSlider);

  visualizerData.resize(128, 0.0f);

  setSize(600, 400);
  startTimerHz(30);
}

MP3GlitchAudioProcessorEditor::~MP3GlitchAudioProcessorEditor() { stopTimer(); }

void MP3GlitchAudioProcessorEditor::timerCallback() {
  // ビジュアライザーの更新
  visualizerPhase++;

  float targetLevel =
      *audioProcessor.getAPVTS().getRawParameterValue("glitchAmount");
  currentGlitchLevel += (targetLevel - currentGlitchLevel) * 0.1f;

  // グリッチ的なビジュアル更新
  for (size_t i = 0; i < visualizerData.size(); ++i) {
    float noise = (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
    float wave = std::sin(visualizerPhase * 0.1f + i * 0.2f);

    if (std::rand() / static_cast<float>(RAND_MAX) <
        currentGlitchLevel * 0.3f) {
      // グリッチ！
      visualizerData[i] = noise;
    } else {
      visualizerData[i] = wave * (1.0f - currentGlitchLevel * 0.5f) +
                          noise * currentGlitchLevel * 0.3f;
    }
  }

  repaint(0, 0, getWidth(), 120);
}

void MP3GlitchAudioProcessorEditor::paint(juce::Graphics &g) {
  // 背景
  g.fillAll(juce::Colour(0xff1a1a2e));

  // グリッドパターン（グリッチ風）
  g.setColour(juce::Colour(0x20ffffff));
  for (int x = 0; x < getWidth(); x += 20) {
    if (std::rand() % 10 < static_cast<int>(currentGlitchLevel * 5)) {
      // ランダムにラインをスキップ（グリッチ効果）
      continue;
    }
    g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
  }

  // タイトル
  g.setColour(juce::Colour(0xffff0088));
  g.setFont(juce::Font(juce::FontOptions(28.0f).withStyle("Bold")));

  juce::String title = "MP3 GLITCH";

  // グリッチテキスト効果
  if (currentGlitchLevel > 0.3f && std::rand() % 10 < 3) {
    int glitchOffset = (std::rand() % 5) - 2;
    g.setColour(juce::Colour(0xff00ffff));
    g.drawText(title, 10 + glitchOffset, 10 + glitchOffset, 200, 30,
               juce::Justification::left);
  }

  g.setColour(juce::Colour(0xffff0088));
  g.drawText(title, 10, 10, 200, 30, juce::Justification::left);

  // サブタイトル
  g.setColour(juce::Colour(0xff00ff88));
  g.setFont(juce::Font(juce::FontOptions(12.0f)));
  g.drawText("DATA CORRUPTION SIMULATOR", 10, 38, 250, 20,
             juce::Justification::left);

  // ビジュアライザー
  juce::Rectangle<float> vizBounds(10.0f, 60.0f, getWidth() - 20.0f, 50.0f);

  // ビジュアライザー背景
  g.setColour(juce::Colour(0xff0a0a15));
  g.fillRect(vizBounds);

  // 波形描画
  juce::Path wavePath;
  float centerY = vizBounds.getCentreY();
  float stepX = vizBounds.getWidth() / visualizerData.size();

  wavePath.startNewSubPath(vizBounds.getX(), centerY);

  for (size_t i = 0; i < visualizerData.size(); ++i) {
    float x = vizBounds.getX() + i * stepX;
    float y = centerY + visualizerData[i] * vizBounds.getHeight() * 0.4f;

    // たまにグリッチジャンプ
    if (currentGlitchLevel > 0.5f && std::rand() % 20 == 0) {
      y = centerY + ((std::rand() % 2) * 2 - 1) * vizBounds.getHeight() * 0.4f;
    }

    wavePath.lineTo(x, y);
  }

  g.setColour(juce::Colour(0xff00ff88));
  g.strokePath(wavePath, juce::PathStrokeType(2.0f));

  // スキャンライン効果
  g.setColour(juce::Colour(0x10ffffff));
  for (int y = 0; y < getHeight(); y += 3) {
    g.fillRect(0, y, getWidth(), 1);
  }

  // グリッチブロック（ランダムに表示）
  if (currentGlitchLevel > 0.4f) {
    int numBlocks = static_cast<int>(currentGlitchLevel * 5);
    for (int i = 0; i < numBlocks; ++i) {
      if (std::rand() % 3 == 0) {
        int blockX = std::rand() % getWidth();
        int blockY = std::rand() % getHeight();
        int blockW = 10 + std::rand() % 50;
        int blockH = 2 + std::rand() % 10;

        g.setColour(
            juce::Colour(static_cast<juce::uint8>(std::rand() % 255),
                         static_cast<juce::uint8>(std::rand() % 255),
                         static_cast<juce::uint8>(std::rand() % 255),
                         static_cast<juce::uint8>(50 + std::rand() % 100)));
        g.fillRect(blockX, blockY, blockW, blockH);
      }
    }
  }

  // ボーダー
  g.setColour(juce::Colour(0xffff0088));
  g.drawRect(getLocalBounds(), 2);
}

void MP3GlitchAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(120); // ビジュアライザー用のスペース
  bounds.reduce(10, 10);

  // スライダーを2行4列で配置
  const int sliderWidth = bounds.getWidth() / 4;
  const int sliderHeight = bounds.getHeight() / 2;

  auto row1 = bounds.removeFromTop(sliderHeight);
  glitchAmountSlider.setBounds(row1.removeFromLeft(sliderWidth));
  frameCorruptionSlider.setBounds(row1.removeFromLeft(sliderWidth));
  bitCrushSlider.setBounds(row1.removeFromLeft(sliderWidth));
  repeatProbSlider.setBounds(row1.removeFromLeft(sliderWidth));

  auto row2 = bounds;
  dropProbSlider.setBounds(row2.removeFromLeft(sliderWidth));
  quantNoiseSlider.setBounds(row2.removeFromLeft(sliderWidth));
  mdctSmearSlider.setBounds(row2.removeFromLeft(sliderWidth));
  mixSlider.setBounds(row2.removeFromLeft(sliderWidth));
}
