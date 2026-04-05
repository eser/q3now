package pipeline

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	sw3z "github.com/eser/q3now/tools/sw3z-archiver"
)

func TestSW3ZSink_ValidOutput(t *testing.T) {
	dir := t.TempDir()
	sink := &SW3ZSink{DestDir: dir, OutputName: "test.sw3z"}

	output := []OutputEntry{
		{OutputPath: "maps/q3dm1.bsp", Data: []byte("bsp data"), SourceIndex: -1, UncompSize: 8},
		{OutputPath: "textures/wall.tga", Data: []byte("tga pixels"), SourceIndex: -1, UncompSize: 10},
	}

	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	err := sink.Run(context.Background(), output, progress)
	if err != nil {
		t.Fatalf("SW3ZSink.Run: %v", err)
	}

	// Verify output file exists and is valid SW3Z.
	outPath := filepath.Join(dir, "test.sw3z")
	f, err := os.Open(outPath)
	if err != nil {
		t.Fatalf("open output: %v", err)
	}
	defer f.Close()

	hdr, err := sw3z.ReadHeader(f)
	if err != nil {
		t.Fatalf("ReadHeader: %v", err)
	}
	if hdr.EntryCount != 2 {
		t.Errorf("expected 2 entries in header, got %d", hdr.EntryCount)
	}
}

func TestSW3ZSink_ReplacedEntries(t *testing.T) {
	dir := t.TempDir()
	sink := &SW3ZSink{DestDir: dir, OutputName: "replaced.sw3z"}

	replacedData := []byte("converted BSP v55 data")
	output := []OutputEntry{
		{OutputPath: "maps/converted.bsp", Data: replacedData, SourceIndex: -1, UncompSize: int64(len(replacedData))},
	}

	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	err := sink.Run(context.Background(), output, progress)
	if err != nil {
		t.Fatalf("SW3ZSink.Run: %v", err)
	}

	outPath := filepath.Join(dir, "replaced.sw3z")
	if _, err := os.Stat(outPath); os.IsNotExist(err) {
		t.Fatal("output file should exist")
	}
}

func TestSW3ZSink_EmptyOutput(t *testing.T) {
	dir := t.TempDir()
	sink := &SW3ZSink{DestDir: dir, OutputName: "empty.sw3z"}

	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	err := sink.Run(context.Background(), nil, progress)
	if err != nil {
		t.Fatalf("SW3ZSink.Run: %v", err)
	}

	outPath := filepath.Join(dir, "empty.sw3z")
	if _, err := os.Stat(outPath); !os.IsNotExist(err) {
		t.Error("no output file should be created for empty input")
	}
}

func TestSW3ZSink_ContextCancellation(t *testing.T) {
	dir := t.TempDir()
	sink := &SW3ZSink{DestDir: dir, OutputName: "cancel.sw3z"}

	// Create many entries to increase chance of hitting cancellation.
	var output []OutputEntry
	for i := 0; i < 1000; i++ {
		output = append(output, OutputEntry{
			OutputPath:  "file" + string(rune('0'+i%10)) + ".txt",
			Data:        []byte("data"),
			SourceIndex: -1,
			UncompSize:  4,
		})
	}
	// Actually this will fail on duplicate paths. Use unique paths.
	output = nil
	for i := 0; i < 100; i++ {
		output = append(output, OutputEntry{
			OutputPath:  fmt.Sprintf("file_%04d.txt", i),
			Data:        []byte("data"),
			SourceIndex: -1,
			UncompSize:  4,
		})
	}

	ctx, cancel := context.WithCancel(context.Background())
	cancel() // cancel immediately

	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	err := sink.Run(ctx, output, progress)
	if err == nil {
		t.Fatal("expected cancellation error")
	}
}

func TestSW3ZSink_ZeroByteEntry(t *testing.T) {
	dir := t.TempDir()
	sink := &SW3ZSink{DestDir: dir, OutputName: "zerobyte.sw3z"}

	output := []OutputEntry{
		{OutputPath: "empty.txt", Data: []byte{}, SourceIndex: -1, UncompSize: 0},
	}

	progress := make(chan Progress, 100)
	go func() {
		for range progress {
		}
	}()

	err := sink.Run(context.Background(), output, progress)
	if err != nil {
		t.Fatalf("SW3ZSink.Run: %v", err)
	}
}
