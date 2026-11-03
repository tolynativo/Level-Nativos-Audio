# LEVEL — Technical architecture and acceptance requirements

**Owner:** Nativos Audio — by Yai y Toly
**Preserve:** approved LEVEL dark charcoal / navy, warm-yellow theme, left module rail, clear waveform, accessible controls, no duplicate visual outlines or misplaced fonts. Exact X2 trademark font must be provided as a licensed asset; not in repository.

This document translates the supplied technical GDD into an implementation contract. It does not claim incomplete functions work.

## Stack
- C++20, JUCE, CMake/CPack, VST3 / AU (macOS) / Standalone, Mac Intel and Apple Silicon, Windows x64.
- APVTS for parameter state and XML preset automation.
- All AI inference on background worker threads, local licensed ONNX models and statically linked ONNX Runtime.
- Real-time callback: no heap allocations, no disk I/O, no mutexes, no model inference.

## Approved UI
- Left: Clean, Lowend, Gate, Dynamics, Breath, Sibilance, Mouth Noise with on/off.
- Upper center: cream waveform and orange editable time regions.
- Lower center: FFT spectrogram in black-to-orange/yellow with logarithmic 200 Hz to 16 kHz axis.
- Lower: real transport, zoom and region color legend.
- Right: selected DSP parameters and actual input/output meters.
- Settings: manual and safe update downloads.
- No fake or decorative controls presented as functional.

## DSP requirements
- Clean: STFT overlap-add, denoise, dereverb, restore highs.
- Lowend: adaptive low-cut, dynamic resonance and localized plosive repair.
- Gate/Dynamics: VAD-aware gating, LUFS-aware leveling, peak catching.
- Breath/Sibilance/Mouth Noise: model-assisted event reduction and spectral repair preserving consonants.
- Continuous audio/thread safety, latency and impulse response validation.

## AI requirements
- Local licensed ONNX models; verified checksums, sample rate/tensor validation.
- Speech, breath, sibilance, plosive and mouth-click detection; F0 tracking; separation when licensed/validated.
- Analysis on worker threads, with error isolation, no realtime stalls.
- Immutable snapshots transferred to realtime audio callback, never access UI-owned vector from processBlock.

## Non-destructive regions
- AudioRegion: identity, start/end, problem type, confidence, gain, enable and edit state.
- Accept, delete, add, resize, adjust gain, undo/redo and persistent project edits.
- Each region affects audio at sample-accurate DAW timeline positions with fades.
- Keep source recordings untouched.

## Files
- Async WAV/AIFF/FLAC import; AudioThumbnail waveform.
- FFT spectrogram computed off callback.
- Offline bounce to 16/24-bit PCM or 32-bit float WAV through separate processor, atomic destination replacement.
- Real standalone transport/loop/seek and DAW transfer/ARA integration.

## v0.4 implementation matrix
| Requirement | Status |
|---|---|
| C++20, JUCE, CMake, CPack source packaging | Configured, build unverified |
| Mac Universal VST3/AU and standalone package choices | Configured, install unverified |
| Windows VST3/Standalone targets | Configured, native installer unverified |
| APVTS, basic real-time DSP, preset save/load | Prototype only |
| In/out meters with zero at actual silence | Code implemented, DAW validation pending |
| Asynchronous AudioThumbnail and drag/drop file import | Source implemented, compilation pending |
| FFT spectrogram with 200 Hz–16 kHz logarithmic mapping | Source implemented, compilation pending |
| Manually add/delete regions and apply in offline render | Source implemented, compilation pending |
| 16/24/32-float WAV offline export | Source implemented, compilation pending |
| Resize handles, individual region gain, undo, persistence | Not implemented |
| Automated AI detection/ONNX linked to licensed inference models | Not implemented; optional build flag only |
| Full STFT noise/dereverb restoration | Not implemented |
| ARA, transfer capture, full standalone transport | Not implemented |
| Signed/notarized installer and verified in-plugin updates | Not implemented |
| X2 trademark font asset | Not available |

## Release acceptance gates
1. Green macOS and Windows CI builds, valid artifact downloads.
2. Verify arm64+x86_64 inside VST3, AU and standalone Mac app.
3. AU validation and VST3 host scan; DAW smoke tests in Ableton, Logic and REAPER.
4. UI screenshot QA 100/125/150/200% scaling and every supported window size.
5. Audio QA at 44.1/48/96kHz, buffers 32 through 2048, mono/stereo, silence/impulses/voice, CPU, memory.
6. Validated preset persistence, WAV import/export, region fades and no clicks/phase artifacts.
7. Licensed local ONNX models tested for accuracy and latency, robust fallback.
8. Signing, notarization, installer checksums and secure updater before public release.

Do not claim final parity with the reference plugin until every advertised function has been implemented, compiled, host-tested and verified.
