package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"image"
	"image/color"
	_ "image/jpeg"
	"image/png"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type Region struct {
	Name         string  `json:"name"`
	X            int     `json:"x"`
	Y            int     `json:"y"`
	W            int     `json:"w"`
	H            int     `json:"h"`
	ThresholdPct float64 `json:"threshold_pct"`
}

type RegionsFile struct {
	GlobalThresholdPct float64  `json:"global_threshold_pct"`
	FuzzPct            float64  `json:"fuzz_pct"`
	Regions            []Region `json:"regions"`
}

type CeilingConfig struct {
	GlobalCeilingPct   float64 `json:"global_ceiling_pct"`
	GlobalStartingPct  float64 `json:"global_starting_pct"`
	RegionCeilingPct   float64 `json:"region_ceiling_pct"`
	RegionStartingPct  float64 `json:"region_starting_pct"`
	FuzzPct            float64 `json:"fuzz_pct"`
}

type RegionResult struct {
	Name         string  `json:"name"`
	ThresholdPct float64 `json:"threshold_pct"`
	DeltaPct     float64 `json:"delta_pct"`
	Pass         bool    `json:"pass"`
}

// toolMarker identifies which tool wrote a result.json. vdiff and vcompare
// write their results into the SAME results/<artboard>_<mode>_<accent>/
// directory tree, and their RunResult schemas share no fields. Without a
// marker, findPriorResult happily unmarshals a vcompare result.json into
// vdiff's struct: every field misses, GlobalThresholdPct silently becomes 0,
// and the threshold-up guard then reads any real threshold as a loosening and
// HALTs (exit 2) before a single pixel is compared. Encoding is not a
// substitute for identification — a prior run is only a prior run of THIS
// tool.
const toolMarker = "vdiff"

type RunResult struct {
	Tool               string         `json:"tool"`
	Baseline           string         `json:"baseline"`
	Impl               string         `json:"impl"`
	Diff               string         `json:"diff"`
	BaselineW          int            `json:"baseline_w"`
	BaselineH          int            `json:"baseline_h"`
	ImplW              int            `json:"impl_w"`
	ImplH              int            `json:"impl_h"`
	GlobalThresholdPct float64        `json:"global_threshold_pct"`
	GlobalDeltaPct     float64        `json:"global_delta_pct"`
	FuzzPct            float64        `json:"fuzz_pct"`
	Regions            []RegionResult `json:"regions"`
	Pass               bool           `json:"pass"`
}

func loadPNG(path string) (image.Image, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	img, _, err := image.Decode(f)
	return img, err
}

func channelDelta(a, b color.Color) (float64, color.RGBA) {
	ar, ag, ab, _ := a.RGBA()
	br, bg, bb, _ := b.RGBA()
	dr := math.Abs(float64(int(ar>>8) - int(br>>8)))
	dg := math.Abs(float64(int(ag>>8) - int(bg>>8)))
	db := math.Abs(float64(int(ab>>8) - int(bb>>8)))
	max := dr
	if dg > max {
		max = dg
	}
	if db > max {
		max = db
	}
	return max / 255.0, color.RGBA{R: 255, G: 0, B: 255, A: 255}
}

func diffPixelByPixel(baseline, impl image.Image, fuzz float64) (image.Image, int, int) {
	bx := baseline.Bounds()
	ix := impl.Bounds()
	w := bx.Dx()
	h := bx.Dy()
	diff := image.NewRGBA(image.Rect(0, 0, w, h))
	mismatched := 0
	total := w * h
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			d, hi := channelDelta(baseline.At(bx.Min.X+x, bx.Min.Y+y), impl.At(ix.Min.X+x, ix.Min.Y+y))
			if d > fuzz {
				diff.Set(x, y, hi)
				mismatched++
			} else {
				r, g, b, _ := baseline.At(bx.Min.X+x, bx.Min.Y+y).RGBA()
				diff.Set(x, y, color.RGBA{R: uint8(r >> 10), G: uint8(g >> 10), B: uint8(b >> 10), A: 255})
			}
		}
	}
	return diff, mismatched, total
}

func regionDelta(baseline, impl image.Image, r Region, fuzz float64) float64 {
	bx := baseline.Bounds()
	ix := impl.Bounds()
	rx0 := bx.Min.X + r.X
	ry0 := bx.Min.Y + r.Y
	ix0 := ix.Min.X + r.X
	iy0 := ix.Min.Y + r.Y
	mismatched := 0
	total := r.W * r.H
	for dy := 0; dy < r.H; dy++ {
		for dx := 0; dx < r.W; dx++ {
			d, _ := channelDelta(baseline.At(rx0+dx, ry0+dy), impl.At(ix0+dx, iy0+dy))
			if d > fuzz {
				mismatched++
			}
		}
	}
	if total == 0 {
		return 0
	}
	return 100.0 * float64(mismatched) / float64(total)
}

func savePNG(path string, img image.Image) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()
	return png.Encode(f, img)
}

func loadCeilingConfig(path string) (*CeilingConfig, error) {
	if path == "" {
		return nil, nil
	}
	b, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var c CeilingConfig
	if err := json.Unmarshal(b, &c); err != nil {
		return nil, err
	}
	return &c, nil
}

// scanPriorResults walks the sibling run directories of currentDir's parent and
// returns every result.json this tool itself wrote, most recent first.
//
// Two rules make a directory a prior run of THIS tool, and both are load-bearing:
//
//  1. The result must carry `"tool": "vdiff"`. A result.json written by
//     vcompare — or by any future tool sharing the results tree — is a
//     different schema, not a prior run. Unmarshalling it here yields zeroes
//     for every vdiff field, which the threshold-up guard reads as a prior of
//     0.00 and rejects the current run against. Results predating the marker
//     are also skipped: an unidentifiable file cannot be proven to be ours.
//
//  2. Ordering is by file modification time, not by directory name. Run dirs
//     are named with a timestamp only by convention; an ad-hoc probe directory
//     ("v2b", "scratch") sorts wherever its name falls, and name-sort picked it
//     as "most recent". mtime is a property of the write, not of the name.
func scanPriorResults(currentDir string) ([]string, error) {
	cfgDir := filepath.Dir(currentDir)
	cur := filepath.Base(currentDir)
	entries, err := os.ReadDir(cfgDir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	type cand struct {
		path string
		mod  time.Time
	}
	var cands []cand
	for _, e := range entries {
		if !e.IsDir() || e.Name() == cur {
			continue
		}
		p := filepath.Join(cfgDir, e.Name(), "result.json")
		fi, err := os.Stat(p)
		if err != nil {
			continue
		}
		b, err := os.ReadFile(p)
		if err != nil {
			continue
		}
		var probe struct {
			Tool string `json:"tool"`
		}
		if err := json.Unmarshal(b, &probe); err != nil || probe.Tool != toolMarker {
			continue
		}
		cands = append(cands, cand{p, fi.ModTime()})
	}
	sort.SliceStable(cands, func(i, j int) bool { return cands[i].mod.After(cands[j].mod) })
	paths := make([]string, 0, len(cands))
	for _, c := range cands {
		paths = append(paths, c.path)
	}
	return paths, nil
}

// findPriorResult returns the most recent prior vdiff result.json for the same
// artboard/config, or nil when there is none — the first-run case.
func findPriorResult(outResult string) (*RunResult, string, error) {
	if outResult == "" {
		return nil, "", nil
	}
	abs, err := filepath.Abs(outResult)
	if err != nil {
		return nil, "", err
	}
	paths, err := scanPriorResults(filepath.Dir(abs))
	if err != nil {
		return nil, "", err
	}
	if len(paths) == 0 {
		return nil, "", nil
	}
	priorPath := paths[0]
	b, err := os.ReadFile(priorPath)
	if err != nil {
		return nil, "", err
	}
	var pr RunResult
	if err := json.Unmarshal(b, &pr); err != nil {
		return nil, priorPath, err
	}
	return &pr, priorPath, nil
}

func usage() {
	fmt.Fprintln(os.Stderr, "usage: vdiff -baseline <png> -impl <png> -regions <json> -out-diff <png> -out-result <json> [-config <json>] [-allow-threshold-up]")
	os.Exit(2)
}

func main() {
	baselinePath := flag.String("baseline", "", "baseline PNG path")
	implPath := flag.String("impl", "", "implementation PNG path")
	regionsPath := flag.String("regions", "", "regions JSON path")
	outDiff := flag.String("out-diff", "", "diff PNG output path")
	outResult := flag.String("out-result", "", "result JSON output path")
	configPath := flag.String("config", "", "ceiling config JSON (default: regions-dir/../config.json)")
	allowThresholdUp := flag.Bool("allow-threshold-up", false, "permit thresholds higher than prior run (W-7.22 HALT-fork bypass; never set by make targets)")
	flag.Parse()
	if *baselinePath == "" || *implPath == "" || *regionsPath == "" {
		usage()
	}

	rf, err := os.ReadFile(*regionsPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "regions read: %v\n", err)
		os.Exit(2)
	}
	var regions RegionsFile
	if err := json.Unmarshal(rf, &regions); err != nil {
		fmt.Fprintf(os.Stderr, "regions parse: %v\n", err)
		os.Exit(2)
	}

	// Load ceiling config (defaults to tests/visual/config.json sibling of regions dir).
	cfgPath := *configPath
	if cfgPath == "" {
		cfgPath = filepath.Join(filepath.Dir(*regionsPath), "..", "config.json")
	}
	ceiling, err := loadCeilingConfig(cfgPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "ceiling config: %v\n", err)
		os.Exit(2)
	}

	// Ceiling enforcement — applies regardless of --allow-threshold-up.
	if ceiling != nil {
		if ceiling.GlobalCeilingPct > 0 && regions.GlobalThresholdPct > ceiling.GlobalCeilingPct {
			fmt.Fprintf(os.Stderr, "CEILING VIOLATION: global_threshold_pct %.2f > global_ceiling_pct %.2f — HALT-fork per W-7.22\n",
				regions.GlobalThresholdPct, ceiling.GlobalCeilingPct)
			os.Exit(2)
		}
		if ceiling.RegionCeilingPct > 0 {
			for _, r := range regions.Regions {
				if r.ThresholdPct > ceiling.RegionCeilingPct {
					fmt.Fprintf(os.Stderr, "CEILING VIOLATION: region %s threshold_pct %.2f > region_ceiling_pct %.2f — HALT-fork per W-7.22\n",
						r.Name, r.ThresholdPct, ceiling.RegionCeilingPct)
					os.Exit(2)
				}
			}
		}
	}

	// Threshold-up guard — compares against most recent prior run for the
	// same artboard/config (derived from outResult sibling timestamp dirs).
	if !*allowThresholdUp && *outResult != "" {
		prior, priorPath, err := findPriorResult(*outResult)
		if err != nil {
			fmt.Fprintf(os.Stderr, "prior result lookup: %v\n", err)
			os.Exit(2)
		}
		if prior != nil {
			var ups []string
			if regions.GlobalThresholdPct > prior.GlobalThresholdPct {
				ups = append(ups, fmt.Sprintf("global %.2f -> %.2f", prior.GlobalThresholdPct, regions.GlobalThresholdPct))
			}
			priorMap := map[string]float64{}
			for _, r := range prior.Regions {
				priorMap[r.Name] = r.ThresholdPct
			}
			for _, r := range regions.Regions {
				if pt, ok := priorMap[r.Name]; ok && r.ThresholdPct > pt {
					ups = append(ups, fmt.Sprintf("region %s %.2f -> %.2f", r.Name, pt, r.ThresholdPct))
				}
			}
			if len(ups) > 0 {
				fmt.Fprintf(os.Stderr, "THRESHOLD-UP REJECTED (prior: %s)\n", priorPath)
				for _, u := range ups {
					fmt.Fprintf(os.Stderr, "  %s\n", u)
				}
				fmt.Fprintln(os.Stderr, "threshold-up requires --allow-threshold-up; HALT-fork per W-7.22")
				os.Exit(2)
			}
		}
	}

	fuzz := regions.FuzzPct / 100.0
	if regions.FuzzPct == 0 && ceiling != nil {
		fuzz = ceiling.FuzzPct / 100.0
	}

	baseline, err := loadPNG(*baselinePath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "baseline load: %v\n", err)
		os.Exit(2)
	}
	impl, err := loadPNG(*implPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "impl load: %v\n", err)
		os.Exit(2)
	}

	bb := baseline.Bounds()
	ib := impl.Bounds()
	if bb.Dx() != ib.Dx() || bb.Dy() != ib.Dy() {
		fmt.Fprintf(os.Stderr, "RESOLUTION MISMATCH: baseline %dx%d vs impl %dx%d — native-resolution policy requires identical dimensions; HALT-fork per W-7.22 (S4)\n",
			bb.Dx(), bb.Dy(), ib.Dx(), ib.Dy())
		os.Exit(2)
	}

	diffImg, mismatched, total := diffPixelByPixel(baseline, impl, fuzz)
	globalDelta := 100.0 * float64(mismatched) / float64(total)

	rr := RunResult{
		Tool:               toolMarker,
		Baseline:           *baselinePath,
		Impl:               *implPath,
		Diff:               *outDiff,
		BaselineW:          bb.Dx(),
		BaselineH:          bb.Dy(),
		ImplW:              ib.Dx(),
		ImplH:              ib.Dy(),
		GlobalThresholdPct: regions.GlobalThresholdPct,
		GlobalDeltaPct:     globalDelta,
		FuzzPct:            regions.FuzzPct,
	}
	pass := globalDelta <= regions.GlobalThresholdPct
	for _, r := range regions.Regions {
		dp := regionDelta(baseline, impl, r, fuzz)
		rp := dp <= r.ThresholdPct
		rr.Regions = append(rr.Regions, RegionResult{Name: r.Name, ThresholdPct: r.ThresholdPct, DeltaPct: dp, Pass: rp})
		if !rp {
			pass = false
		}
	}
	sort.SliceStable(rr.Regions, func(i, j int) bool { return rr.Regions[i].DeltaPct > rr.Regions[j].DeltaPct })
	rr.Pass = pass

	if *outDiff != "" {
		if err := savePNG(*outDiff, diffImg); err != nil {
			fmt.Fprintf(os.Stderr, "diff save: %v\n", err)
		}
	}
	if *outResult != "" {
		if err := os.MkdirAll(filepath.Dir(*outResult), 0o755); err != nil {
			fmt.Fprintf(os.Stderr, "result mkdir: %v\n", err)
		}
		b, _ := json.MarshalIndent(rr, "", "  ")
		_ = os.WriteFile(*outResult, b, 0o644)
	}

	fmt.Printf("resolution: baseline %dx%d  impl %dx%d  match=%v\n", bb.Dx(), bb.Dy(), ib.Dx(), ib.Dy(), bb.Dx() == ib.Dx() && bb.Dy() == ib.Dy())
	fmt.Printf("global: %.3f%% (threshold %.3f%%)  fuzz %.1f%%\n", rr.GlobalDeltaPct, rr.GlobalThresholdPct, rr.FuzzPct)
	for _, r := range rr.Regions {
		status := "PASS"
		if !r.Pass {
			status = "FAIL"
		}
		fmt.Printf("  %s  %-40s  %7.3f%% / %5.2f%%\n", status, strings.TrimSpace(r.Name), r.DeltaPct, r.ThresholdPct)
	}
	if pass {
		fmt.Println("RESULT: PASS")
		os.Exit(0)
	}
	fmt.Println("RESULT: FAIL")
	os.Exit(1)
}
