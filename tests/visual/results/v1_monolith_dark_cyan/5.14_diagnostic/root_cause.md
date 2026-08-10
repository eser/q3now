# Dispatch 5.14 — V1_Monolith cascade root-cause diagnostic

## Method
- 49 LoC `#ifdef _DEBUG`-gated instrumentation added across parser, layout (WUI_LayoutItem + WUI_LayoutFlex flex-child loop), and Clay-emit (cl_wired_parse.c +16, cl_wired_layout.c +22, cl_wired_clay.c +11).
- DEBUG build emitted ~312k WUI_TRACE log entries to `qconsole.jsonl` (~313k lines total).
- Filtered to V1_Monolith items in `parse_trace.log` (82 entries) + one-frame `frame_trace.log` (285 entries).

## ROOT CAUSE (primary): `width <Nvw>` syntax invalid → smart-skip cascade

### Evidence
PARSE log shows the engine produces this exact sequence at main.wui:left_region:

```
PARSE item='top_bar' flex=1 …                                        (last successful parent commit)
WARN: WiredUI: unknown width mode '41.7' (FIT/GROW/PERCENT/FIXED) — 'ui/main.wui'
DEBUG: unknown item keyword 'vw'
DEBUG: unknown item keyword '$spacing_none'
DEBUG: unknown item keyword '0'
DEBUG: unknown item keyword '{'
DEBUG: unknown item keyword 'hero_quake'
PARSE item='left_region' flex=1 pos=1 grow=0.00 rect=(0,0,0,0) offT=(1,10.67vh) offL=(1,5vw) …
PARSE item='hero_wired' flex=0 …                              (now main_root's direct child, not left_region's)
```

### Mechanism
`cl_wired_parse.c:1854-1908` `width <MODE> [v]` parser:
1. Reads `width` keyword.
2. ReadTokenEval reads next token. **Botlib's lexer splits `41.7vw` into two tokens: `41.7` (TT_NUMBER) + `vw` (identifier-like).**
3. Matches mode against FIT/GROW/PERCENT/FIXED. `41.7` matches none.
4. SEV_WARN logged, parser returns without consuming the value tokens.
5. `vw` remains in the stream. Outer loop reads it as the next "keyword".
6. `vw` doesn't match → smart-skip recovery (`cl_wired_parse.c:2412-2421`) reads one more token to consume as "value".
7. The cascade eats subsequent unknown tokens until reaching `{` of the nested `itemDef`, which is a special case for the smart-skip: it calls `WiredPC_SkipBracedBlock` and the ENTIRE first nested itemDef body (hero_quake) is silently discarded.
8. The remaining nested itemDefs (`hero_wired`, `action_menu`, `left_spacer`, `left_footer_row`) get parsed but the parser's nesting context is now de-synchronized — they end up attached to MAIN_ROOT (left_region's parent), not left_region.
9. `left_region.childCount=0` after this cascade. The flex layout pass therefore does NOT recurse into left_region (`cl_wired_layout.c:485-487` gate `isFlexContainer && childCount > 0`).

### Frame trace confirms
```
LAYOUTITEM item='main_root' parent=(0,0,1440,900) resolved=(0,0,1440,900) flex=1 childCount=6
FLEXCHILD parent='main_root' child='top_bar'         assigned=(0,0,1440,40.5)    recurse=1
FLEXCHILD parent='main_root' child='left_region'     assigned=(0,40.5,0,0)       recurse=0    ← w=0, h=0, no children
FLEXCHILD parent='main_root' child='hero_wired'      assigned=(0,40.5,1440,126)  recurse=0    ← orphaned to main_root
FLEXCHILD parent='main_root' child='action_menu'     assigned=(0,166.5,1440,288) recurse=1    ← orphaned to main_root
FLEXCHILD parent='main_root' child='left_spacer'     assigned=(0,454.5,0,427.5)  recurse=0    ← orphaned
FLEXCHILD parent='main_root' child='left_footer_row' assigned=(0,882,1440,18)    recurse=1    ← orphaned
LAYOUTITEM item='right_region' parent=(0,0,1440,900) resolved=(1023.8,72,360,756) flex=1 childCount=3
```

Note: `right_region` triggers the SAME bug (`width 25vw`) — its first nested child `card_profile` (the outer container of qw_profile_card.wui) is eaten. Cards `card_last_match`, `card_global_feed`, `card_arena` (other outer containers) are likewise missing entirely from PARSE log; only their inner items survive scattered as top-level menu items.

## ROOT CAUSE (secondary): `borderBottom`/`borderTop` not recognized

### Evidence
- `code/client/wired/ui/cl_wired_parse.c` has only `border` keyword handler at `:1968`, NO `borderBottom`/`borderTop`/`borderLeft`/`borderRight`.
- `qw_menu_item.wui:32` macro expands to `borderBottom 1px $line` per row.
- The keyword `borderBottom` triggers smart-skip cascade INSIDE each menu_item.
- Smart-skip cascade eats subsequent tokens; menu_campaign's `direction row` likely gets eaten too, leaving menu_campaign with `direction = WUI_LAYOUT_NONE` (default 0).

### Frame trace confirms
```
EMIT item='menu_campaign' xywh=(0,166.5,1440,44) dir=0 …   ← dir=0 (LEFT_TO_RIGHT, but layout-pass treats NONE)
FLEXCHILD parent='menu_campaign' child='<anon>' assigned=(1598.4,207.0,0.0,36.0) recurse=0
FLEXCHILD parent='menu_campaign' child='<anon>' assigned=(1598.4,207.0,0.0,36.0) recurse=0
FLEXCHILD parent='menu_campaign' child='<anon>' assigned=(1598.4,225.0,0.0,0.0)  recurse=0
FLEXCHILD parent='menu_campaign' child='<anon>' assigned=(1598.4,216.0,0.0,18.0) recurse=0
```

All 4 chevron/label/spacer/sub children render at the SAME x=1598.4 (off-screen — viewport is 1440). x cursor is NOT being applied because flex direction !=  WUI_LAYOUT_ROW in the layout pass. (The exact path producing x=1598.4 needs additional instrumentation in WUI_LayoutFlex line 330-340 if dispatch 5.15 wants finer detail; the high-level cause is direction-row-not-set.)

## Per-Delta root cause summary

### Delta A — cards inner content missing
**Stage**: parser (cl_wired_parse.c:1854-1908 width parser).
**Mechanism**: `right_region`'s `width 25vw` triggers unknown-mode warning → smart-skip cascade eats `card_profile` outer container's body. Inner items (card_profile_header_row, card_profile_row) become right_region's direct children (only 3 children instead of 4 cards). card_last_match, card_global_feed, card_arena outer containers ALL eaten — their inner items become top-level menu items (parent=viewport coords) and render at off-screen x=1598.4 due to Delta C cascade.

### Delta B — hero overlap
**Stage**: parser (cl_wired_parse.c:1854-1908 width parser).
**Mechanism**: `left_region`'s `width 41.7vw` triggers smart-skip cascade eating `hero_quake`. Surviving siblings (hero_wired, action_menu, left_spacer, left_footer_row) become MAIN_ROOT's direct children. hero_wired renders at (0, 40.5, 1440, 126) — full-width at top of viewport. The visible "QUAKE" text in the capture comes from smart-skip cascade leakage — `text "QUAKE"` keyword from hero_quake's body likely gets applied to left_region or main_root before SkipBracedBlock fires, yielding a stray text rendering at left_region/main_root position.

### Delta C — action_menu menu rows missing
**Stage**: parser (cl_wired_parse.c — missing `borderBottom`/`borderTop` keyword handlers).
**Mechanism**: `qw_menu_item.wui:32` macro `borderBottom 1px $line` triggers smart-skip cascade INSIDE each menu_item declaration. This eats the `direction row` keyword (or its `row` value), leaving menu_campaign.direction = WUI_LAYOUT_NONE. WUI_LayoutFlex then treats menu_campaign as non-ROW flow → child x positions all set to same off-screen value (1598.4), no x cursor increment.

## Trace tables

### Trace: left_region pipeline

| Stage | Value |
|---|---|
| PARSE | item='left_region' flex=1 pos=1 rect=(0,0,0,0) offT/L set, offR/B empty |
| LAYOUTITEM (not entered; not in trace) | — recursion skipped due to childCount=0 |
| FLEXCHILD (as main_root's child) | assigned=(0,40.5,0,0) — flex pass gives 0x0 because items[i].h=0 (no rect h on left_region) |
| EMIT | (not in extracted frame; would show w=0 h=0) |

### Trace: hero_quake pipeline

| Stage | Value |
|---|---|
| PARSE | **NOT PRESENT** — itemDef body eaten by smart-skip |
| LAYOUTITEM | — does not exist |
| FLEXCHILD | — does not exist |
| EMIT | — does not exist |

### Trace: hero_wired pipeline

| Stage | Value |
|---|---|
| PARSE | item='hero_wired' flex=0 rect=(0,0,1,14VH) — correctly parsed |
| FLEXCHILD (as main_root's direct child) | assigned=(0,40.5,1440,126) recurse=0 — orphaned position |
| EMIT | — (not extracted; would show sizing.h=FIXED(126), sizing.w=PERCENT(1)) |

### Trace: menu_campaign pipeline

| Stage | Value |
|---|---|
| PARSE | item='menu_campaign' flex=1 — but direction likely lost due to borderBottom cascade |
| FLEXCHILD (as action_menu's child) | assigned=(0,166.5,1440,44) recurse=1 |
| LAYOUTITEM | item='menu_campaign' resolved=(0,166.5,1440,44) flex=1 childCount=4 |
| FLEXCHILD children (anon ×4) | ALL at x=1598.4 (off-screen) — flex direction NOT ROW |
| EMIT | xywh=(0,166.5,1440,44) sizing.h=FIXED(44) dir=0 gap=14 |

## Dispatch 5.15 fix proposals + LoC estimates

### Fix 1 — accept `width <value>` shorthand (no MODE keyword required)
**Target**: cl_wired_parse.c:1854-1908 width/height parser branch.
**Change**: after reading the first token, if it's NOT a MODE keyword (FIT/GROW/PERCENT/FIXED), treat it as a value token (UnreadToken + ParseValue). This makes `width 41.7vw` equivalent to `width FIXED 41.7vw` / `width PERCENT 41.7vw`.
**Estimated LoC**: +12 in cl_wired_parse.c. Engine-only fix. No widget edits needed.

### Fix 2 — register `borderBottom`/`borderTop`/`borderLeft`/`borderRight` keyword handlers
**Target**: cl_wired_parse.c (around the existing `border` handler at :1968).
**Change**: add 4 keyword handlers that parse `border<Side> <width> <color>` and set the per-side `bordersize4[]` array (already in wiredItemDef_t per dispatch 5.5 wiredItemDef_t fields).
**Estimated LoC**: +24 in cl_wired_parse.c (one branch per side; share the existing border-value/color decode code).

### Fix 3 — investigate menu_campaign direction-row loss (verification + targeted fix)
**Target**: confirm Fix 2 actually restores `direction row` parsing for menu_campaign (it should, by removing the smart-skip cascade). If not, add additional WUI_LayoutFlex direction-NONE handling.
**Estimated LoC**: 0 expected (fixed transitively by Fix 2); +10 if WUI_LayoutFlex needs an explicit direction-NONE → direction-ROW default for backwards compat.

### Total dispatch 5.15 engine LoC estimate
**~36 LoC engine** (Fix 1 + Fix 2), 0 widget edits.
**Expected SSIM impact**: substantial. Restoring proper nesting fixes left_region/right_region/cards/menu_items structure. Best-case SSIM crosses 0.70 target with all 3 Deltas resolved.

## Files
- Instrumentation: `code/client/wired/ui/cl_wired_parse.c` +16 LoC, `code/client/wired/ui/cl_wired_layout.c` +22 LoC (+ LOG_DECLARE_CHANNEL), `code/client/wired/ui/cl_wired_clay.c` +11 LoC. Total: **49 LoC**, all `#ifdef _DEBUG` gated. Production build untouched at runtime.
- Captured artifacts:
  - `tests/visual/results/v1_monolith_dark_cyan/5.14_diagnostic/impl.png` (visual reference)
  - `tests/visual/results/v1_monolith_dark_cyan/5.14_diagnostic/parse_trace.log` (82 PARSE entries, V1_Monolith items)
  - `tests/visual/results/v1_monolith_dark_cyan/5.14_diagnostic/frame_trace.jsonl` (one frame, 285 raw JSON entries)
  - `tests/visual/results/v1_monolith_dark_cyan/5.14_diagnostic/frame_trace.log` (one frame, decoded for readability)
