package pipeline

import (
	"context"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/manifest"
)

// Pipeline orchestrates the Extract → Process → SW3Z Sink flow.
type Pipeline struct {
	paths      *config.Paths
	sources    []SourceGroup
	processors []Processor
	outputName string
	progress   chan Progress
	result     *manifest.PipelineResult
}

// Option configures optional pipeline behavior.
type Option func(*Pipeline)

// WithProcessors adds processors to the pipeline chain.
func WithProcessors(processors ...Processor) Option {
	return func(p *Pipeline) {
		p.processors = append(p.processors, processors...)
	}
}

// WithOutputName sets the output archive filename (default: "pax01.sw3z").
func WithOutputName(name string) Option {
	return func(p *Pipeline) {
		p.outputName = name
	}
}

// New creates a pipeline for importing assets from multiple source groups.
func New(paths *config.Paths, sources []SourceGroup, opts ...Option) *Pipeline {
	p := &Pipeline{
		paths:      paths,
		sources:    sources,
		outputName: "pax01.sw3z",
		progress:   make(chan Progress, 100),
	}
	for _, opt := range opts {
		opt(p)
	}
	return p
}

// Progress returns the channel for reading progress events.
func (p *Pipeline) Progress() <-chan Progress {
	return p.progress
}

// Result returns the pipeline result after Run completes successfully.
func (p *Pipeline) Result() *manifest.PipelineResult {
	return p.result
}

// Run executes the full import pipeline: Extract → Process → SW3Z Sink.
func (p *Pipeline) Run(ctx context.Context) error {
	defer close(p.progress)

	slog.Info("pipeline: starting",
		"sources", len(p.sources),
		"processors", len(p.processors),
		"output", p.outputName)

	// --- Extract ---
	extract := &ExtractTransform{Sources: p.sources}
	entries, err := extract.Run(ctx, p.progress)
	if err != nil {
		return fmt.Errorf("extract: %w", err)
	}

	// --- Process ---
	process := &ProcessStep{Processors: p.processors}
	output, err := process.Run(ctx, entries, p.progress)
	if err != nil {
		return fmt.Errorf("process: %w", err)
	}

	// --- Sink: SW3Z ---
	destDir := p.paths.BaseQ3Dir()
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return fmt.Errorf("creating destination directory: %w", err)
	}

	sink := &SW3ZSink{
		DestDir:    destDir,
		OutputName: p.outputName,
	}
	if err := sink.Run(ctx, output, p.progress); err != nil {
		return fmt.Errorf("repackage: %w", err)
	}

	// Build result for manifest.
	p.result = &manifest.PipelineResult{
		SourcePath: p.sources[0].Dir,
		SourceType: "multi-origin",
	}
	p.result.BaseQ3 = collectAssetFiles(destDir)

	slog.Info("pipeline: complete")
	return nil
}

// CleanupPartials removes any stale .importing temp files from a directory.
func CleanupPartials(dir string) {
	matches, err := filepath.Glob(filepath.Join(dir, "*.importing"))
	if err != nil {
		return
	}
	for _, m := range matches {
		slog.Info("cleaning up partial import", "file", m)
		os.Remove(m)
	}
}

// collectAssetFiles lists pk3 and sw3z files in a directory for the manifest.
func collectAssetFiles(dir string) []manifest.AssetFile {
	var files []manifest.AssetFile
	for _, pattern := range []string{"*.pk3", "*.sw3z"} {
		matches, err := filepath.Glob(filepath.Join(dir, pattern))
		if err != nil {
			continue
		}
		for _, m := range matches {
			info, err := os.Stat(m)
			if err != nil {
				continue
			}
			files = append(files, manifest.AssetFile{
				Name: filepath.Base(m),
				Size: info.Size(),
			})
		}
	}
	return files
}
