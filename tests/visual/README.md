# Visual test harness

Two coexisting gates per the W-17 strict regime:

- **`vdiff`** (`tools/visual-diff/`) — per-pixel diff with per-region thresholds. Audit-trail tool; `vdiff` exit code is no longer the active V1_Monolith gate after dispatch 5b.
- **`vcompare`** (`tools/visual-compare/`) — perceptual (SSIM + ΔE_00) + structural (Zhang-Shasha tree edit distance) AND-combined gate. **Active gate** for V1_Monolith.

Exit codes (both tools): 0 = PASS, 1 = FAIL, 2 = HALT (ceiling violation / threshold-up without flag / resolution mismatch / missing structural input).

## Layout

- `config.json` — ceiling policy (vdiff: global 10% / region 20%; vcompare: SSIM ≥0.70 / ΔE_00 ≤25 / structural ≤0.40). Starting thresholds same file.
- `baselines/<artboard>_<mode>_<accent>.png` — Chrome headless render of the React mockup
- `baselines/<artboard>_<mode>_<accent>_dom.json` — DOM tree (Clay-mirror schema) for structural diff
- `regions/<artboard>.json` (+ optional `_<mode>` override) — per-region thresholds + optional vcompare per-metric overrides
- `results/<artboard>_<mode>_<accent>/<timestamp>/` — impl.png, impl_clay.json, result.json (+ diff.png for vdiff runs)
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

## Adding an artboard

1. Add `scripts/artboard_<name>.html` mounting the component + DOM walker.
2. Add `regions/<name>.json` with thresholds at the starting values from `config.json`.
3. Run `make visual-baseline ARTBOARD=<name> MODE=<mode> ACCENT=<accent>` per palette config (writes both `.png` and `_dom.json`).
4. Run `make visual-compare ARTBOARD=<name> ...` to gate.
