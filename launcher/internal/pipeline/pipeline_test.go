package pipeline

import (
	"archive/zip"
	"os"
	"path/filepath"
	"testing"
)

func TestCollectAssetFiles_FindsSW3Z(t *testing.T) {
	dir := t.TempDir()

	// Create a fake .pk3 and .sw3z file.
	os.WriteFile(filepath.Join(dir, "pak0.pk3"), []byte("pk3"), 0o644)
	os.WriteFile(filepath.Join(dir, "pax01.sw3z"), []byte("sw3z"), 0o644)
	os.WriteFile(filepath.Join(dir, "notes.txt"), []byte("ignored"), 0o644)

	files := collectAssetFiles(dir)
	if len(files) != 2 {
		t.Fatalf("expected 2 asset files (.pk3 + .sw3z), got %d", len(files))
	}

	names := make(map[string]bool)
	for _, f := range files {
		names[f.Name] = true
	}
	if !names["pak0.pk3"] {
		t.Error("expected pak0.pk3 in results")
	}
	if !names["pax01.sw3z"] {
		t.Error("expected pax01.sw3z in results")
	}
}

func TestPipeline_NoProcessors_AllSkipped(t *testing.T) {
	// With no processors, all entries are skipped (whitelist model).
	// The pipeline should run without error and produce no output archive.
	dir := t.TempDir()
	srcDir := filepath.Join(dir, "source")
	destDir := filepath.Join(dir, "dest")
	os.MkdirAll(srcDir, 0755)
	os.MkdirAll(destDir, 0755)

	// Create a minimal pk3 in source.
	pk3Path := filepath.Join(srcDir, "pak0.pk3")
	f, _ := os.Create(pk3Path)
	zw := zip.NewWriter(f)
	w, _ := zw.Create("test.txt")
	w.Write([]byte("data"))
	zw.Close()
	f.Close()

	// Run pipeline with no processors — everything should be skipped.
	sources := []SourceGroup{{Origin: "q3_base", Dir: srcDir}}

	// We can't easily construct a full Pipeline without config.Paths,
	// so test the Extract → Process → empty output chain directly.
	extract := &ExtractTransform{Sources: sources}
	progress := make(chan Progress, 100)
	go func() { for range progress {} }()

	entries, err := extract.Run(t.Context(), progress)
	if err != nil {
		t.Fatalf("extract: %v", err)
	}
	if len(entries) == 0 {
		t.Fatal("expected entries from source")
	}

	// Process with no processors → all skipped.
	process := &ProcessStep{Processors: nil}
	progress2 := make(chan Progress, 100)
	go func() { for range progress2 {} }()

	output, err := process.Run(t.Context(), entries, progress2)
	if err != nil {
		t.Fatalf("process: %v", err)
	}
	if len(output) != 0 {
		t.Errorf("expected 0 output (all skipped), got %d", len(output))
	}
}

func TestPipeline_EndToEnd_MockProcessor(t *testing.T) {
	dir := t.TempDir()
	srcDir := filepath.Join(dir, "source")
	destDir := filepath.Join(dir, "dest")
	os.MkdirAll(srcDir, 0755)
	os.MkdirAll(destDir, 0755)

	// Create source pk3 with known content.
	pk3Path := filepath.Join(srcDir, "pak0.pk3")
	f, _ := os.Create(pk3Path)
	zw := zip.NewWriter(f)
	w, _ := zw.Create("maps/arena7.bsp")
	w.Write([]byte("bsp data"))
	w2, _ := zw.Create("video/intro.roq")
	w2.Write([]byte("roq data"))
	zw.Close()
	f.Close()

	// Processor: include maps/*, skip everything else.
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			if len(e.Path) > 5 && e.Path[:5] == "maps/" {
				return EntryDecision{Action: Include}, nil
			}
			return EntryDecision{Action: Skip}, nil
		},
	}

	// Extract.
	extract := &ExtractTransform{Sources: []SourceGroup{{Origin: "q3_base", Dir: srcDir}}}
	progress := make(chan Progress, 100)
	go func() { for range progress {} }()

	entries, err := extract.Run(t.Context(), progress)
	if err != nil {
		t.Fatalf("extract: %v", err)
	}

	// Process.
	process := &ProcessStep{Processors: []Processor{proc}}
	progress2 := make(chan Progress, 100)
	go func() { for range progress2 {} }()

	output, err := process.Run(t.Context(), entries, progress2)
	if err != nil {
		t.Fatalf("process: %v", err)
	}

	// Should have 1 entry (maps/arena7.bsp included, video/intro.roq skipped).
	if len(output) != 1 {
		t.Fatalf("expected 1 output, got %d", len(output))
	}
	if output[0].OutputPath != "maps/arena7.bsp" {
		t.Errorf("expected maps/arena7.bsp, got %s", output[0].OutputPath)
	}

	// Write to SW3Z.
	sink := &SW3ZSink{DestDir: destDir, OutputName: "test.sw3z"}
	progress3 := make(chan Progress, 100)
	go func() { for range progress3 {} }()

	err = sink.Run(t.Context(), output, progress3)
	if err != nil {
		t.Fatalf("sink: %v", err)
	}

	// Verify output exists.
	outPath := filepath.Join(destDir, "test.sw3z")
	if _, err := os.Stat(outPath); os.IsNotExist(err) {
		t.Fatal("expected output file to exist")
	}
}
