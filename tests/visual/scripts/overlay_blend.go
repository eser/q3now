// One-shot overlay tool for dispatch 5c S2 — 50/50 alpha-blends two
// same-resolution PNGs and writes the result. Not part of any build target;
// invoked manually from the diagnostic loop, lives next to other harness
// scripts.  go run tests/visual/scripts/overlay_blend.go <a.png> <b.png> <out.png>
package main

import (
	"fmt"
	"image"
	"image/color"
	"image/png"
	_ "image/jpeg"
	"os"
)

func loadPNG(path string) image.Image {
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
	return img
}

func main() {
	if len(os.Args) < 4 {
		fmt.Fprintln(os.Stderr, "usage: overlay_blend <a.png> <b.png> <out.png>")
		os.Exit(2)
	}
	a := loadPNG(os.Args[1])
	b := loadPNG(os.Args[2])
	ab := a.Bounds()
	bb := b.Bounds()
	w := ab.Dx()
	h := ab.Dy()
	if bb.Dx() < w {
		w = bb.Dx()
	}
	if bb.Dy() < h {
		h = bb.Dy()
	}
	out := image.NewRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			ar, ag, ablu, _ := a.At(ab.Min.X+x, ab.Min.Y+y).RGBA()
			br, bg, bblu, _ := b.At(bb.Min.X+x, bb.Min.Y+y).RGBA()
			out.Set(x, y, color.RGBA{
				R: uint8((ar>>8 + br>>8) / 2),
				G: uint8((ag>>8 + bg>>8) / 2),
				B: uint8((ablu>>8 + bblu>>8) / 2),
				A: 255,
			})
		}
	}
	f, err := os.Create(os.Args[3])
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	defer f.Close()
	_ = png.Encode(f, out)
	fmt.Printf("overlay: %s (%dx%d)\n", os.Args[3], w, h)
}
