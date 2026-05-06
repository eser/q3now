package pipeline

import (
	"archive/zip"
	"context"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/manifest"
)

// IDQuake3PackURL is the canonical URL for the redistributable id-quakepack
// bundle (Q3 demo + Q3TA demo + Q1 demo + missionpack PAKs). Hosted by the
// q3now project; mirror or override at your own risk.
//
// This is the same URL the launcher's GUI DownloadFreeResources flow uses;
// extracted here so CLI invocations share it.
const IDQuake3PackURL = "https://objects.eser.live/quakedata/id-quakepack.zip"

// DownloadOpts configures a RunDownload invocation.
type DownloadOpts struct {
	// URL of the bundle to download. Defaults to IDQuake3PackURL when empty.
	URL string
}

// RunDownload fetches and extracts the id-quakepack bundle. Idempotent:
// re-runs that find an already-downloaded zip skip the download step.
//
// The orchestration is shared between the GUI flow (driven by
// app.runDownload, which wraps with a WailsReporter) and the CLI flow
// (driven by cmd/assets_download.go, which wraps with a StdoutReporter).
//
// Cancellation: ctx cancellation aborts the download and returns the
// context error. The reporter receives Error() in that case.
func RunDownload(ctx context.Context, paths *config.Paths, opts DownloadOpts, reporter Reporter) error {
	url := opts.URL
	if url == "" {
		url = IDQuake3PackURL
	}

	dlDir := paths.DownloadDir()
	zipPath := filepath.Join(dlDir, "id-quakepack.zip")
	unzipDir := filepath.Join(dlDir, "id-quakepack")

	// ── Step 1: Download (skip if cached) ────────────────────────────────
	if fileExists(zipPath) {
		slog.Info("zip already downloaded, skipping download", "file", zipPath)
	} else {
		reporter.Progress("download", 0, 2, "Downloading Quake III data pack...")

		err := downloadFile(ctx, url, zipPath, func(received, total int64) {
			reporter.Progress("download", 0, 2,
				fmt.Sprintf("Downloading Quake III data pack... (%d MB)", received/(1024*1024)))
		})
		if err != nil {
			if ctx.Err() != nil {
				slog.Info("download cancelled by user")
				reporter.Error(ctx.Err())
				return ctx.Err()
			}
			slog.Error("download failed", "url", url, "error", err)
			reporter.Error(err)
			return err
		}
	}

	if ctx.Err() != nil {
		reporter.Error(ctx.Err())
		return ctx.Err()
	}

	// ── Step 2: Unzip ────────────────────────────────────────────────────
	reporter.Progress("extract", 1, 2, "Extracting game data...")

	if err := unzipFile(zipPath, unzipDir); err != nil {
		slog.Error("unzip failed", "file", zipPath, "error", err)
		reporter.Error(fmt.Errorf("failed to extract: %w", err))
		return err
	}

	// ── Step 3: Done ─────────────────────────────────────────────────────
	reporter.Progress("complete", 2, 2, "Download complete!")
	reporter.Done("Download complete!")
	return nil
}

// ImportOpts configures a RunImport invocation.
type ImportOpts struct {
	// Q3Path is the user's full-version Q3 install root, or empty for
	// demo-only import. When non-empty, pax02 (baseq3) and pax04
	// (missionpack) are processed alongside pax01 (id-quakepack).
	Q3Path string
}

// paxJob describes one pipeline pass that produces a single .sw3z file.
type paxJob struct {
	name    string
	output  string
	sources []SourceGroup
	entries map[string]ProcessorEntry
}

// RunImport executes the multi-pass import pipeline:
//
//	validate downloads → build pax jobs → run each pipeline → write manifest
//
// Like RunDownload, this orchestration is shared between GUI (with
// WailsReporter) and CLI (with StdoutReporter). Cancellation aborts the
// in-progress pax pass and returns ctx.Err().
func RunImport(ctx context.Context, paths *config.Paths, opts ImportOpts, reporter Reporter) error {
	// ── Step 1: Validate downloaded pack ─────────────────────────────────
	reporter.Progress("scan", 0, 1, "Checking downloaded resources...")

	packDir := filepath.Join(paths.DownloadDir(), "id-quakepack")
	if !dirExists(packDir) {
		err := fmt.Errorf("free resources not downloaded yet — run `assets download` first")
		reporter.Error(err)
		return err
	}

	// ── Step 2: Build pax jobs ───────────────────────────────────────────
	jobs := []paxJob{
		{
			name:    "pax01",
			output:  "pax01.sw3z",
			sources: []SourceGroup{{Origin: "id-quakepack", Dir: packDir}},
			entries: Q3CopyPax01Entries,
		},
	}

	if opts.Q3Path != "" {
		baseQ3Dir := filepath.Join(opts.Q3Path, "baseq3")
		if fileExists(baseQ3Dir) {
			jobs = append(jobs, paxJob{
				name:    "pax02",
				output:  "pax02.sw3z",
				sources: []SourceGroup{{Origin: "q3_base", Dir: baseQ3Dir}},
				entries: Q3CopyPax02Entries,
			})
		}

		missionpackDir := filepath.Join(opts.Q3Path, "missionpack")
		if fileExists(missionpackDir) {
			jobs = append(jobs, paxJob{
				name:    "pax04",
				output:  "pax04.sw3z",
				sources: []SourceGroup{{Origin: "q3_ta", Dir: missionpackDir}},
				entries: Q3CopyPax04Entries,
			})
		}
	}

	reporter.Progress("scan", 1, 1, fmt.Sprintf("Preparing %d asset packs...", len(jobs)))

	// ── Step 3: Run each pipeline pass ───────────────────────────────────
	for i, job := range jobs {
		if ctx.Err() != nil {
			slog.Info("import cancelled by user")
			reporter.Error(ctx.Err())
			return ctx.Err()
		}

		reporter.Progress("process", i, len(jobs),
			fmt.Sprintf("Processing %s (%d/%d)...", job.name, i+1, len(jobs)))

		pip := New(paths, job.sources,
			WithProcessors(&Q3CopyProcessor{Entries: job.entries}),
			WithOutputName(job.output),
		)

		// Forward pipeline progress events to the reporter.
		go func(jobName string) {
			for p := range pip.Progress() {
				reporter.Progress(p.Step, int(p.Current), int(p.Total),
					fmt.Sprintf("[%s] %s", jobName, p.Message))
			}
		}(job.name)

		if err := pip.Run(ctx); err != nil {
			if ctx.Err() != nil {
				slog.Info("import cancelled by user")
				reporter.Error(ctx.Err())
				return ctx.Err()
			}
			slog.Error("pipeline failed", "pax", job.name, "error", err)
			reporter.Error(fmt.Errorf("failed to process %s: %w", job.name, err))
			return err
		}
	}

	// ── Step 4: Write manifest (signals "assets are ready") ──────────────
	inventory := map[string]string{"id-quakepack": packDir}
	if opts.Q3Path != "" {
		inventory["q3_install"] = opts.Q3Path
	}

	if err := manifest.WriteWithInventory(paths.ManifestPath(), inventory); err != nil {
		slog.Error("failed to write manifest", "error", err)
		reporter.Error(err)
		return err
	}

	slog.Info("import complete", "jobs", len(jobs))
	reporter.Progress("complete", 1, 1, "Import successful!")
	reporter.Done("Import successful!")
	return nil
}

// ── Internal helpers (extracted from app.go) ────────────────────────────────

// downloadFile fetches a URL to a local path with atomic write and
// progress callbacks. Self-bootstraps the destination directory per
// docs/launcher.md "Writer self-bootstrap".
func downloadFile(ctx context.Context, url, destPath string, onProgress func(received, total int64)) error {
	if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
		return fmt.Errorf("creating directory: %w", err)
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("HTTP %d for %s", resp.StatusCode, url)
	}

	tmpPath := destPath + ".downloading"
	out, err := os.Create(tmpPath)
	if err != nil {
		return err
	}
	defer func() {
		out.Close()
		os.Remove(tmpPath) // cleanup on error; no-op after successful rename
	}()

	var received int64
	buf := make([]byte, 32*1024)
	for {
		n, readErr := resp.Body.Read(buf)
		if n > 0 {
			if _, writeErr := out.Write(buf[:n]); writeErr != nil {
				return writeErr
			}
			received += int64(n)
			if onProgress != nil {
				onProgress(received, resp.ContentLength)
			}
		}
		if readErr != nil {
			if readErr == io.EOF {
				break
			}
			return readErr
		}
	}

	if err := out.Sync(); err != nil {
		return err
	}
	out.Close()

	return os.Rename(tmpPath, destPath)
}

// unzipFile extracts a zip archive to destDir, creating directories as
// needed. Includes zip slip protection to prevent path traversal.
func unzipFile(src, destDir string) error {
	r, err := zip.OpenReader(src)
	if err != nil {
		return fmt.Errorf("opening zip: %w", err)
	}
	defer r.Close()

	destDir, err = filepath.Abs(destDir)
	if err != nil {
		return fmt.Errorf("resolving destination: %w", err)
	}

	for _, f := range r.File {
		target := filepath.Join(destDir, f.Name)
		if !strings.HasPrefix(filepath.Clean(target), filepath.Clean(destDir)+string(os.PathSeparator)) {
			return fmt.Errorf("zip slip detected: %s", f.Name)
		}

		if f.FileInfo().IsDir() {
			if err := os.MkdirAll(target, 0755); err != nil {
				return fmt.Errorf("creating directory %s: %w", target, err)
			}
			continue
		}

		if err := os.MkdirAll(filepath.Dir(target), 0755); err != nil {
			return fmt.Errorf("creating parent directory for %s: %w", target, err)
		}

		if err := extractZipEntry(f, target); err != nil {
			return err
		}
	}

	return nil
}

func extractZipEntry(f *zip.File, target string) error {
	rc, err := f.Open()
	if err != nil {
		return fmt.Errorf("opening zip entry %s: %w", f.Name, err)
	}
	defer rc.Close()

	out, err := os.OpenFile(target, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, f.Mode())
	if err != nil {
		return fmt.Errorf("creating file %s: %w", target, err)
	}
	defer out.Close()

	if _, err := io.Copy(out, rc); err != nil {
		return fmt.Errorf("writing file %s: %w", target, err)
	}

	return nil
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func dirExists(path string) bool {
	info, err := os.Stat(path)
	return err == nil && info.IsDir()
}

// IsBundleStaged reports whether a previous RunDownload completed
// (the unzipped bundle directory exists). Used by callers that want
// to skip the download phase or report idempotency.
func IsBundleStaged(paths *config.Paths) bool {
	return dirExists(filepath.Join(paths.DownloadDir(), "id-quakepack"))
}

// touchTime helps tests assert "file was just written"; keeping the
// exported API symmetric with time.Now in the GUI flow.
var _ = time.Now
