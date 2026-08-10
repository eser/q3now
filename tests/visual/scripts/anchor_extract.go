// Anchor extractor for dispatch 5c S2 — scans vertical columns in PNG to
// find the topmost row whose luminance crosses a threshold within a y range.
// Used to find the top-edge of large text glyphs (Q-glyph top, M-glyph top
// etc.) so per-anchor (dx,dy) can be measured against baseline/impl.
package main

import (
	"fmt"
	"image"
	_ "image/jpeg"
	_ "image/png"
	"os"
	"strconv"
)

func lum(c image.Image, x, y int) int {
	r, g, b, _ := c.At(x, y).RGBA()
	return int((r>>8)*299+(g>>8)*587+(b>>8)*114) / 1000
}

func main() {
	if len(os.Args) < 7 {
		fmt.Fprintln(os.Stderr, "usage: anchor_extract <png> <x> <y0> <y1> <threshold> <direction:above|below>")
		os.Exit(2)
	}
	path := os.Args[1]
	x, _ := strconv.Atoi(os.Args[2])
	y0, _ := strconv.Atoi(os.Args[3])
	y1, _ := strconv.Atoi(os.Args[4])
	thr, _ := strconv.Atoi(os.Args[5])
	dir := os.Args[6]
	f, err := os.Open(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	defer f.Close()
	img, _, err := image.Decode(f)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	bnd := img.Bounds()
	if x < bnd.Min.X || x >= bnd.Max.X {
		fmt.Fprintln(os.Stderr, "x out of bounds")
		os.Exit(2)
	}
	step := 1
	yStart, yEnd := y0, y1
	if y0 > y1 {
		step = -1
	}
	for y := yStart; (step > 0 && y <= yEnd) || (step < 0 && y >= yEnd); y += step {
		l := lum(img, bnd.Min.X+x, bnd.Min.Y+y)
		match := false
		if dir == "above" {
			match = l >= thr
		} else {
			match = l <= thr
		}
		if match {
			fmt.Printf("%s @ x=%d, lum=%d crosses thr=%d (%s) at y=%d\n", path, x, l, thr, dir, y)
			return
		}
	}
	fmt.Printf("%s @ x=%d: no crossing in y=[%d,%d]\n", path, x, y0, y1)
}
