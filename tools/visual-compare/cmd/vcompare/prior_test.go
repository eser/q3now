package main

import (
	"image"
	"image/color"
	"os"
	"path/filepath"
	"testing"
	"time"
)

// writeRun materialises results/<cfg>/<name>/result.json with the given body
// and mtime, mirroring the layout vcompare_run.sh / compare.sh produce.
func writeRun(t *testing.T, cfgDir, name, body string, mod time.Time) string {
	t.Helper()
	dir := filepath.Join(cfgDir, name)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatal(err)
	}
	p := filepath.Join(dir, "result.json")
	if err := os.WriteFile(p, []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Chtimes(p, mod, mod); err != nil {
		t.Fatal(err)
	}
	return p
}

const vcompareResult = `{"tool":"vcompare",
 "ssim":{"global":{"threshold":0.5},"regions":{"r":{"name":"r","threshold":0.45}}},
 "deltaE2000":{"global":{"threshold":12.0},"regions":{}},
 "structural":{"global":{"threshold":0.10},"regions":{}},
 "verdict":"pass"}`

// vdiff's schema shares no field with vcompare's. Read as a RunResult it gives
// zero thresholds everywhere, which enforceThresholdUp skips via its
// `oldVal == 0` short-circuit — the loosen guard is silently disarmed rather
// than loudly wrong. That is the more dangerous failure of the two.
const vdiffResult = `{"tool":"vdiff","global_threshold_pct":5.0,"regions":[{"name":"r","threshold_pct":10.0}]}`

func TestForeignToolResultIsNotAPriorRun(t *testing.T) {
	cfgDir := t.TempDir()
	base := time.Now().Add(-time.Hour)
	writeRun(t, cfgDir, "20260101_000000", vcompareResult, base)
	writeRun(t, cfgDir, "v2b", vdiffResult, base.Add(30*time.Minute))

	prior, path, err := findPriorResult(filepath.Join(cfgDir, "20260102_000000"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior == nil {
		t.Fatal("expected the genuine vcompare prior to be found")
	}
	if filepath.Base(filepath.Dir(path)) != "20260101_000000" {
		t.Fatalf("selected %s; expected the vcompare run, not the vdiff one", path)
	}
	if prior.SSIM.Global.Threshold != 0.5 {
		t.Fatalf("ssim global threshold = %v; want 0.5 (0 means a foreign schema leaked in)",
			prior.SSIM.Global.Threshold)
	}
}

// A disarmed guard is the failure mode this closes: with only a vdiff result
// present, vcompare must report NO prior rather than adopt a prior whose zero
// thresholds let any loosening through.
func TestOnlyForeignResultsMeansNoPrior(t *testing.T) {
	cfgDir := t.TempDir()
	writeRun(t, cfgDir, "v2b", vdiffResult, time.Now())

	prior, _, err := findPriorResult(filepath.Join(cfgDir, "20260102_000000"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior != nil {
		t.Fatalf("expected no prior run; got %+v", prior)
	}
}

func TestPriorSelectedByModTimeNotName(t *testing.T) {
	cfgDir := t.TempDir()
	now := time.Now()
	writeRun(t, cfgDir, "aaa_oldname_newest", vcompareResult, now)
	writeRun(t, cfgDir, "zzz_newname_oldest", vcompareResult, now.Add(-2*time.Hour))

	_, path, err := findPriorResult(filepath.Join(cfgDir, "current"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if got := filepath.Base(filepath.Dir(path)); got != "aaa_oldname_newest" {
		t.Fatalf("selected %s; expected the most recently WRITTEN run", got)
	}
}

func TestUnmarkedResultIsNotAPriorRun(t *testing.T) {
	cfgDir := t.TempDir()
	writeRun(t, cfgDir, "20260101_000000", `{"ssim":{"global":{"threshold":0.5}}}`, time.Now())

	prior, _, err := findPriorResult(filepath.Join(cfgDir, "20260102_000000"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior != nil {
		t.Fatal("an unmarked result must not be adopted as a prior run")
	}
}

// --- ink check -------------------------------------------------------------

// solid builds a w*h image filled with one colour, optionally with a filled
// rect of a second colour — enough to stand in for "HUD drew here" vs "nothing
// drew here" without needing an engine.
func solid(w, h int, base color.RGBA, mark *image.Rectangle, markCol color.RGBA) image.Image {
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			c := base
			if mark != nil && x >= mark.Min.X && x < mark.Max.X && y >= mark.Min.Y && y < mark.Max.Y {
				c = markCol
			}
			img.Set(x, y, c)
		}
	}
	return img
}

// A region where the HUD-on and HUD-off frames are identical contains no HUD
// ink, whatever the pixels happen to look like. This is the false-green case:
// such a region cannot fail an image comparison, because both sides show the
// same background.
func TestRegionInkPctZeroWhenFramesMatch(t *testing.T) {
	bg := color.RGBA{40, 30, 20, 255}
	on := solid(200, 200, bg, nil, bg)
	off := solid(200, 200, bg, nil, bg)
	if got := regionInkPct(on, off, 10, 10, 100, 100); got != 0 {
		t.Fatalf("ink = %v%%; want 0%% when the two frames are identical", got)
	}
}

// Ink is measured only where the frames differ, so a widget drawn in one
// corner does not make a neighbouring region look alive.
func TestRegionInkPctIsLocal(t *testing.T) {
	bg := color.RGBA{40, 30, 20, 255}
	drawn := image.Rect(0, 0, 50, 50)
	on := solid(200, 200, bg, &drawn, color.RGBA{240, 240, 240, 255})
	off := solid(200, 200, bg, nil, bg)

	if got := regionInkPct(on, off, 0, 0, 50, 50); got != 100 {
		t.Fatalf("ink over the drawn rect = %v%%; want 100%%", got)
	}
	if got := regionInkPct(on, off, 100, 100, 50, 50); got != 0 {
		t.Fatalf("ink over an undrawn rect = %v%%; want 0%%", got)
	}
}

// A difference at or below the tolerance is not ink — it must not lift an
// empty region over the floor.
func TestRegionInkPctIgnoresSubToleranceNoise(t *testing.T) {
	bg := color.RGBA{40, 30, 20, 255}
	noisy := color.RGBA{40 + inkTolerance, 30, 20, 255}
	on := solid(100, 100, noisy, nil, noisy)
	off := solid(100, 100, bg, nil, bg)
	if got := regionInkPct(on, off, 0, 0, 100, 100); got != 0 {
		t.Fatalf("ink = %v%%; a delta of exactly inkTolerance must not count", got)
	}
}

// The floor must sit far below real content and far above an empty region.
// Measured on V2_HUD_Active: real regions 9.1-90.8%, empty regions 0.000%.
func TestInkFloorSeparatesMeasuredExtremes(t *testing.T) {
	const measuredEmpty, measuredSmallestReal = 0.000, 9.1364
	if measuredEmpty >= inkFloorPct {
		t.Fatalf("floor %v does not clear a measured-empty region (%v)", inkFloorPct, measuredEmpty)
	}
	if measuredSmallestReal <= inkFloorPct {
		t.Fatalf("floor %v rejects the smallest measured real region (%v)", inkFloorPct, measuredSmallestReal)
	}
}

// The ink check assumes the two frames are the same scene differing only by the
// HUD. A run whose engine failed to load the map paired a MENU screenshot with
// a HUD frame; every region then measured 60-100% "ink" and the config was
// recorded PASS on a menu-vs-arena comparison. Region-level ink cannot catch
// that — the frames must be checked as a pair.
func TestFrameInkSeparatesValidPairFromDifferentScenes(t *testing.T) {
	const measuredValidPair = 3.26 // all five palette configs: 3.24-3.26%
	const measuredMismatch = 94.83 // menu screenshot vs HUD frame
	if measuredValidPair >= maxFrameInkPct {
		t.Fatalf("cap %v rejects a measured-valid pair (%v%%)", maxFrameInkPct, measuredValidPair)
	}
	if measuredMismatch <= maxFrameInkPct {
		t.Fatalf("cap %v accepts a measured scene mismatch (%v%%)", maxFrameInkPct, measuredMismatch)
	}
}

// Two entirely different images differ over essentially the whole frame, which
// is what the cap exists to reject.
func TestFrameInkIsHugeForDifferentScenes(t *testing.T) {
	dark := solid(200, 200, color.RGBA{10, 10, 10, 255}, nil, color.RGBA{})
	light := solid(200, 200, color.RGBA{200, 200, 200, 255}, nil, color.RGBA{})
	got := regionInkPct(dark, light, 0, 0, 200, 200)
	if got <= maxFrameInkPct {
		t.Fatalf("frame ink between two different scenes = %v%%; must exceed the %v%% cap", got, maxFrameInkPct)
	}
}
