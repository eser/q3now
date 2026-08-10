// png-perturb — synthesize a "defect" frame from a blessed golden, for the
// visual-gate self-tests (proving a gate FAILs on the defect class it guards).
//
// Three modes:
//   png-perturb <in.png> <out.png> <delta>            VALUE defect (brightness)
//       add a signed constant to each R,G,B (clamped 0..255). A uniform +40 is
//       the exposure/tonemap regression class the value goldens catch.
//   png-perturb --shift <dx> <dy> <in.png> <out.png>  PLACEMENT defect (translate)
//       translate the image by (dx,dy) px, filling the vacated edge with black.
//       A whole-frame translation is exactly a viewport-placement bug (geometry
//       framed at the wrong screen location) — feeding (golden, shifted) to the
//       viewport gate's tiled_diff MUST exceed the FAIL tolerance.
//   png-perturb --chromatic <maxPx> <in.png> <out.png>  RADIAL fringe (chromatic)
//       resample R/B at a radial per-channel UV offset (R pulled outward, B inward,
//       G fixed), the offset growing with distance from centre (0 at centre, maxPx
//       at the corner). This is exactly what tonemap.frag's sampleChromatic does, so
//       feeding (golden, synthetic) to the chromatic gate's edge_center_diff produces
//       a real EDGE-concentrated signal (edges shift, centre ~0) — the chromatic
//       gate's teeth, exercised on true image pixels with no engine launch.
package main

import (
	"fmt"
	"image"
	"image/png"
	"math"
	"os"
	"strconv"
)

func clamp(v int) uint8 {
	if v < 0 {
		return 0
	}
	if v > 255 {
		return 255
	}
	return uint8(v)
}

func readPNG(path string) (image.Image, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	img, _, err := image.Decode(f)
	return img, err
}

func writePNG(path string, img image.Image) error {
	out, err := os.Create(path)
	if err != nil {
		return err
	}
	defer out.Close()
	return png.Encode(out, img)
}

// brightness-delta mode
func doDelta(inPath, outPath string, delta int) {
	src, err := readPNG(inPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: read %s: %v\n", inPath, err)
		os.Exit(1)
	}
	b := src.Bounds()
	dst := image.NewNRGBA(b)
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			r, g, bl, a := src.At(x, y).RGBA()
			i := (y-b.Min.Y)*dst.Stride + (x-b.Min.X)*4
			dst.Pix[i+0] = clamp(int(r>>8) + delta)
			dst.Pix[i+1] = clamp(int(g>>8) + delta)
			dst.Pix[i+2] = clamp(int(bl>>8) + delta)
			dst.Pix[i+3] = uint8(a >> 8)
		}
	}
	if err := writePNG(outPath, dst); err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: write %s: %v\n", outPath, err)
		os.Exit(1)
	}
}

// translate (placement) mode: dst(x,y) = src(x-dx, y-dy); out-of-range -> black.
func doShift(dx, dy int, inPath, outPath string) {
	src, err := readPNG(inPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: read %s: %v\n", inPath, err)
		os.Exit(1)
	}
	b := src.Bounds()
	w, h := b.Dx(), b.Dy()
	dst := image.NewNRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			sx, sy := x-dx, y-dy
			i := y*dst.Stride + x*4
			if sx >= 0 && sx < w && sy >= 0 && sy < h {
				r, g, bl, a := src.At(b.Min.X+sx, b.Min.Y+sy).RGBA()
				dst.Pix[i+0] = uint8(r >> 8)
				dst.Pix[i+1] = uint8(g >> 8)
				dst.Pix[i+2] = uint8(bl >> 8)
				dst.Pix[i+3] = uint8(a >> 8)
			} else {
				dst.Pix[i+0], dst.Pix[i+1], dst.Pix[i+2], dst.Pix[i+3] = 0, 0, 0, 255
			}
		}
	}
	if err := writePNG(outPath, dst); err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: write %s: %v\n", outPath, err)
		os.Exit(1)
	}
}

// radial chromatic-aberration mode: mirrors tonemap.frag sampleChromatic(). For each
// output pixel, offset = (uv - centre) * radial * maxPx (radial = |uv - centre|, 0..~0.707),
// sample R at +offset, B at -offset, G at the pixel (nearest-neighbour, clamped to the
// frame). The result carries the same edge-concentrated radial fringe the shader produces,
// so the chromatic gate's edge_center_diff(in, out) sees a real edge>>centre signal.
func doChromatic(maxPx float64, inPath, outPath string) {
	src, err := readPNG(inPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: read %s: %v\n", inPath, err)
		os.Exit(1)
	}
	b := src.Bounds()
	w, h := b.Dx(), b.Dy()
	dst := image.NewNRGBA(image.Rect(0, 0, w, h))
	// sample the given channel at (fx,fy) in pixel space, clamped to the frame.
	sampleAt := func(fx, fy float64) (r, g, bl, a uint32) {
		sx := int(math.Round(fx))
		sy := int(math.Round(fy))
		if sx < 0 {
			sx = 0
		} else if sx >= w {
			sx = w - 1
		}
		if sy < 0 {
			sy = 0
		} else if sy >= h {
			sy = h - 1
		}
		return src.At(b.Min.X+sx, b.Min.Y+sy).RGBA()
	}
	cx, cy := float64(w)/2.0, float64(h)/2.0
	// normalise offsets in UV space so the radial profile matches the shader (which works
	// in [0,1] UV): dirUV = (uv - 0.5), radial = |dirUV| (max ~0.707 at the corner). The
	// per-channel pixel offset = dirUV * radial * (maxPx / 0.707-ish); we express it directly
	// in pixels: pixOffset = (px-centre)/|halfExtent| * radial * maxPx along each axis.
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			// UV-space direction from centre, components in [-0.5, 0.5].
			du := (float64(x) - cx) / float64(w)
			dv := (float64(y) - cy) / float64(h)
			radial := math.Sqrt(du*du + dv*dv) // 0 at centre, ~0.707 at corner
			// pixel offset along the radial direction, scaled by radial (edge-concentrated).
			ox := du * radial * maxPx * 2.0 // *2: du is half-normalised (range ±0.5)
			oy := dv * radial * maxPx * 2.0
			// R pulled outward (+offset), B inward (-offset), G at the pixel.
			rr, _, _, _ := sampleAt(float64(x)+ox, float64(y)+oy)
			_, gg, _, _ := sampleAt(float64(x), float64(y))
			_, _, bb, aa := sampleAt(float64(x)-ox, float64(y)-oy)
			i := y*dst.Stride + x*4
			dst.Pix[i+0] = uint8(rr >> 8)
			dst.Pix[i+1] = uint8(gg >> 8)
			dst.Pix[i+2] = uint8(bb >> 8)
			dst.Pix[i+3] = uint8(aa >> 8)
		}
	}
	if err := writePNG(outPath, dst); err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: write %s: %v\n", outPath, err)
		os.Exit(1)
	}
}

func atofOrDie(s, what string) float64 {
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: bad %s %q: %v\n", what, s, err)
		os.Exit(2)
	}
	return v
}

func atoiOrDie(s, what string) int {
	v, err := strconv.Atoi(s)
	if err != nil {
		fmt.Fprintf(os.Stderr, "png-perturb: bad %s %q: %v\n", what, s, err)
		os.Exit(2)
	}
	return v
}

func main() {
	args := os.Args[1:]
	if len(args) == 5 && args[0] == "--shift" {
		doShift(atoiOrDie(args[1], "dx"), atoiOrDie(args[2], "dy"), args[3], args[4])
		return
	}
	if len(args) == 4 && args[0] == "--chromatic" {
		doChromatic(atofOrDie(args[1], "maxPx"), args[2], args[3])
		return
	}
	if len(args) == 3 {
		doDelta(args[0], args[1], atoiOrDie(args[2], "delta"))
		return
	}
	fmt.Fprintln(os.Stderr, "usage: png-perturb <in.png> <out.png> <delta>")
	fmt.Fprintln(os.Stderr, "       png-perturb --shift <dx> <dy> <in.png> <out.png>")
	fmt.Fprintln(os.Stderr, "       png-perturb --chromatic <maxPx> <in.png> <out.png>")
	os.Exit(2)
}
