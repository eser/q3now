# Visual test harness

Two coexisting gates per the W-17 strict regime:

- **`vdiff`** (`tools/visual-diff/`) — per-pixel diff with per-region thresholds. Audit-trail tool; `vdiff` exit code is no longer the active V1_Monolith gate after dispatch 5b.
- **`vcompare`** (`tools/visual-compare/`) — perceptual (SSIM + ΔE_00) + structural (Zhang-Shasha tree edit distance) AND-combined gate. **Active gate** for V1_Monolith.

Exit codes (both tools): 0 = PASS, 1 = FAIL, 2 = HALT (ceiling violation / threshold-up without flag / resolution mismatch / missing structural input / empty gating region).

## The ink check: a gating region must contain engine ink

A region anchored where the engine draws nothing cannot fail an image
comparison — it scores background against background, and passes. That is a
false green, and it is worse than a red, because it reads as coverage while
verifying nothing. Four of the nine V2_HUD_Active gating regions were in
exactly that state (`kill_feed` SSIM 0.3653, `holdables_strip` 0.5086,
`weapon_carousel` 0.3245, `scoreboard_overlay` 0.6058 — all at **0.000% ink**),
because the regions file was authored against the `modern` HUD variant that
commit 2a397bf3 deleted.

So in-game captures now take **two** screenshots in one engine session at one
pinned viewpoint: `impl.png` with the HUD, and `impl_nohud.png` with `hud none`.
Differencing them isolates exactly the pixels the HUD drew. `vcompare
--impl-nohud` measures that per region and **errors** (exit 2) on any gating
region below `inkFloorPct` (0.5%). Measured separation on V2_HUD_Active is two
orders of magnitude: real regions 9.1–90.8%, empty regions 0.000%.

A region that is legitimately empty has three honest options — never a
threshold tweak:

| situation | what to do |
|---|---|
| region points at the wrong place | re-anchor it to where the engine draws |
| widget not shipped (`#include` commented out) | `gating: false` with the reason |
| widget correct, content state-conditional | `allow_empty` + `empty_reason` (required together) |

The pair is also checked as a whole: a valid HUD-on/HUD-off pair differs by only
the overlay (measured 3.24–3.26% of the frame across all five palette configs),
so a pair differing by more than `maxFrameInkPct` (25%) is rejected outright.
This is not hypothetical — a run whose engine hung before the map loaded paired
a **menu** screenshot with a HUD frame, every gating region measured 60–100%
"ink", and the config was recorded PASS on a menu-versus-arena comparison
(frame difference: 94.83%). Proving each region has ink is not sufficient on its
own; the two frames must also be the same scene.

Known limit: ink proves *something* drew in the rect, not that the *intended*
widget did. `weapon_placeholder` measures 4.24% ink while not being shipped at
all — every one of those pixels is the ammo panel overlapping its corner.
Overlapping rects still need a human to confirm which widget they measure.

## When the perceptual metrics do not gate

`pixel_metrics_gated: false` records SSIM/ΔE/structural as audit-trail numbers
while the verdict rests on the ink check. It requires
`pixel_metrics_ungated_reason`, and it requires `--impl-nohud` (with the metrics
ungated and no ink check there would be nothing left gating at all).

V2_HUD_Active sets it, on measurement rather than convenience. With every
region correctly anchored and ink-verified, SSIM measured 0.168–0.279 for the
four content-bearing panels — while three control regions containing **no HUD
on either side** measured 0.405, 0.621 and 0.634. Absence outscores presence:
the baseline is a flat vector artboard over a smooth gradient and the impl is a
live 3D arena, so SSIM collapses on the background disagreement regardless of
HUD fidelity. No threshold separates "the HUD regressed" from "the arena is
textured". The structural fix is an **engine-blessed baseline** (a previous
engine capture) rather than the mockup; until that decision is taken, the
mockup stays the design reference and these numbers stay recorded, not gating.

## Layout

- `config.json` — ceiling policy (vdiff: global 10% / region 20%; vcompare: SSIM ≥0.70 / ΔE_00 ≤25 / structural ≤0.40). Starting thresholds same file.
- `baselines/<artboard>_<mode>_<accent>.png` — Chrome headless render of the React mockup
- `baselines/<artboard>_<mode>_<accent>_dom.json` — DOM tree (Clay-mirror schema) for structural diff
- `regions/<artboard>.json` (+ optional `_<mode>` override) — per-region thresholds + optional vcompare per-metric overrides
- `results/<artboard>_<mode>_<accent>/<timestamp>/` — impl.png, impl_nohud.png (in-game artboards), impl_clay.json, result.json (+ diff.png for vdiff runs)
- `scripts/artboard_<artboard>.html` — isolated React harness per artboard (includes DOM walker)
- `scripts/regen_baseline.sh` — chrome headless render → baselines/ (PNG + DOM JSON)
- `scripts/capture_impl.sh` — engine boot + `screenshot` + `wui_test_dump_clay`
- `scripts/compare.sh` — vdiff (audit-trail / per-pixel)
- `scripts/vcompare_run.sh` — vcompare (active gate / perceptual + structural)

## Make targets

- `make visual-baseline ARTBOARD=v1_monolith MODE=dark ACCENT=amber` — regen baseline (PNG + DOM)
- `make visual-test ARTBOARD=v1_monolith MODE=dark ACCENT=amber` — vdiff gate
- `make visual-test-all` — vdiff fan-out across `VISUAL_CFGS`
- `make visual-compare ARTBOARD=v1_monolith MODE=dark ACCENT=amber` — vcompare gate (perceptual + structural)
- `make visual-compare-all` — vcompare fan-out
- `make visual-tools-test` — Go unit tests for vdiff/vcompare themselves (no engine, no display, ~1s)

## Prior-run selection

Both tools reject a threshold that is looser than the most recent prior run for
the same artboard/config. "Most recent prior run" means a `result.json` **this
tool itself wrote**, identified by a `"tool"` field, selected by file mtime.

Both rules exist because both were violated. The tools share one results tree
with disjoint schemas, and selection was by directory *name*-sort: an ad-hoc
probe directory (`v2b`) whose `result.json` came from vcompare sorted last, so
vdiff adopted it, unmarshalled every field to zero, and rejected the run's real
thresholds as a loosening — exit 2 before comparing a single pixel. The
symmetric case is quieter and worse: vcompare reading a vdiff result gets zero
thresholds, which its `oldVal == 0` short-circuit skips, leaving the loosen
guard silently disarmed. A `result.json` without the marker is not adopted
either — an unidentifiable file cannot be proven to be ours.

## Threshold policy (dispatch 5a re-pin + 5b extension)

Ceilings declared in `config.json`, enforced by both tools:

| Tool   | Scope  | Ceiling                | Starting             |
|--------|--------|------------------------|----------------------|
| vdiff  | global | 10% pct                | 5% pct               |
| vdiff  | region | 20% pct                | 10% pct              |
| vcompare | global SSIM      | ≥ 0.70 (floor)  | 0.90 |
| vcompare | region SSIM      | ≥ 0.70          | 0.85 |
| vcompare | global ΔE_00     | ≤ 25.0          | 8.0  |
| vcompare | region ΔE_00     | ≤ 25.0          | 12.0 |
| vcompare | global structural | ≤ 0.40         | 0.10 |
| vcompare | region structural | ≤ 0.40         | 0.20 |

- Region/global thresholds **above the ceiling** (or below for SSIM) → exit 2 with `CEILING VIOLATION ... HALT-fork per W-7.22`.
- Threshold **DOWN** (tightening) is silent-allowed.
- Threshold **UP** (loosening: SSIM down, ΔE up, structural up, vdiff pct up) versus the most recent prior `result.json` for the same artboard/mode/accent is rejected unless `--allow-threshold-up` is passed. Make targets never pass that flag; threshold-up must be a hand-typed CLI invocation.

## Palette accent vocab

Canonical accent tokens, verbatim from `code/client/wired/ui/cl_wired_palette.c:33`:
```
amber, blood, toxic, cyan, violet
```

Mapping note (legacy dispatch text → engine token): `blue` → `cyan`; `green` → `toxic`. `amber/cyan/toxic/violet` are already canonical. `blood` is registered but not in the V1_Monolith default `VISUAL_CFGS` matrix.

## Regions JSON schema

vdiff + vcompare share a single regions JSON file. vdiff reads `global_threshold_pct` + per-region `threshold_pct` + `fuzz_pct`. vcompare reads optional `ssim_threshold` / `deltaE2000_threshold` / `structural_threshold` per region (absent overrides fall through to `config.json` defaults).

```json
{
  "global_threshold_pct": 5.0,
  "fuzz_pct": 12.0,
  "ssim_threshold_global": 0.90,
  "deltaE2000_threshold_global": 8.0,
  "structural_threshold_global": 0.10,
  "regions": [
    { "name": "menu_row_0", "x": 80, "y": 380, "w": 460, "h": 60,
      "threshold_pct": 10.0,
      "ssim_threshold": 0.80, "deltaE2000_threshold": 14.0 }
  ]
}
```

## DOM ↔ Clay node mapping

DOM extraction (`scripts/artboard_*.html` walker) and Clay dump (`wui_test_dump_clay` in `cl_wired_clay.c`) emit the same schema so Zhang-Shasha edit distance node-matches without translation overhead beyond the type classifier:

| DOM (Chrome harness)        | Clay (engine)                            | `type`        |
|-----------------------------|------------------------------------------|---------------|
| `<div>` with children       | `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START` | `container`   |
| element with no children + non-transparent `background-color` | `CLAY_RENDER_COMMAND_TYPE_RECTANGLE` | `rect` |
| element with `border-width > 0`  | `CLAY_RENDER_COMMAND_TYPE_BORDER`   | `border`      |
| text node                   | `CLAY_RENDER_COMMAND_TYPE_TEXT`          | `text`        |
| `<img>` or `<svg>`          | `CLAY_RENDER_COMMAND_TYPE_IMAGE`         | `image`       |

Node-match criterion (per Zhang-Shasha relabel cost = 0): exact `type` match + rect overlap > 50% + `style.color` ΔE_00 < 10 + text content equal (for text nodes).

## Harness prerequisites (`vendor/`)

The `scripts/artboard_*.html` harnesses render **fully offline**. They load React,
the fonts, and the design components from `tests/visual/vendor/`, which is
gitignored and must be built once before any baseline run:

```
node tests/visual/scripts/vendor_sync.mjs            # default design source
node tests/visual/scripts/vendor_sync.mjs --design-src <dir>
QW_DESIGN_SRC=<dir> node tests/visual/scripts/vendor_sync.mjs
```

That script (a) copies the React 18.3.1 UMD builds out of
`launcher/frontend/node_modules`, (b) copies Oxanium / JetBrains Mono / Share Tech
Mono TTFs out of `assets/fonts` + `tools/msdf/fonts`, and (c) compiles the design
JSX to plain `React.createElement` JS using the `@babel/core` already vendored
under `launcher/frontend/node_modules`.

The design JSX lives in a **separate private repo**, so it is deliberately not
committed here — `vendor/` is a reproducible local artifact, never a commit. If
`vendor/` is missing or stale the harness renders an empty `#root`, which shows up
as a PNG with a single distinct colour.

Harness resolution follows the pipeline (currently 1280x720) and is not hardcoded;
pass `?w=&h=` to capture at another size.

## Adding an artboard

1. Add `scripts/artboard_<name>.html` mounting the component + DOM walker.
2. Add `regions/<name>.json` with thresholds at the starting values from `config.json`.
3. Run `make visual-baseline ARTBOARD=<name> MODE=<mode> ACCENT=<accent>` per palette config (writes both `.png` and `_dom.json`).
4. Run `make visual-compare ARTBOARD=<name> ...` to gate.
