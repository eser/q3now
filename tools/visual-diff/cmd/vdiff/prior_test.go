package main

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

// writeRun materialises results/<cfg>/<name>/result.json with the given body
// and mtime, mirroring the layout compare.sh / vcompare_run.sh produce.
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

const vdiffResult = `{"tool":"vdiff","global_threshold_pct":5.0,"regions":[{"name":"r","threshold_pct":10.0}]}`

// A vcompare result.json shares the directory tree but not one field with
// vdiff's schema. Unmarshalled into RunResult it yields GlobalThresholdPct 0.
const vcompareResult = `{"tool":"vcompare","ssim":{"global":{"threshold":0.5}},"verdict":"pass"}`

// The original defect: an ad-hoc probe directory whose result.json was written
// by vcompare was selected as vdiff's "most recent prior run" purely because
// its NAME sorted last. Every vdiff threshold then compared against 0, so any
// real threshold read as a loosening and the run HALTed (exit 2) before
// comparing a pixel.
func TestForeignToolResultIsNotAPriorRun(t *testing.T) {
	cfgDir := t.TempDir()
	base := time.Now().Add(-time.Hour)
	writeRun(t, cfgDir, "20260101_000000", vdiffResult, base)
	// "v2b" sorts after "20260101_000000" and is written LATER, so neither
	// name-sort nor mtime alone rejects it — only the tool marker does.
	writeRun(t, cfgDir, "v2b", vcompareResult, base.Add(30*time.Minute))

	cur := filepath.Join(cfgDir, "20260102_000000")
	prior, path, err := findPriorResult(filepath.Join(cur, "result.json"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior == nil {
		t.Fatal("expected the genuine vdiff prior to be found")
	}
	if filepath.Base(filepath.Dir(path)) != "20260101_000000" {
		t.Fatalf("selected %s; expected the vdiff run, not the vcompare one", path)
	}
	if prior.GlobalThresholdPct != 5.0 {
		t.Fatalf("GlobalThresholdPct = %v; want 5.0 (0 means a foreign schema leaked in)",
			prior.GlobalThresholdPct)
	}
}

// With no result this tool wrote, there is no prior — the first-run case. It
// must NOT fall back to a foreign result.
func TestOnlyForeignResultsMeansNoPrior(t *testing.T) {
	cfgDir := t.TempDir()
	writeRun(t, cfgDir, "v2b", vcompareResult, time.Now())

	cur := filepath.Join(cfgDir, "20260102_000000")
	prior, _, err := findPriorResult(filepath.Join(cur, "result.json"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior != nil {
		t.Fatalf("expected no prior run; got %+v", prior)
	}
}

// Recency is a property of the write, not of the directory name. A run dir
// named out of convention must not be able to masquerade as the latest.
func TestPriorSelectedByModTimeNotName(t *testing.T) {
	cfgDir := t.TempDir()
	now := time.Now()
	writeRun(t, cfgDir, "aaa_oldname_newest", vdiffResult, now)
	writeRun(t, cfgDir, "zzz_newname_oldest", vdiffResult, now.Add(-2*time.Hour))

	cur := filepath.Join(cfgDir, "current")
	_, path, err := findPriorResult(filepath.Join(cur, "result.json"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if got := filepath.Base(filepath.Dir(path)); got != "aaa_oldname_newest" {
		t.Fatalf("selected %s; expected the most recently WRITTEN run", got)
	}
}

// A result.json predating the marker cannot be proven to be ours, so it is not
// treated as a prior run.
func TestUnmarkedResultIsNotAPriorRun(t *testing.T) {
	cfgDir := t.TempDir()
	writeRun(t, cfgDir, "20260101_000000", `{"global_threshold_pct":5.0}`, time.Now())

	cur := filepath.Join(cfgDir, "20260102_000000")
	prior, _, err := findPriorResult(filepath.Join(cur, "result.json"))
	if err != nil {
		t.Fatalf("findPriorResult: %v", err)
	}
	if prior != nil {
		t.Fatal("an unmarked result must not be adopted as a prior run")
	}
}
