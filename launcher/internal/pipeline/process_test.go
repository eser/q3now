package pipeline

import (
	"context"
	"errors"
	"testing"
)

// mockProcessor implements Processor for testing.
type mockProcessor struct {
	processFunc  func(AssetEntry, func() ([]byte, error)) (EntryDecision, error)
	finalizeFunc func() ([]OutputEntry, error)
}

func (m *mockProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	if m.processFunc != nil {
		return m.processFunc(entry, readFile)
	}
	return EntryDecision{Action: Skip}, nil
}

func (m *mockProcessor) Finalize() ([]OutputEntry, error) {
	if m.finalizeFunc != nil {
		return m.finalizeFunc()
	}
	return nil, nil
}

func makeEntry(origin, path string) AssetEntry {
	return AssetEntry{
		Origin:      origin,
		Path:        path,
		SourcePak:   "/fake/pak0.pk3",
		SourceIndex: 0,
		UncompSize:  100,
	}
}

func runProcess(t *testing.T, entries []AssetEntry, processors ...Processor) []OutputEntry {
	t.Helper()
	step := &ProcessStep{Processors: processors}
	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()
	output, err := step.Run(context.Background(), entries, progress)
	if err != nil {
		t.Fatalf("ProcessStep.Run: %v", err)
	}
	return output
}

func TestProcess_SingleProcessor_Include(t *testing.T) {
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Include}, nil
		},
	}

	output := runProcess(t, []AssetEntry{makeEntry("q3_base", "maps/q3dm1.bsp")}, proc)
	if len(output) != 1 {
		t.Fatalf("expected 1 output, got %d", len(output))
	}
	if output[0].OutputPath != "maps/q3dm1.bsp" {
		t.Errorf("expected maps/q3dm1.bsp, got %s", output[0].OutputPath)
	}
}

func TestProcess_SingleProcessor_Skip(t *testing.T) {
	proc := &mockProcessor{} // default: Skip

	output := runProcess(t, []AssetEntry{makeEntry("q3_base", "video/intro.roq")}, proc)
	if len(output) != 0 {
		t.Fatalf("expected 0 output (skipped), got %d", len(output))
	}
}

func TestProcess_SingleProcessor_Replace(t *testing.T) {
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{
				Action:   Replace,
				DestPath: "maps/converted.bsp",
				Data:     []byte("new bsp data"),
			}, nil
		},
	}

	output := runProcess(t, []AssetEntry{makeEntry("q1_base", "maps/dm1.bsp")}, proc)
	if len(output) != 1 {
		t.Fatalf("expected 1 output, got %d", len(output))
	}
	if output[0].OutputPath != "maps/converted.bsp" {
		t.Errorf("expected maps/converted.bsp, got %s", output[0].OutputPath)
	}
	if string(output[0].Data) != "new bsp data" {
		t.Errorf("expected replacement data, got %q", output[0].Data)
	}
}

func TestProcess_Chain_LastNonSkipWins(t *testing.T) {
	proc1 := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Include}, nil // proc1: Include
		},
	}
	proc2 := &mockProcessor{} // proc2: Skip (default)
	proc3 := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Replace, Data: []byte("replaced")}, nil // proc3: Replace
		},
	}

	output := runProcess(t, []AssetEntry{makeEntry("q3_base", "test.txt")}, proc1, proc2, proc3)
	if len(output) != 1 {
		t.Fatalf("expected 1 output, got %d", len(output))
	}
	// Last non-Skip is Replace from proc3.
	if output[0].Data == nil {
		t.Error("expected Replace data, got nil (Include won instead)")
	}
}

func TestProcess_Chain_AllSkip(t *testing.T) {
	proc1 := &mockProcessor{}
	proc2 := &mockProcessor{}

	output := runProcess(t, []AssetEntry{makeEntry("q3_base", "test.txt")}, proc1, proc2)
	if len(output) != 0 {
		t.Fatalf("expected 0 output (all skipped), got %d", len(output))
	}
}

func TestProcess_Chain_Proc1IncludeProc2Skip(t *testing.T) {
	proc1 := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Include}, nil
		},
	}
	proc2 := &mockProcessor{} // Skip

	output := runProcess(t, []AssetEntry{makeEntry("q3_base", "test.txt")}, proc1, proc2)
	if len(output) != 1 {
		t.Fatalf("expected 1 output (Include from proc1 wins), got %d", len(output))
	}
	if output[0].Data != nil {
		t.Error("expected Include (nil Data), got data")
	}
}

func TestProcess_Finalize_Synthetics(t *testing.T) {
	proc := &mockProcessor{
		finalizeFunc: func() ([]OutputEntry, error) {
			return []OutputEntry{
				{OutputPath: "resources/arenalist.json", Data: []byte(`{"arenas":[]}`), SourceIndex: -1, UncompSize: 14},
			}, nil
		},
	}

	output := runProcess(t, nil, proc)
	if len(output) != 1 {
		t.Fatalf("expected 1 synthetic, got %d", len(output))
	}
	if output[0].OutputPath != "resources/arenalist.json" {
		t.Errorf("expected resources/arenalist.json, got %s", output[0].OutputPath)
	}
}

func TestProcess_Finalize_Empty(t *testing.T) {
	proc := &mockProcessor{} // Finalize returns nil

	output := runProcess(t, nil, proc)
	if len(output) != 0 {
		t.Fatalf("expected 0 output, got %d", len(output))
	}
}

func TestProcess_ReadFileCallback_LazyCall(t *testing.T) {
	readFileCalled := false
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
			// Don't call readFile — Include without reading content.
			readFileCalled = false // should stay false
			return EntryDecision{Action: Include}, nil
		},
	}

	runProcess(t, []AssetEntry{makeEntry("q3_base", "test.txt")}, proc)
	if readFileCalled {
		t.Error("readFile should not have been called for a simple Include")
	}
}

func TestProcess_CollisionDetection(t *testing.T) {
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Include}, nil
		},
	}

	step := &ProcessStep{Processors: []Processor{proc}}
	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	// Two entries with the same path → collision.
	entries := []AssetEntry{
		makeEntry("q3_base", "maps/q3dm1.bsp"),
		makeEntry("qlive_base", "maps/q3dm1.bsp"),
	}

	_, err := step.Run(context.Background(), entries, progress)
	if err == nil {
		t.Fatal("expected collision error for duplicate output paths")
	}
}

func TestProcess_DestPathTraversal(t *testing.T) {
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{Action: Include, DestPath: "../../etc/passwd"}, nil
		},
	}

	step := &ProcessStep{Processors: []Processor{proc}}
	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	_, err := step.Run(context.Background(), []AssetEntry{makeEntry("q3_base", "test.txt")}, progress)
	if err == nil {
		t.Fatal("expected error for unsafe DestPath")
	}
}

func TestProcess_ErrorPropagation(t *testing.T) {
	procErr := errors.New("processor exploded")
	proc := &mockProcessor{
		processFunc: func(e AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
			return EntryDecision{}, procErr
		},
	}

	step := &ProcessStep{Processors: []Processor{proc}}
	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	_, err := step.Run(context.Background(), []AssetEntry{makeEntry("q3_base", "test.txt")}, progress)
	if err == nil {
		t.Fatal("expected error propagation")
	}
}
