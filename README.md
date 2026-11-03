# LEVEL — Nativos Audio
**by Yai y Toly**

Development VST3/AU vocal processor for macOS and VST3 for Windows.

## Build
GitHub Actions runs on push to main. Download macOS artifacts from Actions > Build LEVEL Installers. macOS package is **unsigned**, intended for testing only. Universal target arm64+x86_64. The Mac Installer has individually selectable VST3 and AU components, Nativos Music Audio welcome page and manual.

## Important
This is an early **real-time DSP foundation**, NOT VoiceAssist-equivalent AI analysis or spectral editing. The displayed waveform shows recent audio only. No automatic updater is shipped: Settings > Updates explains that verified signed releases are required. Do not distribute without audio/host testing, code signing and notarization.

## Meter behavior
Both input and output meters read real samples; when the input and output are silent, bars read zero (no decorative or simulated signal).

## Manual
Bundled under Resources/Level-Manual.html and linked from Settings when the plugin is installed. X2 Compressor trademark font has not yet been supplied and is not included.

## v0.3.0 implementation note
The interface was revised with full-size waveform viewing, seven selectable module pages, actual input/output knobs and improved non-overlapping layout. The meters read real audio and reset to zero on a silent input block. This is a basic realtime processor; AI event detection, offline clip editing, region handles, spectral repair, ARA transfer, and a verified update downloader are NOT implemented. The visual trademark font is not included without the X2 font asset. Mac installer build and host audio tests are pending.

## v0.4 engineering milestone
- C++20, JUCE VST3/AU/Standalone targets and CPack source support.
- Async AudioThumbnail for WAV/AIFF/FLAC import; dark/yellow approved palette retained.
- FFT spectrogram viewer (200 Hz–16 kHz, off audio callback).
- UI-only non-destructive manual region markers (drag to add, right-click to remove); not applied to the DSP chain yet.
- Separate worker for 24-bit WAV export with isolated Level processor; output saved via temporary file.
- Standalone Mac component added to optional installer choices.
- Region manager CTest coverage (smoke tests; no DAW host verification yet).
- ONNX dependency opt-in remains OFF until licensed models and platform static libraries are provided. There is no trained AI segmentation, source separation, automated detection, ARA, offline region application, or complete spectral restoration.

**Do not call this a finished VoiceAssist-equivalent release. Mac/Windows builds, audio behavior, artifacts, and UI must be validated in their host environments.**
