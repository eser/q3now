package deltae

import (
	"image"
	"image/color"
	"math"
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

func TestIdentical(t *testing.T) {
	a := solid(color.RGBA{100, 100, 100, 255}, 16, 16)
	v := Global(a, a)
	if v > 0.001 {
		t.Fatalf("identical deltaE should be 0, got %f", v)
	}
}

func TestBlackVsWhite(t *testing.T) {
	a := solid(color.RGBA{0, 0, 0, 255}, 16, 16)
	b := solid(color.RGBA{255, 255, 255, 255}, 16, 16)
	v := Global(a, b)
	// CIEDE2000 between pure black and white is around 100 (CIELAB L*
	// extremes); accept a wide band — anything > 80 confirms ordering.
	if v < 80 {
		t.Fatalf("B/W deltaE should be >=80, got %f", v)
	}
}

// Sharma-Wu-Dalal reference test pairs (subset). These are canonical
// validation rows from the CIEDE2000 supplementary test data.
func TestSharmaReferencePairs(t *testing.T) {
	cases := []struct {
		L1, a1, b1, L2, a2, b2, want float64
	}{
		{50.0000, 2.6772, -79.7751, 50.0000, 0.0000, -82.7485, 2.0425},
		{50.0000, 3.1571, -77.2803, 50.0000, 0.0000, -82.7485, 2.8615},
		{50.0000, -1.3802, -84.2814, 50.0000, 0.0000, -82.7485, 1.0000},
		{50.0000, 0.0000, 0.0000, 50.0000, -1.0000, 2.0000, 2.3669},
	}
	for i, c := range cases {
		got := CIEDE2000(c.L1, c.a1, c.b1, c.L2, c.a2, c.b2)
		if math.Abs(got-c.want) > 0.01 {
			t.Errorf("case %d: want ΔE=%f, got %f", i, c.want, got)
		}
	}
}
