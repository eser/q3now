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
	"time"

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
	// AllowEmpty exempts a gating region from the ink check below. It exists
	// for regions whose widget is correct but whose CONTENT is conditional on
	// game state — the weapon carousel is a 1650ms post-switch transient, the
	// holdables strip renders only owned holdables — so an empty render is the
	// right render at a no-combat spawn. Setting it is a claim about the
	// widget, so it requires `empty_reason` to say which state leaves it empty.
	AllowEmpty       bool    `json:"allow_empty,omitempty"`
	EmptyReason      string  `json:"empty_reason,omitempty"`
}

type RegionsFile struct {
	SSIMThresholdGlobal    float64  `json:"ssim_threshold_global,omitempty"`
	DeltaEThresholdGlobal  float64  `json:"deltaE2000_threshold_global,omitempty"`
	StructThresholdGlobal  float64  `json:"structural_threshold_global,omitempty"`
	// GatingRegionsOnly: when true, the global metrics are recorded but not
	// gated, and the verdict folds only gating:true regions.
	GatingRegionsOnly      bool     `json:"gating_regions_only,omitempty"`
	// PixelMetricsGated: whether SSIM / ΔE_00 / structural contribute to the
	// verdict at all. Default (absent) is true — every existing artboard keeps
	// its behaviour. Setting it false records the metrics as audit-trail
	// numbers while the verdict rests on the ink check alone, for the case
	// where the baseline and the impl are not the same KIND of image: a flat
	// vector artboard versus a live 3D scene. Measured on V2_HUD_Active,
	// regions containing NO HUD on either side score HIGHER (SSIM 0.41-0.63)
	// than correctly-anchored, ink-verified HUD regions (0.17-0.28) — the
	// metric rewards absence, so no threshold separates a regression from the
	// background disagreement. Turning it off is a deliberate, recorded choice;
	// `pixel_metrics_ungated_reason` is required so it can never be a silent one.
	PixelMetricsGated      *bool    `json:"pixel_metrics_gated,omitempty"`
	PixelMetricsUngatedWhy string   `json:"pixel_metrics_ungated_reason,omitempty"`
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

// toolMarker identifies which tool wrote a result.json. vcompare and vdiff
// write into the SAME results/<artboard>_<mode>_<accent>/ tree with disjoint
// schemas. Reading a vdiff result.json here yields zero thresholds for every
// metric, which enforceThresholdUp then skips (its `oldVal == 0` short-circuit)
// — a silently DISARMED loosen guard rather than a loud one. Requiring the
// marker makes a prior run mean a prior run of this tool.
const toolMarker = "vcompare"

type RunResult struct {
	Tool   string `json:"tool"`
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
	// Ink records, per region, the share of pixels the HUD actually drew —
	// measured against the HUD-off frame when one is supplied. A gating region
	// below the floor is an authoring error, not a comparison result, so it is
	// reported separately from the metrics and forces verdict=error.
	Ink        *SectionResult `json:"ink,omitempty"`
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

// scanPriorResults walks the sibling run directories of currentDir's parent and
// returns every result.json this tool itself wrote, most recent first.
//
// Two rules make a directory a prior run of THIS tool, and both are load-bearing:
//
//  1. The result must carry `"tool": "vcompare"`. A vdiff result.json is a
//     different schema, not a prior run; unmarshalling it here produces zero
//     thresholds for every metric, which enforceThresholdUp skips outright,
//     leaving the loosen guard silently disarmed. Results predating the marker
//     are skipped for the same reason — an unidentifiable file cannot be proven
//     to be ours.
//
//  2. Ordering is by file modification time, not by directory name. Run dirs
//     are named with a timestamp only by convention; an ad-hoc probe directory
//     sorts wherever its name falls, and name-sort picked it as "most recent".
//     mtime is a property of the write, not of the name.
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

// findPriorResult returns the most recent prior vcompare result.json for the
// same artboard/config, or nil when there is none — the first-run case.
func findPriorResult(resultDir string) (*RunResult, string, error) {
	abs, err := filepath.Abs(resultDir)
	if err != nil {
		return nil, "", err
	}
	paths, err := scanPriorResults(abs)
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

// inkTolerance is the per-channel 8-bit delta above which a pixel counts as
// drawn by the HUD. The two frames come from ONE engine session at one pinned
// viewpoint, so the arena behind the HUD is bit-identical between them and the
// measured floor in a genuinely empty region is 0.000%. 8 is well clear of that
// floor while ignoring any residual dithering.
const inkTolerance = 8

// inkFloorPct is the minimum share of a gating region's pixels that the HUD
// must actually draw. Measured contrast on V2_HUD_Active at 1440x900: regions
// with real content score 39-87%; regions over bare background score 0.000%.
// 0.5% sits between them by two orders of magnitude, so this discriminates
// "the widget drew here" from "nothing drew here" without being a fidelity
// threshold — it is a liveness check, not a quality one.
const inkFloorPct = 0.5

// maxFrameInkPct caps how much of the WHOLE frame may differ between the
// HUD-on and HUD-off captures. The HUD is a small overlay: measured across the
// five V2_HUD_Active palette configs a valid pair differs by ~2.4% of the frame.
// A pair differing by much more is not a HUD-on/HUD-off pair of one scene — it
// is two different scenes, and every per-region ink reading taken from it is
// meaningless.
//
// This is not hypothetical. A run whose engine hung before the map loaded
// produced a MENU screenshot paired with a HUD frame; every gating region then
// measured 60-100% "ink" and the config was recorded PASS on a comparison
// between the main menu and an arena. Proving each region has ink is not enough
// on its own — the two frames must also be the same scene.
const maxFrameInkPct = 25.0

// regionInkPct returns the percentage of the region's pixels where the HUD-on
// frame differs from the HUD-off frame. Because both frames are the same scene
// from the same pinned viewpoint, every differing pixel was drawn by the HUD.
func regionInkPct(on, off image.Image, x, y, w, h int) float64 {
	b := on.Bounds()
	ob := off.Bounds()
	total, hits := 0, 0
	for dy := 0; dy < h; dy++ {
		for dx := 0; dx < w; dx++ {
			px, py := b.Min.X+x+dx, b.Min.Y+y+dy
			qx, qy := ob.Min.X+x+dx, ob.Min.Y+y+dy
			if px >= b.Max.X || py >= b.Max.Y || qx >= ob.Max.X || qy >= ob.Max.Y {
				continue
			}
			total++
			ar, ag, ab, _ := on.At(px, py).RGBA()
			br, bg, bb, _ := off.At(qx, qy).RGBA()
			d := func(u, v uint32) int {
				n := int(u>>8) - int(v>>8)
				if n < 0 {
					return -n
				}
				return n
			}
			m := d(ar, br)
			if n := d(ag, bg); n > m {
				m = n
			}
			if n := d(ab, bb); n > m {
				m = n
			}
			if m > inkTolerance {
				hits++
			}
		}
	}
	if total == 0 {
		return 0
	}
	return 100 * float64(hits) / float64(total)
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
	implNoHudPath := flag.String("impl-nohud", "", "implementation PNG captured with the HUD suppressed, same viewpoint; enables the per-region ink check")
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
	// HUD-off reference frame: the same scene from the same pinned viewpoint
	// with the HUD suppressed. Differencing against it isolates the pixels the
	// HUD drew, which is the only way this tool — seeing PNGs, not draw calls —
	// can tell "the widget rendered here" from "nothing rendered here".
	var implNoHud image.Image
	if *implNoHudPath != "" {
		implNoHud, err = loadPNG(*implNoHudPath)
		if err != nil {
			fmt.Fprintf(os.Stderr, "impl-nohud load: %v\n", err)
			os.Exit(2)
		}
	}

	bb := baseline.Bounds()
	ib := impl.Bounds()
	if bb.Dx() != ib.Dx() || bb.Dy() != ib.Dy() {
		fmt.Fprintf(os.Stderr, "RESOLUTION MISMATCH: baseline %dx%d vs impl %dx%d — native-resolution policy requires identical dimensions; HALT-fork per W-7.22\n",
			bb.Dx(), bb.Dy(), ib.Dx(), ib.Dy())
		os.Exit(2)
	}
	if implNoHud != nil {
		nb := implNoHud.Bounds()
		if nb.Dx() != ib.Dx() || nb.Dy() != ib.Dy() {
			fmt.Fprintf(os.Stderr, "RESOLUTION MISMATCH: impl %dx%d vs impl-nohud %dx%d — the ink check differences the two frame-for-frame; HALT-fork per W-7.22\n",
				ib.Dx(), ib.Dy(), nb.Dx(), nb.Dy())
			os.Exit(2)
		}
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
		Tool:      toolMarker,
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

	// Absent means gated — every artboard authored before this flag existed
	// keeps exactly its previous behaviour.
	pixelMetricsGated := rf.PixelMetricsGated == nil || *rf.PixelMetricsGated
	if !pixelMetricsGated && strings.TrimSpace(rf.PixelMetricsUngatedWhy) == "" {
		fmt.Fprintln(os.Stderr, "pixel_metrics_gated:false requires pixel_metrics_ungated_reason — ungating the perceptual metrics must be a recorded decision, not a silent one; HALT-fork per W-7.22")
		os.Exit(2)
	}

	allPass := rr.SSIM.Global.Verdict == "pass" &&
		rr.DeltaE2000.Global.Verdict == "pass" &&
		(rr.Structural.Global.Verdict == "pass" || rr.Structural.Global.Verdict == "skip")
	if !pixelMetricsGated {
		allPass = true
	}

	// Gating-regions-only verdict: the global metrics are BG-confounded over
	// the live arena, so they are recorded but not gated; the verdict folds
	// only gating:true regions (e.g. the HUD elements, excluding BG/full-frame).
	if rf.GatingRegionsOnly {
		allPass = true
	}

	if implNoHud != nil {
		frameInk := regionInkPct(impl, implNoHud, 0, 0, ib.Dx(), ib.Dy())
		if frameInk > maxFrameInkPct {
			fmt.Fprintf(os.Stderr, "CAPTURE PAIR MISMATCH: the HUD-on and HUD-off frames differ over %.2f%% of the frame (max %.2f%%).\n",
				frameInk, maxFrameInkPct)
			fmt.Fprintln(os.Stderr, "A HUD-on/HUD-off pair of ONE scene differs only by the HUD overlay (~2.4% measured).")
			fmt.Fprintln(os.Stderr, "This pair is two different scenes — typically the engine failed to load the map and")
			fmt.Fprintln(os.Stderr, "screenshotted the menu. Every per-region ink reading from it is meaningless; HALT-fork per W-7.22")
			os.Exit(2)
		}
		rr.Ink = &SectionResult{
			Global:  MetricResult{Name: "global", Value: frameInk, Threshold: maxFrameInkPct, Verdict: "info"},
			Regions: map[string]MetricResult{},
		}
	}
	// With the perceptual metrics ungated, the ink check is the ONLY thing left
	// gating the verdict. Running in that combination without a HUD-off frame
	// would report PASS while checking nothing whatsoever — a gate in name only.
	if !pixelMetricsGated && rr.Ink == nil {
		fmt.Fprintln(os.Stderr, "pixel_metrics_gated:false requires --impl-nohud: with the perceptual metrics ungated the ink check is the only remaining gate, and without the HUD-off frame there would be nothing left to check; HALT-fork per W-7.22")
		os.Exit(2)
	}

	// inkErrors collects gating regions that measure no engine ink. They are
	// authoring faults — the region points somewhere the HUD does not draw, or
	// at a widget that is not shipped — so they are fatal regardless of what
	// SSIM says. A region that scores well because BOTH sides are empty is a
	// false green, which is worse than a red.
	var inkErrors []string

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

		if rr.Ink != nil {
			ink := regionInkPct(impl, implNoHud, r.X, r.Y, r.W, r.H)
			iVerdict := "info"
			if gating {
				switch {
				case ink >= inkFloorPct:
					iVerdict = "pass"
				case r.AllowEmpty:
					// Declared conditional-content region: empty here is
					// correct, so it does not fail — but it is not silently a
					// pass either, and the reason travels in the result.
					iVerdict = "empty-allowed"
				default:
					iVerdict = "error"
					inkErrors = append(inkErrors, fmt.Sprintf(
						"%s: %.3f%% ink (floor %.2f%%) — the engine draws nothing in this rect, so its score compares background against background",
						r.Name, ink, inkFloorPct))
				}
			}
			rr.Ink.Regions[r.Name] = MetricResult{Name: r.Name, Value: ink, Threshold: inkFloorPct, Verdict: iVerdict}
		}

		if pixelMetricsGated && gating &&
			(rr.SSIM.Regions[r.Name].Verdict == "fail" ||
				rr.DeltaE2000.Regions[r.Name].Verdict == "fail" ||
				rr.Structural.Regions[r.Name].Verdict == "fail") {
			allPass = false
		}
	}

	// A region declaring allow_empty is making a claim about the widget, so it
	// must say which game state leaves it empty. An undocumented exemption is
	// how a false green comes back.
	for _, r := range rf.Regions {
		if r.AllowEmpty && strings.TrimSpace(r.EmptyReason) == "" {
			inkErrors = append(inkErrors, fmt.Sprintf(
				"%s: allow_empty set without empty_reason — an exemption must record which game state leaves the region empty", r.Name))
		}
	}

	switch {
	case len(inkErrors) > 0:
		// Distinct from "fail": the gate did not measure a regression, it
		// found that it was never in a position to measure one.
		rr.Verdict = "error"
	case allPass:
		rr.Verdict = "pass"
	default:
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
	if rr.Ink != nil {
		fmt.Printf("ink        global %.3f%% (HUD pixels vs the HUD-off frame)\n", rr.Ink.Global.Value)
		printRegionTable("ink", rr.Ink.Regions, true)
	} else {
		fmt.Printf("ink        SKIP (no --impl-nohud frame; region liveness UNVERIFIED)\n")
	}

	if !pixelMetricsGated {
		fmt.Printf("metric scope: ssim/deltaE2000/structural RECORDED BUT NOT GATED — %s\n", strings.TrimSpace(rf.PixelMetricsUngatedWhy))
		fmt.Printf("              the verdict rests on the per-region ink check\n")
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

	if len(inkErrors) > 0 {
		fmt.Fprintln(os.Stderr, "EMPTY GATING REGION(S) — a region that scores well because both sides are empty is a false green:")
		for _, e := range inkErrors {
			fmt.Fprintf(os.Stderr, "  %s\n", e)
		}
		fmt.Fprintln(os.Stderr, "Re-anchor the region to where the engine draws, mark it gating:false with a reason,")
		fmt.Fprintln(os.Stderr, "or set allow_empty + empty_reason if the widget is correct and its content is state-conditional.")
		fmt.Println("RESULT: ERROR")
		os.Exit(2)
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
