# Changelog

All notable changes to q3now are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

- **BREAKING (USE_SDL=0 builds only)**: Replaced the three legacy platform audio backends
  (`win_snd.c` DirectSound, `linux_snd.c` ALSA/OSS, `sdl_snd.c` SDL2 Audio) with a single
  cross-platform `snd_miniaudio.c` using miniaudio v0.11.25. Standard `USE_SDL=1` builds
  see no behavioral break — they switch from SDL2 audio to miniaudio's WASAPI/CoreAudio/
  PulseAudio paths transparently.

### Removed

- `code/sdl/sdl_snd.c`, `code/unix/linux_snd.c`, `code/win32/win_snd.c` — replaced by
  `code/client/snd_miniaudio.c`
- `s_useOpenAL`, `s_alGain`, `s_alDopplerFactor`, `s_alDriver`, and other `s_al*` cvars —
  if any existed in the user config, they are now ignored (no-op)

### Added

- `code/client/snd_miniaudio.c` — single audio backend
- `code/client/miniaudio.h` — vendored single-header library at v0.11.25
- New cvars: `s_device` (audio device override), `s_latency` (period size hint in ms,
  range 2–20, default 6), `s_underruns` (read-only diagnostic counter)
- New console command: `snd_test` (plays a 2-second 1 kHz sine sweep through both channels
  for testing audio output)
- New Wired UI audio settings panel (`modfiles/ui/sound.wmenu`) with device dropdown,
  sample-rate selector, latency slider, test button, and waveform visualization
- New HUD element: `audio_waveform` — renders ~120 vertical bars of recent RMS levels
  for visual confirmation that audio is playing
- New Wired UI primitive: dynamic dropdown via `populateCallback` keyword (extends
  the existing MULTI widget; first user is the audio device list)
- `tools/check_audio_callback.sh` — static analysis ensuring `S_MiniaudioCallback`
  contains zero forbidden tokens (mutex/malloc/log/cvar mutation)
- `code/client/snd_miniaudio_test.c` — 11 standalone unit tests for the lock-free
  ring buffer (wraparound, underrun, overrun, concurrent reader/writer, RMS validation)
- `tools/audio_latency_benchmark.md` and `tools/measure_audio_latency.sh` — methodology
  for measuring audio output latency
- `tools/audio_manual_test_plan.md` — 24-scenario manual gameplay test checklist
- `.github/workflows/audio-test.yml` — runs unit tests + static analysis on every
  push/PR touching audio files
