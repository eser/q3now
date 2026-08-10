// Package deltae implements CIEDE2000 (Sharma-Wu-Dalal 2005). sRGB → linear
// RGB → XYZ (D65) → Lab → ΔE_00. Per-pixel averaged for regions.
package deltae

import (
	"image"
	"math"
)

// D65 reference white in XYZ (normalized so Y=1.0).
const (
	xnD65 = 0.95047
	ynD65 = 1.00000
	znD65 = 1.08883
)

func srgbToLinear(c float64) float64 {
	c /= 255.0
	if c <= 0.04045 {
		return c / 12.92
	}
	return math.Pow((c+0.055)/1.055, 2.4)
}

func linearToXYZ(r, g, b float64) (x, y, z float64) {
	x = 0.4124564*r + 0.3575761*g + 0.1804375*b
	y = 0.2126729*r + 0.7151522*g + 0.0721750*b
	z = 0.0193339*r + 0.1191920*g + 0.9503041*b
	return
}

func labF(t float64) float64 {
	const delta = 6.0 / 29.0
	if t > delta*delta*delta {
		return math.Cbrt(t)
	}
	return t/(3*delta*delta) + 4.0/29.0
}

// SRGBToLab maps an sRGB triple in [0,255] to CIE L*a*b* under D65.
func SRGBToLab(r8, g8, b8 uint8) (L, a, b float64) {
	rl := srgbToLinear(float64(r8))
	gl := srgbToLinear(float64(g8))
	bl := srgbToLinear(float64(b8))
	x, y, z := linearToXYZ(rl, gl, bl)
	fx := labF(x / xnD65)
	fy := labF(y / ynD65)
	fz := labF(z / znD65)
	L = 116*fy - 16
	a = 500 * (fx - fy)
	b = 200 * (fy - fz)
	return
}

func deg(rad float64) float64 { return rad * 180.0 / math.Pi }
func rad(d float64) float64   { return d * math.Pi / 180.0 }

// CIEDE2000 returns the perceptual color difference between two Lab pairs.
func CIEDE2000(L1, a1, b1, L2, a2, b2 float64) float64 {
	const kL, kC, kH = 1.0, 1.0, 1.0

	C1 := math.Hypot(a1, b1)
	C2 := math.Hypot(a2, b2)
	Cbar := (C1 + C2) / 2.0

	C7 := math.Pow(Cbar, 7)
	G := 0.5 * (1.0 - math.Sqrt(C7/(C7+math.Pow(25, 7))))

	a1p := (1.0 + G) * a1
	a2p := (1.0 + G) * a2
	C1p := math.Hypot(a1p, b1)
	C2p := math.Hypot(a2p, b2)

	h1p := 0.0
	if a1p != 0 || b1 != 0 {
		h1p = deg(math.Atan2(b1, a1p))
		if h1p < 0 {
			h1p += 360
		}
	}
	h2p := 0.0
	if a2p != 0 || b2 != 0 {
		h2p = deg(math.Atan2(b2, a2p))
		if h2p < 0 {
			h2p += 360
		}
	}

	dLp := L2 - L1
	dCp := C2p - C1p

	var dhp float64
	if C1p*C2p == 0 {
		dhp = 0
	} else {
		dhp = h2p - h1p
		if dhp > 180 {
			dhp -= 360
		} else if dhp < -180 {
			dhp += 360
		}
	}
	dHp := 2 * math.Sqrt(C1p*C2p) * math.Sin(rad(dhp/2))

	Lpbar := (L1 + L2) / 2.0
	Cpbar := (C1p + C2p) / 2.0

	var Hpbar float64
	if C1p*C2p == 0 {
		Hpbar = h1p + h2p
	} else {
		Hpbar = h1p + h2p
		if math.Abs(h1p-h2p) > 180 {
			if Hpbar < 360 {
				Hpbar += 360
			} else {
				Hpbar -= 360
			}
		}
		Hpbar /= 2.0
	}

	T := 1 -
		0.17*math.Cos(rad(Hpbar-30)) +
		0.24*math.Cos(rad(2*Hpbar)) +
		0.32*math.Cos(rad(3*Hpbar+6)) -
		0.20*math.Cos(rad(4*Hpbar-63))

	dTheta := 30 * math.Exp(-math.Pow((Hpbar-275)/25, 2))
	Cp7 := math.Pow(Cpbar, 7)
	Rc := 2 * math.Sqrt(Cp7/(Cp7+math.Pow(25, 7)))

	SL := 1 + (0.015*math.Pow(Lpbar-50, 2))/math.Sqrt(20+math.Pow(Lpbar-50, 2))
	SC := 1 + 0.045*Cpbar
	SH := 1 + 0.015*Cpbar*T

	RT := -math.Sin(rad(2*dTheta)) * Rc

	dL := dLp / (kL * SL)
	dC := dCp / (kC * SC)
	dH := dHp / (kH * SH)
	return math.Sqrt(dL*dL + dC*dC + dH*dH + RT*dC*dH)
}

// Compute returns the mean CIEDE2000 over the region rect of two images.
func Compute(a, b image.Image, x0, y0, w, h int) float64 {
	if w == 0 || h == 0 {
		return 0
	}
	ba := a.Bounds()
	bb := b.Bounds()
	var sum float64
	count := 0
	for dy := 0; dy < h; dy++ {
		for dx := 0; dx < w; dx++ {
			ar, ag, ab, _ := a.At(ba.Min.X+x0+dx, ba.Min.Y+y0+dy).RGBA()
			br, bg, bbl, _ := b.At(bb.Min.X+x0+dx, bb.Min.Y+y0+dy).RGBA()
			L1, A1, B1 := SRGBToLab(uint8(ar>>8), uint8(ag>>8), uint8(ab>>8))
			L2, A2, B2 := SRGBToLab(uint8(br>>8), uint8(bg>>8), uint8(bbl>>8))
			sum += CIEDE2000(L1, A1, B1, L2, A2, B2)
			count++
		}
	}
	if count == 0 {
		return 0
	}
	return sum / float64(count)
}

// Global is a convenience over Compute for a full-image average.
func Global(a, b image.Image) float64 {
	bnd := a.Bounds()
	return Compute(a, b, 0, 0, bnd.Dx(), bnd.Dy())
}
