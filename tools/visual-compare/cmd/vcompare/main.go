// vcompare runs SSIM + ΔE_00 + Zhang-Shasha tree edit distance against a
// baseline / impl pair. AND-combined gate: every metric must clear its
// threshold for verdict=pass.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"image"
	_ "image/jpeg"
	_ "image/png"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/eser/q3now/tools/visual-compare/internal/deltae"
	"github.com/eser/q3now/tools/visual-compare/internal/ssim"
	"github.com/eser/q3now/tools/visual-compare/internal/structural"
)

type Region struct {
	Name             string  `json:"name"`
	X                int     `json:"x"`
	Y                int     `json:"y"`
	W                int     `json:"w"`
	H                int     `json:"h"`
	SSIMThreshold    float64 `json:"ssim_threshold,omitempty"`
	DeltaEThreshold  float64 `json:"deltaE2000_threshold,omitempty"`
	StructThreshold  float64 `json:"structural_threshold,omitempty"`
	// Gating: when false, the region is measured and recorded but does NOT
	// contribute to the pass/fail verdict. Absent (nil) defaults to gating.
	Gating           *bool   `json:"gating,omitempty"`
}

type RegionsFile struct {
	SSIMThresholdGlobal    float64  `json:"ssim_threshold_global,omitempty"`
	DeltaEThresholdGlobal  float64  `json:"deltaE2000_threshold_global,omitempty"`
	StructThresholdGlobal  float64  `json:"structural_threshold_global,omitempty"`
	// GatingRegionsOnly: when true, the global metrics are recorded but not
	// gated, and the verdict folds only gating:true regions.
	GatingRegionsOnly      bool     `json:"gating_regions_only,omitempty"`
	Regions                []Region `json:"regions"`
}

type Config struct {
	// vdiff legacy fields, ignored by vcompare:
	GlobalCeilingPct  float64 `json:"global_ceiling_pct,omitempty"`
	RegionCeilingPct  float64 `json:"region_ceiling_pct,omitempty"`
	GlobalStartingPct float64 `json:"global_starting_pct,omitempty"`
	RegionStartingPct float64 `json:"region_starting_pct,omitempty"`
	FuzzPct           float64 `json:"fuzz_pct,omitempty"`

	// vcompare section:
	SSIMStartingGlobal    float64 `json:"ssim_threshold_global,omitempty"`
	SSIMStartingRegion    float64 `json:"ssim_threshold_region_default,omitempty"`
	DeltaEStartingGlobal  float64 `json:"deltaE2000_threshold_global,omitempty"`
	DeltaEStartingRegion  float64 `json:"deltaE2000_threshold_region_default,omitempty"`
	StructStartingGlobal  float64 `json:"structural_threshold_global,omitempty"`
	StructStartingRegion  float64 `json:"structural_threshold_region_default,omitempty"`

	SSIMCeiling   float64 `json:"ssim_ceiling,omitempty"`
	DeltaECeiling float64 `json:"deltaE2000_ceiling,omitempty"`
	StructCeiling float64 `json:"structural_ceiling,omitempty"`
}

type MetricResult struct {
	Name      string  `json:"name"`
	Value     float64 `json:"value"`
	Threshold float64 `json:"threshold"`
	Verdict   string  `json:"verdict"`
}

type SectionResult struct {
	Global  MetricResult            `json:"global"`
	Regions map[string]MetricResult `json:"regions"`
}

type RunResult struct {
	Config struct {
		Artboard string `json:"artboard"`
		Mode     string `json:"mode"`
		Accent   string `json:"accent"`
	} `json:"config"`
	Baseline   string         `json:"baseline"`
	Impl       string         `json:"impl"`
	BaselineW  int            `json:"baseline_w"`
	BaselineH  int            `json:"baseline_h"`
	ImplW      int            `json:"impl_w"`
	ImplH      int            `json:"impl_h"`
	SSIM       SectionResult  `json:"ssim"`
	DeltaE2000 SectionResult  `json:"deltaE2000"`
	Structural SectionResult  `json:"structural"`
	Verdict    string         `json:"verdict"`
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

func loadConfig(path string) (*Config, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var c Config
	if err := json.Unmarshal(b, &c); err != nil {
		return nil, err
	}
	return &c, nil
}

func ceiling(c *Config, key string) float64 {
	if c == nil {
		return 0
	}
	switch key {
	case "ssim":
		return c.SSIMCeiling
	case "deltae":
		return c.DeltaECeiling
	case "structural":
		return c.StructCeiling
	}
	return 0
}

// applyDefault picks region's per-metric threshold; falls back to config
// starting-default when region's override is unset (== 0).
func applyDefault(regionVal, configDefault float64) float64 {
	if regionVal > 0 {
		return regionVal
	}
	return configDefault
}

// enforceCeilings rejects regions JSON values that would weaken the gate
// past the W-7.22 ceiling. SSIM is "higher is stricter" so the ceiling is a
// floor: any threshold BELOW SSIMCeiling is a violation. deltaE / structural
// are "lower is stricter" so the ceiling is a max: any threshold ABOVE is a
// violation.
func enforceCeilings(rf RegionsFile, cfg *Config) error {
	if cfg == nil {
		return nil
	}
	if cfg.SSIMCeiling > 0 {
		if rf.SSIMThresholdGlobal > 0 && rf.SSIMThresholdGlobal < cfg.SSIMCeiling {
			return fmt.Errorf("CEILING VIOLATION: ssim_threshold_global %.4f < ssim_ceiling %.4f — HALT-fork per W-7.22",
				rf.SSIMThresholdGlobal, cfg.SSIMCeiling)
		}
		for _, r := range rf.Regions {
			if r.SSIMThreshold > 0 && r.SSIMThreshold < cfg.SSIMCeiling {
				return fmt.Errorf("CEILING VIOLATION: region %s ssim_threshold %.4f < ssim_ceiling %.4f — HALT-fork per W-7.22",
					r.Name, r.SSIMThreshold, cfg.SSIMCeiling)
			}
		}
	}
	if cfg.DeltaECeiling > 0 {
		if rf.DeltaEThresholdGlobal > cfg.DeltaECeiling {
			return fmt.Errorf("CEILING VIOLATION: deltaE2000_threshold_global %.3f > deltaE2000_ceiling %.3f — HALT-fork per W-7.22",
				rf.DeltaEThresholdGlobal, cfg.DeltaECeiling)
		}
		for _, r := range rf.Regions {
			if r.DeltaEThreshold > cfg.DeltaECeiling {
				return fmt.Errorf("CEILING VIOLATION: region %s deltaE2000_threshold %.3f > deltaE2000_ceiling %.3f — HALT-fork per W-7.22",
					r.Name, r.DeltaEThreshold, cfg.DeltaECeiling)
			}
		}
	}
	if cfg.StructCeiling > 0 {
		if rf.StructThresholdGlobal > cfg.StructCeiling {
			return fmt.Errorf("CEILING VIOLATION: structural_threshold_global %.4f > structural_ceiling %.4f — HALT-fork per W-7.22",
				rf.StructThresholdGlobal, cfg.StructCeiling)
		}
		for _, r := range rf.Regions {
			if r.StructThreshold > cfg.StructCeiling {
				return fmt.Errorf("CEILING VIOLATION: region %s structural_threshold %.4f > structural_ceiling %.4f — HALT-fork per W-7.22",
					r.Name, r.StructThreshold, cfg.StructCeiling)
			}
		}
	}
	return nil
}

// findPriorResult walks sibling timestamp directories of resultDir's parent
// for the most recent prior result.json.
func findPriorResult(resultDir string) (*RunResult, string, error) {
	abs, err := filepath.Abs(resultDir)
	if err != nil {
		return nil, "", err
	}
	cfgDir := filepath.Dir(abs)
	cur := filepath.Base(abs)
	entries, err := os.ReadDir(cfgDir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, "", nil
		}
		return nil, "", err
	}
	var candidates []string
	for _, e := range entries {
		if !e.IsDir() || e.Name() == cur {
			continue
		}
		p := filepath.Join(cfgDir, e.Name(), "result.json")
		if _, err := os.Stat(p); err == nil {
			candidates = append(candidates, p)
		}
	}
	if len(candidates) == 0 {
		return nil, "", nil
	}
	sort.Strings(candidates)
	priorPath := candidates[len(candidates)-1]
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

// enforceThresholdUp checks that no metric+region threshold has been loosened
// versus the most recent prior result. SSIM "up" means lower (less strict);
// ΔE / structural "up" means higher (less strict).
func enforceThresholdUp(prior *RunResult, rf RegionsFile, cfg *Config) []string {
	if prior == nil {
		return nil
	}
	var ups []string
	cmp := func(metric, scope, name string, newVal, oldVal float64, strictHigher bool) {
		if oldVal == 0 || newVal == 0 {
			return
		}
		if strictHigher && newVal < oldVal {
			ups = append(ups, fmt.Sprintf("%s %s %s %.4f -> %.4f", metric, scope, name, oldVal, newVal))
		}
		if !strictHigher && newVal > oldVal {
			ups = append(ups, fmt.Sprintf("%s %s %s %.4f -> %.4f", metric, scope, name, oldVal, newVal))
		}
	}
	newSSIMG := applyDefault(rf.SSIMThresholdGlobal, cfg.SSIMStartingGlobal)
	newDEG := applyDefault(rf.DeltaEThresholdGlobal, cfg.DeltaEStartingGlobal)
	newSTG := applyDefault(rf.StructThresholdGlobal, cfg.StructStartingGlobal)
	cmp("ssim", "global", "", newSSIMG, prior.SSIM.Global.Threshold, true)
	cmp("deltaE2000", "global", "", newDEG, prior.DeltaE2000.Global.Threshold, false)
	cmp("structural", "global", "", newSTG, prior.Structural.Global.Threshold, false)

	for _, r := range rf.Regions {
		newSSIM := applyDefault(r.SSIMThreshold, cfg.SSIMStartingRegion)
		newDE := applyDefault(r.DeltaEThreshold, cfg.DeltaEStartingRegion)
		newST := applyDefault(r.StructThreshold, cfg.StructStartingRegion)
		if pr, ok := prior.SSIM.Regions[r.Name]; ok {
			cmp("ssim", "region", r.Name, newSSIM, pr.Threshold, true)
		}
		if pr, ok := prior.DeltaE2000.Regions[r.Name]; ok {
			cmp("deltaE2000", "region", r.Name, newDE, pr.Threshold, false)
		}
		if pr, ok := prior.Structural.Regions[r.Name]; ok {
			cmp("structural", "region", r.Name, newST, pr.Threshold, false)
		}
	}
	return ups
}

func verdictSSIM(value, threshold float64) string {
	if value >= threshold {
		return "pass"
	}
	return "fail"
}
func verdictLowerStricter(value, threshold float64) string {
	if value <= threshold {
		return "pass"
	}
	return "fail"
}

func usage() {
	fmt.Fprintln(os.Stderr, "usage: vcompare --baseline <png> --impl <png> --regions <json> --result-dir <dir> [--baseline-tree <json>] [--impl-tree <json>] [--config <json>] [--artboard NAME] [--mode NAME] [--accent NAME] [--allow-threshold-up]")
	os.Exit(2)
}

func main() {
	baselinePath := flag.String("baseline", "", "baseline PNG path")
	implPath := flag.String("impl", "", "implementation PNG path")
	regionsPath := flag.String("regions", "", "regions JSON path")
	resultDir := flag.String("result-dir", "", "directory to receive result.json")
	baselineTreePath := flag.String("baseline-tree", "", "DOM JSON path (Clay-mirror schema)")
	implTreePath := flag.String("impl-tree", "", "Clay JSON path")
	configPath := flag.String("config", "", "config JSON; defaults to <regions-dir>/../config.json")
	artboard := flag.String("artboard", "", "artboard name (recorded in result.json)")
	mode := flag.String("mode", "", "palette mode (recorded)")
	accent := flag.String("accent", "", "palette accent (recorded)")
	allowThresholdUp := flag.Bool("allow-threshold-up", false, "permit thresholds weaker than prior run (W-7.22 HALT-fork bypass; never set by make targets)")
	flag.Parse()

	if *baselinePath == "" || *implPath == "" || *regionsPath == "" || *resultDir == "" {
		usage()
	}

	rfBytes, err := os.ReadFile(*regionsPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "regions read: %v\n", err)
		os.Exit(2)
	}
	var rf RegionsFile
	if err := json.Unmarshal(rfBytes, &rf); err != nil {
		fmt.Fprintf(os.Stderr, "regions parse: %v\n", err)
		os.Exit(2)
	}

	cfgPath := *configPath
	if cfgPath == "" {
		cfgPath = filepath.Join(filepath.Dir(*regionsPath), "..", "config.json")
	}
	cfg, err := loadConfig(cfgPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "config load: %v\n", err)
		os.Exit(2)
	}
	if cfg == nil {
		fmt.Fprintf(os.Stderr, "config missing at %s — HALT-fork per W-7.22 (vcompare requires config.json for ceilings)\n", cfgPath)
		os.Exit(2)
	}

	if err := enforceCeilings(rf, cfg); err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(2)
	}

	if !*allowThresholdUp {
		prior, priorPath, err := findPriorResult(*resultDir)
		if err != nil {
			fmt.Fprintf(os.Stderr, "prior result lookup: %v\n", err)
			os.Exit(2)
		}
		if ups := enforceThresholdUp(prior, rf, cfg); len(ups) > 0 {
			fmt.Fprintf(os.Stderr, "THRESHOLD-UP REJECTED (prior: %s)\n", priorPath)
			for _, u := range ups {
				fmt.Fprintf(os.Stderr, "  %s\n", u)
			}
			fmt.Fprintln(os.Stderr, "threshold-up requires --allow-threshold-up; HALT-fork per W-7.22")
			os.Exit(2)
		}
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
		fmt.Fprintf(os.Stderr, "RESOLUTION MISMATCH: baseline %dx%d vs impl %dx%d — native-resolution policy requires identical dimensions; HALT-fork per W-7.22\n",
			bb.Dx(), bb.Dy(), ib.Dx(), ib.Dy())
		os.Exit(2)
	}

	// Optional structural trees.  Both required for structural metric; if
	// only one is present the structural section reports HALT per W-7.22.
	var baselineTree, implTree *structural.Node
	structuralAvailable := *baselineTreePath != "" && *implTreePath != ""
	if structuralAvailable {
		baselineTree, err = structural.Load(*baselineTreePath)
		if err != nil {
			fmt.Fprintf(os.Stderr, "baseline tree load: %v\n", err)
			os.Exit(2)
		}
		implTree, err = structural.Load(*implTreePath)
		if err != nil {
			fmt.Fprintf(os.Stderr, "impl tree load: %v\n", err)
			os.Exit(2)
		}
	} else if *baselineTreePath != "" || *implTreePath != "" {
		fmt.Fprintln(os.Stderr, "STRUCTURAL INPUT MISMATCH: provide both --baseline-tree and --impl-tree, or neither — HALT-fork per W-7.22")
		os.Exit(2)
	}

	// Run metrics.
	ssimGlobal := ssim.Global(baseline, impl)
	deltaEGlobal := deltae.Global(baseline, impl)
	structGlobal := 0.0
	if structuralAvailable {
		structGlobal = structural.EditDistance(baselineTree, implTree)
	}

	rr := RunResult{
		Baseline:  *baselinePath,
		Impl:      *implPath,
		BaselineW: bb.Dx(), BaselineH: bb.Dy(),
		ImplW: ib.Dx(), ImplH: ib.Dy(),
	}
	rr.Config.Artboard = *artboard
	rr.Config.Mode = *mode
	rr.Config.Accent = *accent

	ssimGT := applyDefault(rf.SSIMThresholdGlobal, cfg.SSIMStartingGlobal)
	deGT := applyDefault(rf.DeltaEThresholdGlobal, cfg.DeltaEStartingGlobal)
	stGT := applyDefault(rf.StructThresholdGlobal, cfg.StructStartingGlobal)

	rr.SSIM = SectionResult{
		Global:  MetricResult{Name: "global", Value: ssimGlobal, Threshold: ssimGT, Verdict: verdictSSIM(ssimGlobal, ssimGT)},
		Regions: map[string]MetricResult{},
	}
	rr.DeltaE2000 = SectionResult{
		Global:  MetricResult{Name: "global", Value: deltaEGlobal, Threshold: deGT, Verdict: verdictLowerStricter(deltaEGlobal, deGT)},
		Regions: map[string]MetricResult{},
	}
	rr.Structural = SectionResult{
		Global:  MetricResult{Name: "global", Value: structGlobal, Threshold: stGT, Verdict: structuralVerdict(structuralAvailable, structGlobal, stGT)},
		Regions: map[string]MetricResult{},
	}

	allPass := rr.SSIM.Global.Verdict == "pass" &&
		rr.DeltaE2000.Global.Verdict == "pass" &&
		(rr.Structural.Global.Verdict == "pass" || rr.Structural.Global.Verdict == "skip")

	// Gating-regions-only verdict: the global metrics are BG-confounded over
	// the live arena, so they are recorded but not gated; the verdict folds
	// only gating:true regions (e.g. the HUD elements, excluding BG/full-frame).
	if rf.GatingRegionsOnly {
		allPass = true
	}

	for _, r := range rf.Regions {
		sT := applyDefault(r.SSIMThreshold, cfg.SSIMStartingRegion)
		dT := applyDefault(r.DeltaEThreshold, cfg.DeltaEStartingRegion)
		tT := applyDefault(r.StructThreshold, cfg.StructStartingRegion)

		sV := ssim.Compute(baseline, impl, r.X, r.Y, r.W, r.H)
		dV := deltae.Compute(baseline, impl, r.X, r.Y, r.W, r.H)
		var tV float64
		var tVerdict string
		if structuralAvailable {
			bFilt := structural.Filter(baselineTree, float64(r.X), float64(r.Y), float64(r.W), float64(r.H))
			iFilt := structural.Filter(implTree, float64(r.X), float64(r.Y), float64(r.W), float64(r.H))
			tV = structural.EditDistance(bFilt, iFilt)
			tVerdict = verdictLowerStricter(tV, tT)
		} else {
			tVerdict = "skip"
		}

		rr.SSIM.Regions[r.Name] = MetricResult{Name: r.Name, Value: sV, Threshold: sT, Verdict: verdictSSIM(sV, sT)}
		rr.DeltaE2000.Regions[r.Name] = MetricResult{Name: r.Name, Value: dV, Threshold: dT, Verdict: verdictLowerStricter(dV, dT)}
		rr.Structural.Regions[r.Name] = MetricResult{Name: r.Name, Value: tV, Threshold: tT, Verdict: tVerdict}

		gating := r.Gating == nil || *r.Gating
		if gating && (rr.SSIM.Regions[r.Name].Verdict == "fail" ||
			rr.DeltaE2000.Regions[r.Name].Verdict == "fail" ||
			rr.Structural.Regions[r.Name].Verdict == "fail") {
			allPass = false
		}
	}

	if allPass {
		rr.Verdict = "pass"
	} else {
		rr.Verdict = "fail"
	}

	if err := os.MkdirAll(*resultDir, 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "result mkdir: %v\n", err)
		os.Exit(2)
	}
	out := filepath.Join(*resultDir, "result.json")
	b, _ := json.MarshalIndent(rr, "", "  ")
	if err := os.WriteFile(out, b, 0o644); err != nil {
		fmt.Fprintf(os.Stderr, "result write: %v\n", err)
		os.Exit(2)
	}

	fmt.Printf("resolution: baseline %dx%d  impl %dx%d  match=%v\n", bb.Dx(), bb.Dy(), ib.Dx(), ib.Dy(), bb.Dx() == ib.Dx() && bb.Dy() == ib.Dy())
	fmt.Printf("ssim       global %.4f (>=%.4f)  %s\n", rr.SSIM.Global.Value, rr.SSIM.Global.Threshold, strings.ToUpper(rr.SSIM.Global.Verdict))
	fmt.Printf("deltaE2000 global %.3f  (<=%.3f) %s\n", rr.DeltaE2000.Global.Value, rr.DeltaE2000.Global.Threshold, strings.ToUpper(rr.DeltaE2000.Global.Verdict))
	if structuralAvailable {
		fmt.Printf("structural global %.4f (<=%.4f)  %s\n", rr.Structural.Global.Value, rr.Structural.Global.Threshold, strings.ToUpper(rr.Structural.Global.Verdict))
	} else {
		fmt.Printf("structural global SKIP (no trees provided)\n")
	}

	printRegionTable("ssim", rr.SSIM.Regions, true)
	printRegionTable("deltaE2000", rr.DeltaE2000.Regions, false)
	if structuralAvailable {
		printRegionTable("structural", rr.Structural.Regions, false)
	}

	if rf.GatingRegionsOnly {
		var nonGating []string
		for _, r := range rf.Regions {
			if r.Gating != nil && !*r.Gating {
				nonGating = append(nonGating, r.Name)
			}
		}
		fmt.Printf("verdict scope: gating-regions-only (global metrics informational); non-gating: %v\n", nonGating)
	}

	if rr.Verdict == "pass" {
		fmt.Println("RESULT: PASS")
		os.Exit(0)
	}
	fmt.Println("RESULT: FAIL")
	os.Exit(1)
}

func structuralVerdict(avail bool, value, threshold float64) string {
	if !avail {
		return "skip"
	}
	return verdictLowerStricter(value, threshold)
}

func printRegionTable(metric string, regions map[string]MetricResult, strictHigher bool) {
	type row struct {
		name string
		r    MetricResult
	}
	var rows []row
	for n, r := range regions {
		rows = append(rows, row{n, r})
	}
	sort.Slice(rows, func(i, j int) bool {
		if strictHigher {
			return rows[i].r.Value < rows[j].r.Value
		}
		return rows[i].r.Value > rows[j].r.Value
	})
	for _, rw := range rows {
		fmt.Printf("  %-10s  %-4s  %-32s  %8.4f / %8.4f\n",
			metric, strings.ToUpper(rw.r.Verdict), rw.name, rw.r.Value, rw.r.Threshold)
	}
}
