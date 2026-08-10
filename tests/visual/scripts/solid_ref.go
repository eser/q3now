// Generate solid-color reference PNGs for dispatch 5c S3.
// go run solid_ref.go <hex> <w> <h> <out.png>
package main

import (
	"fmt"
	"image"
	"image/color"
	"image/png"
	"os"
	"strconv"
)

func main() {
	if len(os.Args) < 5 {
		fmt.Fprintln(os.Stderr, "usage: solid_ref <hex> <w> <h> <out.png>")
		os.Exit(2)
	}
	hex := os.Args[1]
	w, _ := strconv.Atoi(os.Args[2])
	h, _ := strconv.Atoi(os.Args[3])
	rN, _ := strconv.ParseInt(hex[0:2], 16, 32)
	gN, _ := strconv.ParseInt(hex[2:4], 16, 32)
	bN, _ := strconv.ParseInt(hex[4:6], 16, 32)
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	c := color.RGBA{uint8(rN), uint8(gN), uint8(bN), 255}
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			img.Set(x, y, c)
		}
	}
	f, _ := os.Create(os.Args[4])
	defer f.Close()
	_ = png.Encode(f, img)
}
