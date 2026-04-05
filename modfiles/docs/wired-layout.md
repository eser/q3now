# Wired UI Layout System Reference

The Wired UI layout system provides resolution-independent coordinates,
flexbox-based layout, aspect ratio constraints, transitions, responsive
breakpoints, and developer tools. All layout files use the `.wmenu` extension
and are parsed by the Wired UI parser (`cl_wired_parse.c`).

---

## 1. Coordinate Units

Every numeric value in the layout system can carry a unit suffix.
When no suffix is present, the value is treated as **normalized** (fraction
of the parent container).

| Unit | Suffix | Meaning | Range | Example |
|------|--------|---------|-------|---------|
| Normalized | *(none)* | Fraction of parent dimension | 0.0 -- 1.0 | `rect 0.1 0.1 0.8 0.8` |
| Viewport width | `vw` | Percentage of viewport width | 0 -- 100 | `width 50 vw` |
| Viewport height | `vh` | Percentage of viewport height | 0 -- 100 | `height 50 vh` |
| Pixels | `px` | Real device pixels | any | `gap 2 px` |

The parser tokenizes the number and suffix separately, so a space between
them is allowed: both `50vw` and `50 vw` work.

Units can be used anywhere a value is expected: `rect`, `gap`, `padding`,
`basis`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`, and inside
`breakpoint` blocks.

```
// Normalized (default) -- 10% from top-left, 80% of parent
rect 0.1 0.1 0.8 0.8

// Mixed units
rect 10 vw 5 vh 80 vw 90 vh

// Pixel gap
gap 4 px
```

---

## 2. Flexbox Layout

An `itemDef` becomes a flex container when it declares a `layout` direction.
Its direct child `itemDef` blocks are laid out along the main axis.

### Container Properties

| Keyword | Values | Default | Description |
|---------|--------|---------|-------------|
| `layout` | `row`, `column`, `row wrap`, `column wrap` | none | Main axis direction. Adding `wrap` enables wrapping. |
| `gap` | `<value>` | 0 | Space between children on the main axis. |
| `padding` | 1, 2, or 4 values | 0 | Internal padding. 1 value = all sides. 2 values = top/bottom, left/right. 4 values = top, right, bottom, left. |
| `align` | `start`, `center`, `end`, `stretch` | `start` | Cross-axis alignment of children. |
| `justify` | `start`, `center`, `end`, `space-between` | `start` | Main-axis distribution of children. |

```
itemDef {
    layout row
    gap 0.02
    padding 0.01
    align center
    justify space-between
    // children go here
}
```

### Child Properties

| Keyword | Values | Default | Description |
|---------|--------|---------|-------------|
| `grow` | `<number>` | 0 | Flex-grow factor. Controls how much extra space this child absorbs. |
| `shrink` | `<number>` | 1 | Flex-shrink factor. Controls how much this child shrinks when space is tight. |
| `basis` | `<value>` | 0 | Initial size on the main axis before grow/shrink is applied. |
| `alignSelf` | `start`, `center`, `end`, `stretch` | parent `align` | Per-child override of the container's cross-axis alignment. |
| `minWidth` | `<value>` | 0 | Minimum width constraint. |
| `maxWidth` | `<value>` | 0 (none) | Maximum width constraint. |
| `minHeight` | `<value>` | 0 | Minimum height constraint. |
| `maxHeight` | `<value>` | 0 (none) | Maximum height constraint. |

```
itemDef {
    layout row
    gap 0.01

    itemDef {
        grow 1
        minWidth 10 vw
        // takes remaining space
    }
    itemDef {
        basis 0.3
        shrink 0
        // fixed 30% width, never shrinks
    }
}
```

---

## 3. Aspect Ratio

The `aspect` keyword constrains an item to a fixed width-to-height ratio.
The item is scaled to **contain** within its declared rect.

| Keyword | Values | Description |
|---------|--------|-------------|
| `aspect` | `<number>` or `<W/H>` | Width divided by height. `1` = square, `16/9` = widescreen. |

```
// Square item
itemDef {
    rect 0.1 0.1 0.4 0.4
    aspect 1
}

// 16:9 video frame
itemDef {
    rect 0.05 0.05 0.9 0.9
    aspect 16/9
}
```

If `aspect` is set, the layout engine computes the largest rectangle that
fits inside the declared rect while preserving the ratio.

---

## 4. Nested Containers

An `itemDef` with `layout` can contain child `itemDef` blocks directly.
Children inherit default visibility and flex-shrink of 1. Nesting depth
is limited only by the parser's item allocation pool.

```
// Two-column layout
itemDef {
    layout row
    gap 0.02
    rect 0 0 1 1

    // sidebar
    itemDef {
        basis 0.25
        shrink 0
        layout column
        gap 0.01

        itemDef { grow 1 }
        itemDef { grow 1 }
    }

    // main content
    itemDef {
        grow 1
        layout column
        gap 0.01

        itemDef { grow 2 }
        itemDef { grow 1 }
    }
}
```

Each child `itemDef` can itself be a flex container, creating arbitrarily
deep layout trees.

---

## 5. Transitions

The `transition` keyword enables smooth animation when an item's rect
changes (e.g., from a breakpoint switch or runtime update).

```
transition <duration_ms> [easing]
```

| Parameter | Values | Default |
|-----------|--------|---------|
| duration | milliseconds (integer) | required |
| easing | `linear`, `ease-in`, `ease-out`, `ease-in-out` | `linear` |

```
itemDef {
    rect 0.1 0.1 0.3 0.3
    transition 300 ease-out
}
```

When the item's target rect changes, the layout engine interpolates from
the old rect to the new rect over the specified duration using the easing
curve.

---

## 6. Responsive Breakpoints

Breakpoints override an item's rect based on the current viewport width.
Up to 8 breakpoints per item. When multiple breakpoints match, the last
matching one wins.

```
breakpoint <minWidth> <maxWidth> {
    rect <x> <y> <w> <h>
}
```

Set `minWidth` or `maxWidth` to `0` to disable that bound.

```
itemDef {
    rect 0.05 0.05 0.9 0.9

    // narrow screens (up to 1280px wide)
    breakpoint 0 1280 {
        rect 0 0 1 1
    }

    // wide screens (1920px and above)
    breakpoint 1920 0 {
        rect 0.1 0.1 0.8 0.8
    }
}
```

Breakpoint rects support all unit types (`vw`, `vh`, `px`, normalized).

---

## 7. Developer Tools

Two cvars control development aids. Both default to `0` (off).

| Cvar | Effect |
|------|--------|
| `wired_hotreload 1` | Watches `.wmenu` / `.whud` files for changes and reloads them automatically. No restart required. |
| `wired_debug_layout 1` | Draws colored outlines around every flex container and child, showing padding and gap regions. |

Enable them from the console:

```
/wired_hotreload 1
/wired_debug_layout 1
```

Combine both for a live-editing workflow: edit a `.wmenu` file in your
text editor, save, and see the updated layout with debug outlines
immediately in the running game.

---

## 8. Examples

### 8.1 FFA Scoreboard Overlay

A vertically stacked scoreboard displayed when TAB is held. Uses
normalized coordinates throughout.

```
#include "ui/wmenumacros.h"

menuDef {
    name "ingame_scoreboard_ffa"
    rect 0.109 0.071 0.781 0
    fullScreen 0
    visible 1
    style 1
    backcolor 0.05 0.05 0.1 0.7

    // gametype title
    itemDef {
        name "title"
        type 0
        rect 0 0.013 0.781 0.038
        cvar "wired_sb_gametype"
        font "sansman" 19
        textalign 1
        forecolor 1.0 1.0 1.0 1.0
        visible 1
        decoration
    }

    // separator
    itemDef {
        rect 0.006 0.096 0.769 0.002
        style 1
        backcolor 0.3 0.3 0.4 0.6
        visible 1
        decoration
    }

    // score list (custom widget type 20)
    itemDef {
        name "scorelist"
        type 20
        rect 0 0.104 0.781 0.708
        feeder 11
        forecolor 1.0 1.0 1.0 1.0
        visible 1
        decoration
    }
}
```

### 8.2 Settings Menu with Sidebar (Flexbox)

A two-panel settings layout using flex containers. The sidebar has a fixed
width; the content area grows to fill remaining space.

```
#include "ui/wmenumacros.h"

menuDef {
    name "settings_flex"
    fullScreen 1
    rect 0 0 1 1
    visible 1
    style 1
    backcolor 0.06 0.06 0.1 1

    WFULLSCREEN_BACKGROUND

    onESC { close }

    // root flex container
    itemDef {
        layout row
        rect 0.05 0.05 0.9 0.9
        gap 0.02
        padding 0.02

        // sidebar (fixed 25% width)
        itemDef {
            basis 0.25
            shrink 0
            layout column
            gap 0.01

            itemDef {
                type 1
                text "Video"
                rect 0 0 0.25 0.05
                font "oxanium" 14
                forecolor WCOLOR_ACCENT
                textalign 0
                visible 1
                action { exec "open video" }
            }
            itemDef {
                type 1
                text "Audio"
                rect 0 0 0.25 0.05
                font "oxanium" 14
                forecolor WCOLOR_ACCENT
                textalign 0
                visible 1
                action { exec "open sound" }
            }
            itemDef {
                type 1
                text "Controls"
                rect 0 0 0.25 0.05
                font "oxanium" 14
                forecolor WCOLOR_ACCENT
                textalign 0
                visible 1
                action { exec "open controls" }
            }
        }

        // content area (grows to fill)
        itemDef {
            grow 1
            layout column
            gap 0.01
            align stretch

            itemDef {
                type 0
                text "Select a category"
                rect 0 0 1 0.05
                font "sansman" 18
                textalign 1
                forecolor 1 1 1 0.5
                visible 1
                decoration
            }
        }
    }
}
```

### 8.3 Responsive Panel with Transitions

A panel that adapts its size to narrow and wide viewports, animating
smoothly between breakpoints.

```
#include "ui/wmenumacros.h"

menuDef {
    name "responsive_demo"
    fullScreen 1
    rect 0 0 1 1
    visible 1

    WFULLSCREEN_BACKGROUND

    onESC { close }

    // content panel -- adapts to screen width
    itemDef {
        rect 0.15 0.1 0.7 0.8
        style 1
        backcolor 0.08 0.08 0.12 0.92
        visible 1
        transition 250 ease-in-out

        // narrow: expand to full width
        breakpoint 0 1280 {
            rect 0.02 0.05 0.96 0.9
        }

        // ultra-wide: shrink and center
        breakpoint 2560 0 {
            rect 0.25 0.1 0.5 0.8
        }

        // inner heading
        itemDef {
            type 0
            text "Responsive Panel"
            rect 0 0.02 1 0.05
            font "sansman" 22
            textalign 1
            forecolor 1 1 1 1
            visible 1
            decoration
        }
    }
}
```

---

## Quick Reference

```
// Units
0.5          normalized (fraction of parent)
50 vw        50% of viewport width
50 vh        50% of viewport height
16 px        16 real pixels

// Flex container
layout row | column | row wrap | column wrap
gap <value>
padding <all> | <vert> <horiz> | <top> <right> <bottom> <left>
align start | center | end | stretch
justify start | center | end | space-between

// Flex child
grow <number>
shrink <number>
basis <value>
alignSelf start | center | end | stretch
minWidth <value>    maxWidth <value>
minHeight <value>   maxHeight <value>

// Aspect ratio
aspect <ratio>      // 1, 16/9, 4/3, etc.

// Transition
transition <ms> [linear | ease-in | ease-out | ease-in-out]

// Breakpoint
breakpoint <minWidth> <maxWidth> { rect <x> <y> <w> <h> }

// Dev tools
/wired_hotreload 1
/wired_debug_layout 1
```
