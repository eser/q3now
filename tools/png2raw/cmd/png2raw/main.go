// png2raw [--size WxH] <file.png> — decode a PNG to a flat raw RGB byte
// stream on stdout. --size normalises HiDPI captures to a fixed comparison
// extent before emitting pixels.
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
	"image/png"
	"math"
	"os"
	"strconv"
	"strings"
)

func main() {
	path := ""
	targetW, targetH := 0, 0
	for i := 1; i < len(os.Args); i++ {
		if os.Args[i] == "--size" {
			if i+1 >= len(os.Args) {
				fmt.Fprintln(os.Stderr, "png2raw: --size requires WxH")
				os.Exit(2)
			}
			i++
			parts := strings.Split(os.Args[i], "x")
			if len(parts) != 2 {
				fmt.Fprintf(os.Stderr, "png2raw: invalid size %q (expected WxH)\n", os.Args[i])
				os.Exit(2)
			}
			var err error
			targetW, err = strconv.Atoi(parts[0])
			if err != nil || targetW <= 0 {
				fmt.Fprintf(os.Stderr, "png2raw: invalid width in %q\n", os.Args[i])
				os.Exit(2)
			}
			targetH, err = strconv.Atoi(parts[1])
			if err != nil || targetH <= 0 {
				fmt.Fprintf(os.Stderr, "png2raw: invalid height in %q\n", os.Args[i])
				os.Exit(2)
			}
		} else if strings.HasPrefix(os.Args[i], "-") {
			fmt.Fprintf(os.Stderr, "png2raw: unknown option %q\n", os.Args[i])
			os.Exit(2)
		} else if path == "" {
			path = os.Args[i]
		} else {
			fmt.Fprintln(os.Stderr, "usage: png2raw [--size WxH] <file.png>")
			os.Exit(2)
		}
	}
	if path == "" {
		fmt.Fprintln(os.Stderr, "usage: png2raw [--size WxH] <file.png>")
		os.Exit(2)
	}

	f, err := os.Open(path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png2raw: open %s: %v\n", path, err)
		os.Exit(1)
	}
	defer f.Close()

	img, err := png.Decode(f)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png2raw: decode %s: %v\n", path, err)
		os.Exit(1)
	}

	b := img.Bounds()
	sourceW, sourceH := b.Dx(), b.Dy()
	if targetW == 0 {
		targetW, targetH = sourceW, sourceH
	}

	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()

	// Flatten once so resize paths and the byte-exact no-resize path share the
	// same colour conversion. PNG decode exposes 16-bit RGBA values; the high
	// byte is the original 8-bit channel written by the engine.
	source := make([]byte, sourceW*sourceH*3)
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, _ := img.At(x, y).RGBA() // each 0..65535
			o := ((y-b.Min.Y)*sourceW + (x - b.Min.X)) * 3
			source[o+0] = byte(r >> 8)
			source[o+1] = byte(g >> 8)
			source[o+2] = byte(bl >> 8)
		}
	}
	if sourceW == targetW && sourceH == targetH {
		_, _ = out.Write(source)
		return
	}

	row := make([]byte, targetW*3)
	integerDownscale := sourceW%targetW == 0 && sourceH%targetH == 0 &&
		sourceW >= targetW && sourceH >= targetH
	if integerDownscale {
		// Retina's common 2x backing is reduced with an exact box average. This
		// compares the same logical pixel footprint as a native 1x capture and
		// avoids selecting an arbitrary subpixel from the HiDPI render.
		sx, sy := sourceW/targetW, sourceH/targetH
		area := uint64(sx * sy)
		for y := 0; y < targetH; y++ {
			for x := 0; x < targetW; x++ {
				var sum [3]uint64
				for yy := 0; yy < sy; yy++ {
					for xx := 0; xx < sx; xx++ {
						i := (((y*sy + yy) * sourceW) + (x*sx + xx)) * 3
						sum[0] += uint64(source[i+0])
						sum[1] += uint64(source[i+1])
						sum[2] += uint64(source[i+2])
					}
				}
				o := x * 3
				row[o+0] = byte((sum[0] + area/2) / area)
				row[o+1] = byte((sum[1] + area/2) / area)
				row[o+2] = byte((sum[2] + area/2) / area)
			}
			_, _ = out.Write(row)
		}
		return
	}

	// Fractional display scales (for example 1.25x on Windows) use bilinear
	// sampling at logical-pixel centres. The harness still gets an exact target
	// extent rather than silently interpreting a different row stride.
	for y := 0; y < targetH; y++ {
		fy := (float64(y)+0.5)*float64(sourceH)/float64(targetH) - 0.5
		y0 := int(math.Floor(fy))
		wy := fy - float64(y0)
		if y0 < 0 {
			y0, wy = 0, 0
		}
		y1 := y0 + 1
		if y1 >= sourceH {
			y1 = sourceH - 1
		}
		for x := 0; x < targetW; x++ {
			fx := (float64(x)+0.5)*float64(sourceW)/float64(targetW) - 0.5
			x0 := int(math.Floor(fx))
			wx := fx - float64(x0)
			if x0 < 0 {
				x0, wx = 0, 0
			}
			x1 := x0 + 1
			if x1 >= sourceW {
				x1 = sourceW - 1
			}
			for c := 0; c < 3; c++ {
				p00 := float64(source[(y0*sourceW+x0)*3+c])
				p10 := float64(source[(y0*sourceW+x1)*3+c])
				p01 := float64(source[(y1*sourceW+x0)*3+c])
				p11 := float64(source[(y1*sourceW+x1)*3+c])
				top := p00 + (p10-p00)*wx
				bottom := p01 + (p11-p01)*wx
				row[x*3+c] = byte(math.Round(top + (bottom-top)*wy))
			}
		}
		_, _ = out.Write(row)
	}
}
