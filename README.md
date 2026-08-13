# MP3 Glitch

MP3 Glitch is a JUCE audio effect that simulates real-time MP3 data-corruption artifacts with frame repeats, frame drops, MDCT-style smearing, quantization noise, pre-echo, and bandwidth limiting.

## Identity

- Company: EsionHsrahLatigid
- Bundle ID: `jp.ehl.mp3glitch`
- Manufacturer code: `EHL_`
- Plug-in code: `Mp3G`
- Formats: VST3, Standalone, and AU on macOS

## Build

```bash
cmake -S . -B build/plugin -DCMAKE_BUILD_TYPE=Release -DMP3GLITCH_BUILD_PLUGIN=ON -DMP3GLITCH_BUILD_TESTS=ON
cmake --build build/plugin --target ehl_stage_products MP3GlitchIntegrationTests --parallel 2
ctest --test-dir build/plugin --output-on-failure
```

On local macOS builds outside CI, VST3 and AU products are also copied to the current user's standard plug-in folders. Override this with `-DEHL_COPY_PLUGIN_AFTER_BUILD=ON|OFF`. Standalone products are not copied to `Audio/Plug-Ins`; they remain in the build and artifact trees.

Stable artifacts are staged under `artifacts/plugin-release/<platform>/`:

- `artifacts/plugin-release/macos-arm64/vst3/mp3glitch_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/mp3glitch_au_plugin.component` on macOS
- `artifacts/plugin-release/macos-arm64/standalone/mp3glitch_standalone_plugin.app` on macOS
- `artifacts/plugin-release/windows-x64/vst3/mp3glitch_vst3_plugin.vst3`
- `artifacts/plugin-release/windows-x64/standalone/mp3glitch_standalone_plugin.exe`
- `artifacts/plugin-release/linux-x64/vst3/mp3glitch_vst3_plugin.vst3`
- `artifacts/plugin-release/linux-x64/standalone/mp3glitch_standalone_plugin`

## Parameters

- Glitch: master glitch amount
- Corrupt: frame corruption probability
- Bitcrush: bit-depth reduction amount
- Repeat: frame-repeat probability
- Drop: frame-drop probability
- Q-Noise: quantization noise amount
- MDCT: frequency-domain coefficient smear amount
- Mix: dry/wet balance

## Codec and License Notes

This plug-in does not encode or decode MP3 data and does not link to an MP3 codec. The MP3 frame and MDCT behavior is simulated directly in DSP code, so there is no bundled MP3 codec dependency.

The source is licensed under the MIT License. JUCE is fetched at configure time and remains subject to the JUCE license terms selected by the builder.
