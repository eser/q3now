#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

import json
import pathlib
import sys

WIDTH, HEIGHT, CHANNELS = 1280, 720, 3


def load(path):
    data = pathlib.Path(path).read_bytes()
    expected = WIDTH * HEIGHT * CHANNELS
    if len(data) != expected:
        raise SystemExit(f"FAIL: {path} is {len(data)} bytes, expected {expected}")
    return data


def delta(a, b, gain=4):
    return bytes(min(abs(x - y) * gain, 255) for x, y in zip(a, b))


def write_ppm(path, data):
    pathlib.Path(path).write_bytes(f"P6\n{WIDTH} {HEIGHT}\n255\n".encode() + data)


def roi_values(data, rect):
    x0, y0, width, height = rect
    values = []
    for y in range(y0, y0 + height):
        start = (y * WIDTH + x0) * CHANNELS
        values.extend(data[start:start + width * CHANNELS])
    return values


if len(sys.argv) != 14:
    raise SystemExit("usage: analyze.py OUT emissive-a emissive-b direct-off direct-on indirect probe-off probe-on shadow-off shadow-on atmosphere-off atmosphere-on final")

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
emissive_a, emissive_b, direct_off, direct_on, indirect, probe_off, probe_on, shadow_off, shadow_on, atmosphere_off, atmosphere_on = map(load, sys.argv[2:-1])
final = load(sys.argv[-1])

emissive = delta(emissive_a, emissive_b, 6)
direct = delta(direct_off, direct_on, 4)
shadow = delta(shadow_off, shadow_on, 6)
atmosphere = delta(atmosphere_off, atmosphere_on, 4)
probe = delta(probe_off, probe_on, 4)
write_ppm(out / "emissive-only.ppm", emissive)
write_ppm(out / "direct-only.ppm", direct)
write_ppm(out / "indirect-only.ppm", indirect)
write_ppm(out / "probe-only.ppm", probe)
write_ppm(out / "shadow-contact.ppm", shadow)
write_ppm(out / "atmosphere-only.ppm", atmosphere)
write_ppm(out / "final.ppm", final)

lava = roi_values(final, (358, 381, 755, 331))
form = roi_values(final, (128, 58, 448, 446))
entity = roi_values(probe, (615, 320, 50, 90))
emissive_changed = sum(value > 18 for value in emissive)
direct_changed = sum(value > 18 for value in direct)
shadow_changed = sum(value > 18 for value in shadow)
atmosphere_changed = sum(value > 18 for value in atmosphere)
clipped_ratio = sum(value >= 254 for value in lava) / max(len(lava), 1)
form_mean = sum(form) / max(len(form), 1)
entity_mean = sum(entity) / max(len(entity), 1)
exposure_delta = abs(sum(emissive_a) - sum(emissive_b)) / len(emissive_a)

metrics = {
    "resolution": [WIDTH, HEIGHT],
    "emissiveChangedChannels": emissive_changed,
    "directChangedChannels": direct_changed,
    "shadowChangedChannels": shadow_changed,
    "atmosphereChangedChannels": atmosphere_changed,
    "lavaClippedChannelRatio": clipped_ratio,
    "shadowSideMean": form_mean,
    "probeEntityMean": entity_mean,
    "animatedExposureMeanDelta": exposure_delta,
}
(out / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n")

checks = [
    (emissive_changed >= 3000, "animated emissive contribution is vacuous"),
    (direct_changed >= 500, "dynamic-direct contribution is vacuous"),
    (shadow_changed >= 250, "cast-shadow contribution is vacuous"),
    (atmosphere_changed >= 1000, "atmosphere contribution is vacuous"),
    (clipped_ratio <= 0.01, "lava highlight clipping exceeds 1%"),
    (form_mean >= 3.0, "shadow-side form is unreadably black"),
    (entity_mean >= 2.0, "probe-lit moving entity is unreadable"),
    (exposure_delta <= 3.0, "animated fire destabilizes mean exposure"),
]
failed = [message for passed, message in checks if not passed]
print("lava-sanctum metrics:", json.dumps(metrics, sort_keys=True))
if failed:
    raise SystemExit("FAIL: " + "; ".join(failed))
print("PASS: lava-sanctum seven-term evidence and ROI gates")
