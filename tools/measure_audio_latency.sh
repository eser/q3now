#!/usr/bin/env bash
# measure_audio_latency.sh
#
# Prints platform-appropriate audio latency measurement instructions for
# the q3now miniaudio backend. This script is informational only — actual
# measurement requires running the engine and physical/virtual loopback.
#
# Companion to tools/audio_latency_benchmark.md.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLATFORM="$(uname -s)"

echo "q3now audio latency benchmark — instructions"
echo "============================================="
echo ""
echo "Platform: $PLATFORM"
echo "Repo:     $REPO_ROOT"
echo ""
echo "Theoretical lower bound for miniaudio (periods=2):"
echo "  s_latency=6  s_khz=48  -> 288 frames * 2 / 48000 = 12.0 ms"
echo "  s_latency=6  s_khz=44  -> ~265 frames * 2 / 44100 = 12.0 ms"
echo "  s_latency=2  s_khz=48  ->  96 frames * 2 / 48000 =  4.0 ms"
echo "  s_latency=20 s_khz=48  -> 960 frames * 2 / 48000 = 40.0 ms"
echo ""
echo "Actual latency depends on the OS audio stack and DAC buffering."
echo ""

case "$PLATFORM" in
    Linux)
        echo "--- Linux: PulseAudio monitor loopback (Method B) ---"
        echo ""
        echo "1. Discover the default sink and matching .monitor source:"
        echo ""
        echo "   pactl list short sinks"
        echo "   pactl list short sources | grep monitor"
        echo ""
        echo "2. (Optional) Load a loopback module so the monitor is easy to capture:"
        echo ""
        echo "   pactl load-module module-loopback source=<sink-name>.monitor"
        echo ""
        echo "3. Capture 5 s from the monitor while q3now plays a sound:"
        echo ""
        echo "   arecord -D pulse -f S16_LE -r 48000 -c 2 -d 5 monitor.wav"
        echo ""
        echo "4. In the q3now console, trigger a sound (e.g. 'snd_test')."
        echo "5. Open monitor.wav in Audacity and measure the offset from"
        echo "   command-time to first non-silent sample."
        echo "6. Cleanup:  pactl unload-module module-loopback"
        echo ""
        if command -v pactl >/dev/null 2>&1 && command -v arecord >/dev/null 2>&1; then
            echo "(pactl and arecord are installed — you can run the steps above as-is.)"
        else
            echo "(Install pulseaudio-utils + alsa-utils to get pactl + arecord.)"
        fi
        ;;
    Darwin)
        echo "--- macOS: BlackHole / Loopback.app virtual driver (Method C) ---"
        echo ""
        echo "macOS does not expose a built-in monitor source. Use either:"
        echo ""
        echo "  - BlackHole (free):  https://existential.audio/blackhole"
        echo "  - Loopback.app  :    https://rogueamoeba.com/loopback/"
        echo ""
        echo "Setup (GUI, cannot be scripted):"
        echo "  1. Install BlackHole 2ch (or Loopback.app)."
        echo "  2. System Settings -> Sound -> Output: select BlackHole 2ch."
        echo "  3. Open Audacity -> set input device to BlackHole 2ch."
        echo "  4. Hit record in Audacity, then trigger 'snd_test' in q3now."
        echo "  5. Stop recording, measure offset to first non-silent sample."
        echo ""
        echo "For the most accurate number, prefer Method A (hardware loopback)"
        echo "with a 3.5mm cable from headphone-out to line-in."
        ;;
    *)
        echo "--- Unknown platform: hardware loopback (Method A) ---"
        echo ""
        echo "1. Connect headphone-out to line-in with a 3.5mm TRS cable."
        echo "2. Disable input boost / AGC on the line-in input."
        echo "3. Capture audio with your platform's recording tool."
        echo "4. Trigger 'snd_test' in the q3now console."
        echo "5. Measure the offset from trigger time to first non-silent"
        echo "   sample in the captured file."
        ;;
esac

echo ""
echo "============================================="
echo ""
echo "See tools/audio_latency_benchmark.md for the full methodology and"
echo "interpretation guide."
echo ""
echo "Known limitation: sdl_snd.c was deleted in spec task-5. To compare"
echo "against the legacy SDL2 audio baseline, git checkout commit db0d777a"
echo "in a SEPARATE worktree before measuring (do not checkout in this"
echo "working tree)."
echo ""
exit 0
