# Audio Output Latency Benchmark — q3now miniaudio backend

## Purpose

This document describes how to measure end-to-end audio output latency for
the q3now miniaudio backend (`code/client/snd_miniaudio.c`). The goal is to
verify the round-trip delay between an in-engine sound trigger (e.g. the
`snd_test` console command) and the moment the resulting waveform leaves the
sound card. The procedure here is intentionally low-tech and reproducible on
a developer workstation — no special CI hardware required.

## What we measure

We measure **command-to-output latency**: the wall-clock interval between
issuing a sound-emitting command on the q3now console and the corresponding
audio sample appearing at the OS playback boundary (or, ideally, at the
analog headphone jack). This captures every layer the audio has to traverse:

1. The engine's mixer (`snd_mix.c`, unchanged from upstream)
2. The miniaudio data callback in `snd_miniaudio.c`
3. The OS audio stack (CoreAudio / WASAPI / PulseAudio / PipeWire compat)
4. The hardware DAC and any analog output buffering

A measurement that only times steps 1–2 would tell us nothing useful about
real-world responsiveness, so we rely on a loopback that captures the signal
*after* it has crossed the OS boundary.

## Theoretical lower bound (miniaudio configuration)

The miniaudio device is configured with two periods and a period size derived
from the `s_latency` cvar (default 6 ms, clamped to [2, 20] ms). Sample rate
is taken from `s_khz` (default 48 kHz). The relevant code lives at
`code/client/snd_miniaudio.c` lines ~446–464.

The theoretical lower bound on output buffering is:

    latency_seconds = (periodSizeInFrames * periods) / sampleRate

For the current defaults:

    s_latency = 6 ms, s_khz = 48 → periodSizeInFrames = 288, periods = 2,
    sampleRate = 48000 → bound ≈ 288 × 2 / 48000 = 12.0 ms

Other configurations:

| s_latency | s_khz | periodSizeInFrames | periods | bound (ms) |
|-----------|-------|--------------------|---------|------------|
| 2 ms      | 48    | 96                 | 2       | 4.0        |
| 6 ms      | 48    | 288                | 2       | 12.0       |
| 6 ms      | 44.1  | ~265               | 2       | 12.0       |
| 20 ms     | 48    | 960                | 2       | 40.0       |

The OS audio stack and DAC will add additional buffering on top — typically
3–15 ms depending on platform — so realistic measurements should be:

- **CoreAudio (macOS)**: bound + 5–10 ms
- **WASAPI shared mode (Windows)**: bound + 10–20 ms
- **PulseAudio (Linux)**: bound + 5–15 ms
- **PipeWire compat (Linux)**: bound + 3–8 ms

## Method A — Hardware loopback (most accurate)

This method bypasses every software-loopback caveat and gives the most
trustworthy number. You need a 3.5 mm TRS male-male cable and a machine with
both a headphone jack and a line-in / mic-in jack.

### Setup

1. Plug one end of the cable into the headphone-out, the other into line-in.
2. Disable input boost / AGC on the line-in input (system sound prefs).
3. Set the input level so that a 1 kHz test tone records at roughly -12 dBFS
   without clipping.

### Capture

Record from the loopback input while running q3now:

    arecord -f S16_LE -r 48000 -c 2 -d 5 loopback.wav   # Linux
    rec -r 48000 -c 2 loopback.wav trim 0 5             # macOS / SoX
    # Or use Audacity → Record button → Stop after 5 s.

### Trigger

In the q3now console, with a stopwatch or a timestamped log:

    snd_test
    # or any single-shot sound effect

### Measure

Open the captured `loopback.wav` in Audacity (or `sox loopback.wav -n stat`
for envelope analysis). The delay is the offset from your `snd_test` issue
time to the first sample where the captured signal exceeds the noise floor.

If you want to remove the human-reaction component, instrument the engine to
print a microsecond timestamp from inside the miniaudio data callback the
moment it writes the first non-silent sample, then compare against the
captured-signal onset time using the system clock. (See "Future work" below.)

## Method B — Linux PulseAudio monitor loopback (no extra hardware)

Every PulseAudio sink exposes a `.monitor` source that captures whatever is
being played. Recording from the monitor source measures latency *up to* the
PulseAudio mixer — it does **not** capture DAC / analog buffering — but it is
quick and requires no cables.

### Setup

    # List sinks and find your default
    pactl list short sinks

    # Identify the matching monitor source (suffix .monitor)
    pactl list short sources | grep monitor

    # Optional: load a loopback module so the monitor is easier to capture
    pactl load-module module-loopback source=<sink-name>.monitor

### Capture

    arecord -D pulse -f S16_LE -r 48000 -c 2 -d 5 monitor.wav

### Trigger and measure

Same as Method A. Subtract the theoretical lower bound (12 ms at default
config) to get an estimate of just the OS-stack contribution.

### Cleanup

    pactl unload-module module-loopback   # if you loaded it

## Method C — macOS BlackHole / Loopback.app virtual driver

macOS does not expose a built-in monitor source. The two common workarounds:

- **BlackHole** (free, https://existential.audio/blackhole): a 2-channel
  virtual audio device. Set q3now's output device to BlackHole, then record
  from BlackHole as an input device in Audacity or QuickTime.
- **Loopback.app** (paid, Rogue Amoeba): GUI-driven virtual cable. Same
  approach: route q3now → Loopback → Audacity input.

Both require GUI configuration in System Settings → Sound; they cannot be
fully scripted. After setup, the capture/trigger/measure steps are identical
to Method A.

## Known limitation: baseline comparison

The original spec called for comparing miniaudio latency against the legacy
`sdl_snd.c` SDL2 audio backend that q3now inherited from quake3e. **That
file was deleted in this spec under task-5**, so the baseline cannot be
measured against the current working tree.

If you want a true before/after comparison, the most recent commit that
still contained `code/sdl/sdl_snd.c` is **`db0d777a`** ("feat: introduced
new libraries for audio and video processing"). To measure the baseline:

1. In a separate worktree or clone, `git checkout db0d777a`
2. Build q3now from that commit
3. Run the same Method A/B/C procedure on the SDL2 binary
4. Record the resulting latency
5. Switch back to current HEAD and re-run for the miniaudio number

Do **not** run `git checkout` inside this working tree — the in-progress
spec changes will be lost.

## How to interpret results

The acceptance criterion for spec priority **P2** ("reduce output latency")
is that the miniaudio backend is at least **5 ms faster** than the SDL2
baseline at the default config (`s_latency 6`, `s_khz 48`). Concretely:

- **Improvement ≥ 5 ms** → P2 confirmed, ship it.
- **Improvement 0–5 ms** → marginal; rerun on a quieter system to rule out
  measurement noise. If still <5 ms, file a follow-up to investigate the
  miniaudio period sizing.
- **Regression (miniaudio slower)** → P2 violated. Do not ship without
  explicit user override. Likely causes: PulseAudio over-buffering, period
  count mismatch, or `s_latency` clamp interacting badly with the chosen
  sample rate. Inspect with `s_info` to see the actually-negotiated values.

Always report: platform, sample rate, `s_latency`, capture method, mean of
≥10 trials, and the standard deviation. A single measurement is not
evidence.

## Future work

- Instrument `snd_miniaudio.c` to log a microsecond-precision timestamp
  when the data callback first writes a non-silent sample after a trigger,
  so the human-reaction component can be eliminated.
- Add a `snd_latencytest` console command that emits a calibration click
  with an embedded timestamp marker and prints the measured round-trip if
  given a path to the captured WAV.
- If a CI runner with audio hardware (or a stable virtual driver) becomes
  available, automate Method B as a regression test that fails the build
  if measured latency exceeds 1.5× the theoretical lower bound.

See `tools/measure_audio_latency.sh` for a platform-detection wrapper that
prints the relevant subset of these instructions.
