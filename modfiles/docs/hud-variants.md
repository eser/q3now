# HUD variants

The in-game HUD ships as two layouts. Switch at runtime:

```
hud classic     # bottom-right ammo block with gauge + icon
hud modern      # mockup-aligned, bare ammo text
```

The `hud` cvar is archived and reloads the layout as soon as it changes
(`cl_wired_ui.c:4126`), so the two can be compared back to back without a
restart.

---

## Why there are two

The V2 artboard puts the ammo readout at **bottom centre**. The running game puts
the **weapon-select bar** there. On the artboard that is fine, because the
artboard does not draw a weapon bar; in the game the two overlap and the ammo
count reads as part of the carousel.

Chasing artboard parity also cost two leaves. The 2026-08-10 layout had four:

| leaf | what it is |
|---|---|
| `statusbar_value` | the number |
| `statusbar_bar` | gauge, `direction L`, fills right-to-left |
| `statusbar_icon` | ammo-type icon |
| caption | weapon shortname |

The mockup draws neither a gauge nor an icon, so the realignment dropped both.
That was a real loss, not a simplification — the gauge is how you read remaining
ammo at a glance without parsing digits.

Rather than pick a winner by fiat, both layouts ship.

---

## `classic`

The 2026-08-10 layout, kept verbatim.

- Ammo block sits **bottom-right**, mirroring the health/armor pair on the left
- Label, value, gauge and icon are all **right-aligned** (`textalign 2`,
  `direction L`), growing leftward from the screen edge
- Does not collide with the weapon-select bar

Widget: `ui/widgets/qw_hud_ammo_readout_classic.wui`

## `modern`

The mockup-aligned layout.

- Ammo block also bottom-right after the 2026-08-16 review, but **bare text** —
  no gauge, no icon, no panel chrome, matching what the artboard draws
- Scores against the V2 artboard in the visual gate

Widget: `ui/widgets/qw_hud_ammo_readout.wui`

---

## The visual gate

`tests/visual/regions/v2_hud_active.json` anchors `ammo_readout` to the mockup's
centre slot (x640 y716 w160 h56). Neither variant sits there any more, so that
region scores badly for both. This is expected: the gate measures agreement with
an artboard whose own layout collides with a widget it does not draw.

Every other region is unaffected — the last full run was 12/15 SSIM PASS and
15/15 deltaE2000 PASS across all five theme configs.
