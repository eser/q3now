package ssim

import (
	"image"
	"image/color"
	"math/rand"
	"testing"
)

func solid(c color.RGBA, w, h int) *image.RGBA {
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			img.Set(x, y, c)
		}
	}
	return img
}

func noise(w, h int, seed int64) *image.RGBA {
	r := rand.New(rand.NewSource(seed))
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			img.Set(x, y, color.RGBA{uint8(r.Intn(256)), uint8(r.Intn(256)), uint8(r.Intn(256)), 255})
		}
	}
	return img
}

func TestIdentical(t *testing.T) {
	a := solid(color.RGBA{128, 64, 200, 255}, 64, 64)
	v := Global(a, a)
	if v < 0.999 {
		t.Fatalf("identical SSIM should be ~1.000, got %f", v)
	}
}

func TestNoisePair(t *testing.T) {
	a := noise(64, 64, 1)
	b := noise(64, 64, 2)
	v := Global(a, b)
	if v > 0.10 {
		t.Fatalf("noise-pair SSIM should be < 0.10, got %f", v)
	}
}

func TestRegionSubset(t *testing.T) {
	a := solid(color.RGBA{200, 50, 100, 255}, 128, 128)
	v := Compute(a, a, 16, 16, 64, 64)
	if v < 0.999 {
		t.Fatalf("identical region SSIM should be ~1.000, got %f", v)
	}
}
