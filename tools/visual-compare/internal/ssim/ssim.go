// Package ssim implements the Wang-Bovik-Sheikh-Simoncelli (2004) Structural
// Similarity Index Measure with an 8x8 uniform window, per-RGB-channel
// averaged. C1=(0.01*255)^2, C2=(0.03*255)^2.
package ssim

import (
	"image"
	"math"
)

const (
	windowSize = 8
	L          = 255.0
)

var (
	c1 = math.Pow(0.01*L, 2)
	c2 = math.Pow(0.03*L, 2)
)

// Image extracts the per-channel byte planes from an image.Image, clipping to
// the region (x0,y0,w,h) inside the image's bounds. Returns RGB planes of size
// w*h each, row-major.
func planes(img image.Image, x0, y0, w, h int) (r, g, b []float64) {
	r = make([]float64, w*h)
	g = make([]float64, w*h)
	b = make([]float64, w*h)
	bnd := img.Bounds()
	for dy := 0; dy < h; dy++ {
		for dx := 0; dx < w; dx++ {
			cr, cg, cb, _ := img.At(bnd.Min.X+x0+dx, bnd.Min.Y+y0+dy).RGBA()
			idx := dy*w + dx
			r[idx] = float64(cr >> 8)
			g[idx] = float64(cg >> 8)
			b[idx] = float64(cb >> 8)
		}
	}
	return
}

// channelSSIM computes mean SSIM over all 8x8 windows fitting in a w*h plane.
func channelSSIM(a, b []float64, w, h int) float64 {
	if w < windowSize || h < windowSize {
		return globalChannelSSIM(a, b, w, h)
	}
	var sum float64
	count := 0
	for y0 := 0; y0+windowSize <= h; y0++ {
		for x0 := 0; x0+windowSize <= w; x0++ {
			sum += windowSSIM(a, b, w, x0, y0)
			count++
		}
	}
	if count == 0 {
		return 1.0
	}
	return sum / float64(count)
}

// windowSSIM computes SSIM for one 8x8 window anchored at (x0,y0).
func windowSSIM(a, b []float64, stride, x0, y0 int) float64 {
	var sumA, sumB float64
	for dy := 0; dy < windowSize; dy++ {
		for dx := 0; dx < windowSize; dx++ {
			idx := (y0+dy)*stride + x0 + dx
			sumA += a[idx]
			sumB += b[idx]
		}
	}
	n := float64(windowSize * windowSize)
	muA := sumA / n
	muB := sumB / n

	var varA, varB, cov float64
	for dy := 0; dy < windowSize; dy++ {
		for dx := 0; dx < windowSize; dx++ {
			idx := (y0+dy)*stride + x0 + dx
			da := a[idx] - muA
			db := b[idx] - muB
			varA += da * da
			varB += db * db
			cov += da * db
		}
	}
	varA /= n - 1
	varB /= n - 1
	cov /= n - 1

	num := (2*muA*muB + c1) * (2*cov + c2)
	den := (muA*muA + muB*muB + c1) * (varA + varB + c2)
	if den == 0 {
		return 1.0
	}
	return num / den
}

// globalChannelSSIM falls back to a single window over the whole plane when
// the region is smaller than the window size.
func globalChannelSSIM(a, b []float64, w, h int) float64 {
	if w == 0 || h == 0 {
		return 1.0
	}
	n := float64(w * h)
	var sumA, sumB float64
	for i := 0; i < len(a); i++ {
		sumA += a[i]
		sumB += b[i]
	}
	muA := sumA / n
	muB := sumB / n
	var varA, varB, cov float64
	for i := 0; i < len(a); i++ {
		da := a[i] - muA
		db := b[i] - muB
		varA += da * da
		varB += db * db
		cov += da * db
	}
	denom := n - 1
	if denom <= 0 {
		denom = 1
	}
	varA /= denom
	varB /= denom
	cov /= denom
	num := (2*muA*muB + c1) * (2*cov + c2)
	den := (muA*muA + muB*muB + c1) * (varA + varB + c2)
	if den == 0 {
		return 1.0
	}
	return num / den
}

// Compute returns per-RGB-channel-averaged mean SSIM over the region rect of
// two images.  Both images must contain the rect (x0,y0,w,h) within bounds.
// Result in [-1, +1] clamped to [0, 1] for threshold semantics.
func Compute(a, b image.Image, x0, y0, w, h int) float64 {
	ar, ag, ab := planes(a, x0, y0, w, h)
	br, bg, bb := planes(b, x0, y0, w, h)
	sr := channelSSIM(ar, br, w, h)
	sg := channelSSIM(ag, bg, w, h)
	sb := channelSSIM(ab, bb, w, h)
	mean := (sr + sg + sb) / 3.0
	if mean < 0 {
		return 0
	}
	if mean > 1 {
		return 1
	}
	return mean
}

// Global is a convenience over Compute for a full-image SSIM.
func Global(a, b image.Image) float64 {
	bnd := a.Bounds()
	return Compute(a, b, 0, 0, bnd.Dx(), bnd.Dy())
}
