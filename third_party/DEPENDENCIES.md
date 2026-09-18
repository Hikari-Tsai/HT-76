# Pinned dependencies

- JUCE 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850` from https://github.com/juce-framework/JUCE. CMake uses `third_party/JUCE` if available and otherwise fetches that exact revision. License: JUCE's own AGPLv3/commercial dual license; see `JUCE/LICENSE.md`.
- AAX SDK 2.9.0, bundled with that JUCE revision at `JUCE/modules/juce_audio_plugin_client/AAX/SDK`. Its license is retained at `SDK/LICENSE.txt`. Used for AAX Native development builds; this project does not include PACE signing credentials or claim retail Pro Tools signing.
- fetcomp-dsp, commit `de18f5ac793e36397c725abdca7fcb8c08760ce2` from https://github.com/Paulllux/fetcomp-dsp. MIT, Copyright (c) 2026 Paul Ulrix. Vendored headers, original README and LICENSE are retained. See `fetcomp-dsp/LOCAL_CHANGES.md` for the audio safety and meter adaptations.
- `MathUtils.h` credits the BSD math approximations from Chowdhury DSP's `chowdsp_utils`. Its attribution/license is retained separately under `licenses/`.

The project's UI assets were extracted from its generated concept artwork. No microphone recording or API credential is embedded in the plugin.
