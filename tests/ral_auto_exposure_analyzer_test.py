#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
"""Exercise the native exposure gate without a window or GPU."""
import json
import re
from pathlib import Path
import subprocess
import tempfile
import unittest

HARNESS = Path(__file__).with_name("ral-auto-exposure-check.sh")


def scenario(mutation=None):
    rows = []
    frame = 0
    # Analytic exposure for a single occupied EV bin (61 and 78), independently
    # evaluated constants, not outputs harvested from the runtime accumulator.
    dark, bright = 1.7221536801131567, 0.9891175496990373

    def message(text, category="system", severity="INFO"):
        rows.append({"sev": severity, "cat": category, "msg": text + "\n"})

    def sample(value):
        nonlocal frame
        frame += 1
        message(f"EXPSERIES frame={frame} exposure_bias={value:.5f}",
                "renderer.ral", "WARN")

    message("window-extent schema=3 requested=1280x720 logical=1280x720 "
            "pixels=2560x1440 publish-ready=1")
    for rate, swing in (("LEGACY", .01), ("MIDDLE", .006), ("CURRENT", .003)):
        for phase in ("FLICKER", "AB", "BA"):
            start = bright if phase == "BA" else dark
            target = dark if phase == "BA" else bright
            for _ in range(16):
                sample(start)
            message(f"Q3_AE_{rate}_{phase}_BEGIN")
            for index in range(300):
                if phase == "FLICKER":
                    value = dark + (swing if index % 2 else -swing)
                else:
                    # Delayed GPU readback followed by a fast, monotonic transition.
                    fraction = max(0.0, min(1.0, (index - 4) / 8.0))
                    value = start + fraction * (target - start)
                    if rate == "CURRENT" and mutation == phase + "_OVERSHOOT" and index == 2:
                        value = 1.9 if phase == "AB" else .8
                    if rate == "CURRENT" and mutation == phase + "_UNDERSHOOT" and index == 20:
                        value = .8 if phase == "AB" else 1.9
                    if rate == "CURRENT" and mutation == phase + "_STALLED":
                        value = start
                    if rate == "LEGACY" and mutation == phase + "_WRONG_TARGET":
                        value += 0.1
                sample(value)
            black = 1000000 if mutation == phase + "_BLACK" else 1000
            bins = [0] * 256
            bins[0] = black
            bins[61 if phase != "AB" else 78] = 1000000 - black
            if mutation == "CLIPPED_EXTREMES":
                # Exactly 50% dark tail + 20% bright tail. The surviving 30%
                # has the same single-bin target; true black is not metered.
                bins[1] = 499500
                bins[255] = 199800
                bins[61 if phase != "AB" else 78] = 299700
            peak = max(range(256), key=bins.__getitem__)
            message(f"hdr histogram readback: total=1000000 peakBin={peak}(count={bins[peak]}) "
                    f"bins[0]={black} [64]={bins[64]} [128]={bins[128]} [192]={bins[192]} [255]={bins[255]}",
                    "renderer.ral", "WARN")
            for offset in range(0, 256, 64):
                if mutation == phase + "_MISSING_CHUNK" and offset == 192:
                    continue
                chunk = ",".join(str(value) for value in bins[offset:offset+64])
                message(f"HISTAUDIT start={offset} bins={chunk}", "renderer.ral", "WARN")
            if phase in ("AB", "BA") and mutation != phase + "_MISSING_CAMERA":
                camera = [1075,1432,77,0,0] if phase == "AB" else [1035,1448,77,135,0]
                if rate == "MIDDLE" and mutation == phase + "_CAMERA_DRIFT":
                    camera[3] += 1
                message(" ".join(map(str, camera)), "cgame")
            message(f"Q3_AE_{rate}_{phase}_END")
    message("Q3_AE_COMPLETE")
    return rows


class ExposureAnalyzerTest(unittest.TestCase):
    def test_bootstrap_fits_engine_startup_command_budget(self):
        # Com_ParseCommandLine ignores commands beyond MAX_CONSOLE_LINES.
        # In particular, the final +exec must survive additions to this list.
        source = (HARNESS.parent.parent / "code/qcommon/common.c").read_text()
        limit = int(re.search(r"#define\s+MAX_CONSOLE_LINES\s+(\d+)", source)[1])
        invocation = HARNESS.read_text().split('python3 "$TIMEOUT_RUNNER"', 1)[1].split('\nRC=$?', 1)[0]
        commands = re.findall(r"\+(?:set|exec)\b", invocation)
        self.assertLessEqual(len(commands), limit)
        self.assertEqual(commands[-1], "+exec")

    def analyze(self, mutation=None):
        with tempfile.TemporaryDirectory(prefix="ral-exposure-oracle-") as temp:
            root = Path(temp)
            log = root / "qconsole.jsonl"
            log.write_text("".join(json.dumps(row) + "\n" for row in scenario(mutation)))
            manifest = root / "manifest.jsonl"
            manifest.write_text(json.dumps({"kind": "result", "rc": 0, "timeout": False}) + "\n")
            return subprocess.run(["bash", str(HARNESS), "--analyze", str(log), str(manifest)],
                                  capture_output=True, text=True, timeout=10)

    def test_fast_monotonic_transition_is_not_ringing(self):
        result = self.analyze()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_actual_overshoot_and_undershoot_still_fail(self):
        for phase in ("AB", "BA"):
            for defect in ("OVERSHOOT", "UNDERSHOOT"):
                with self.subTest(phase=phase, defect=defect):
                    result = self.analyze(phase + "_" + defect)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("overshoot/ringing", result.stdout + result.stderr)

    def test_stalled_transition_still_fails(self):
        for phase in ("AB", "BA"):
            with self.subTest(phase=phase):
                result = self.analyze(phase + "_STALLED")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("transition convergence", result.stdout + result.stderr)

    def test_black_camera_cannot_pass_as_a_stable_endpoint(self):
        for phase in ("AB", "BA"):
            with self.subTest(phase=phase):
                result = self.analyze(phase + "_BLACK")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("unmetered scene", result.stdout + result.stderr)

    def test_stable_but_wrong_accumulator_cannot_define_its_own_target(self):
        for phase in ("AB", "BA"):
            with self.subTest(phase=phase):
                result = self.analyze(phase + "_WRONG_TARGET")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("independent endpoint", result.stdout + result.stderr)

    def test_partial_histogram_is_not_evidence(self):
        result = self.analyze("AB_MISSING_CHUNK")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("audit incomplete", result.stdout + result.stderr)

    def test_percentile_clipping_and_black_exclusion(self):
        result = self.analyze("CLIPPED_EXTREMES")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_camera_motion_cannot_be_mistaken_for_exposure_drift(self):
        for phase in ("AB", "BA"):
            with self.subTest(phase=phase):
                result = self.analyze(phase + "_CAMERA_DRIFT")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("camera drift", result.stdout + result.stderr)

    def test_missing_camera_is_not_evidence(self):
        result = self.analyze("AB_MISSING_CAMERA")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("camera receipt", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
