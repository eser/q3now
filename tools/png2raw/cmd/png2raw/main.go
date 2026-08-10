// png2raw <file.png> — decode a PNG to a flat raw RGB byte stream on stdout.
//
// Output layout: W*H*3 bytes, row-major, top-down, 8-bit R,G,B per pixel,
// alpha dropped. No header, no padding, no row stride beyond W*3. This is the
// exact flat stream the smoke-harness `od -t u1` pipeline consumes (the same
// shape the old raw-TGA `dd skip=18` read produced, minus the 18-byte TGA
// header and minus TGA's BGR/bottom-up quirks — Go's image/png decode
// normalises orientation to top-down).
//
// Values are emitted byte-exact: no gamma correction, no premultiply, no
// rounding. PNG is lossless, so the pixel bytes match what the engine wrote
// and the downstream block-mean / tile-diff thresholds carry over unchanged.
package main

import (
	"bufio"
	"fmt"
	"image"
	"image/png"
	"os"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: png2raw <file.png>")
		os.Exit(2)
	}

	f, err := os.Open(os.Args[1])
	if err != nil {
		fmt.Fprintf(os.Stderr, "png2raw: open %s: %v\n", os.Args[1], err)
		os.Exit(1)
	}
	defer f.Close()

	img, err := png.Decode(f)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png2raw: decode %s: %v\n", os.Args[1], err)
		os.Exit(1)
	}

	b := img.Bounds()
	w := b.Dx()

	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()

	// Fast path for the common 8-bit RGBA-backed image: index the pixel
	// buffer directly and drop the alpha byte. Falls back to the generic
	// At()/RGBA() path (16-bit-per-channel, paletted, gray, …) which Go
	// scales every channel to the 0..65535 range — we take the high byte to
	// recover the 8-bit value the engine encoded.
	if rgba, ok := img.(*image.RGBA); ok {
		row := make([]byte, w*3)
		for y := b.Min.Y; y < b.Max.Y; y++ {
			o := 0
			p := rgba.PixOffset(b.Min.X, y)
			for x := 0; x < w; x++ {
				row[o+0] = rgba.Pix[p+0]
				row[o+1] = rgba.Pix[p+1]
				row[o+2] = rgba.Pix[p+2]
				o += 3
				p += 4
			}
			out.Write(row)
		}
		return
	}

	row := make([]byte, w*3)
	for y := b.Min.Y; y < b.Max.Y; y++ {
		o := 0
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA() // each 0..65535
			row[o+0] = byte(r >> 8)
			row[o+1] = byte(g >> 8)
			row[o+2] = byte(bl >> 8)
			o += 3
		}
		out.Write(row)
	}
}
