// sw3z is a CLI tool for creating and inspecting SW3Z game asset archives.
//
// Usage:
//
//	sw3z a [-Z none|lz4] [-0] [-x pattern]... <output.sw3z> <input-dir>
//	sw3z l <archive.sw3z>
//	sw3z t <archive.sw3z>
//	sw3z x <archive.sw3z> [output-dir]
//
// Subcommands:
//
//	a    Archive: recursively pack a directory into an sw3z file.
//	     Skips dotfiles and dotdirs (.DS_Store, .git/, etc.).
//	     Use -x to exclude additional glob patterns.
//	     Use -Z to control compression (default: auto-detect).
//	     Use -0 for uncompressed (shorthand for -Z none).
//	l    List: show archive contents.
//	t    Test: verify integrity of all entries.
//	x    Extract: extract archive to disk.
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	sw3z "github.com/eser/q3now/tools/sw3z-archiver"
)

// compressionMode controls how files are compressed.
type compressionMode int

const (
	compAuto compressionMode = iota // try LZ4, keep if smaller
	compNone                        // store raw
	compLZ4                         // force LZ4
)

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(1)
	}

	switch os.Args[1] {
	case "a":
		args := os.Args[2:]
		var excludes []string
		mode := compAuto

		// Parse flags.
		for len(args) >= 1 {
			switch {
			case args[0] == "-x" && len(args) >= 2:
				excludes = append(excludes, args[1])
				args = args[2:]
			case args[0] == "-Z" && len(args) >= 2:
				switch strings.ToLower(args[1]) {
				case "none":
					mode = compNone
				case "lz4":
					mode = compLZ4
				default:
					fmt.Fprintf(os.Stderr, "unknown compression: %s (use none or lz4)\n", args[1])
					os.Exit(1)
				}
				args = args[2:]
			case args[0] == "-0":
				mode = compNone
				args = args[1:]
			default:
				goto doneFlags
			}
		}
	doneFlags:

		if len(args) != 2 {
			fmt.Fprintf(os.Stderr, "usage: sw3z a [-Z none|lz4] [-0] [-x pattern]... <output.sw3z> <input-dir>\n")
			os.Exit(1)
		}

		if err := archive(args[0], args[1], excludes, mode); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	case "l":
		if len(os.Args) != 3 {
			fmt.Fprintf(os.Stderr, "usage: sw3z l <archive.sw3z>\n")
			os.Exit(1)
		}
		if err := listArchive(os.Args[2]); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	case "t":
		if len(os.Args) != 3 {
			fmt.Fprintf(os.Stderr, "usage: sw3z t <archive.sw3z>\n")
			os.Exit(1)
		}
		if err := testArchive(os.Args[2]); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	case "x":
		if len(os.Args) < 3 || len(os.Args) > 4 {
			fmt.Fprintf(os.Stderr, "usage: sw3z x <archive.sw3z> [output-dir]\n")
			os.Exit(1)
		}
		outDir := "."
		if len(os.Args) == 4 {
			outDir = os.Args[3]
		}
		if err := extractArchive(os.Args[2], outDir); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	default:
		fmt.Fprintf(os.Stderr, "unknown subcommand: %s\n", os.Args[1])
		usage()
		os.Exit(1)
	}
}

func usage() {
	fmt.Fprintf(os.Stderr, "usage: sw3z <subcommand> [args]\n\n")
	fmt.Fprintf(os.Stderr, "subcommands:\n")
	fmt.Fprintf(os.Stderr, "  a [-Z none|lz4] [-0] [-x pattern]... <output.sw3z> <input-dir>\n")
	fmt.Fprintf(os.Stderr, "      archive a directory\n")
	fmt.Fprintf(os.Stderr, "      -Z none|lz4  compression method (default: auto-detect)\n")
	fmt.Fprintf(os.Stderr, "      -0            store uncompressed (shorthand for -Z none)\n")
	fmt.Fprintf(os.Stderr, "      -x pattern    exclude files matching glob pattern\n")
	fmt.Fprintf(os.Stderr, "  l <archive.sw3z>\n")
	fmt.Fprintf(os.Stderr, "      list archive contents\n")
	fmt.Fprintf(os.Stderr, "  t <archive.sw3z>\n")
	fmt.Fprintf(os.Stderr, "      test archive integrity\n")
	fmt.Fprintf(os.Stderr, "  x <archive.sw3z> [output-dir]\n")
	fmt.Fprintf(os.Stderr, "      extract archive to disk (default: current directory)\n")
}

func archive(outputPath, inputDir string, excludes []string, mode compressionMode) error {
	info, err := os.Stat(inputDir)
	if err != nil {
		return fmt.Errorf("input directory: %w", err)
	}
	if !info.IsDir() {
		return fmt.Errorf("%s is not a directory", inputDir)
	}

	out, err := os.Create(outputPath)
	if err != nil {
		return fmt.Errorf("create output: %w", err)
	}
	defer out.Close()

	w := sw3z.Create(out)
	var count, compCount int
	var totalSize, savedSize int64

	err = filepath.Walk(inputDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		name := info.Name()

		if strings.HasPrefix(name, ".") {
			if info.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}

		if info.IsDir() {
			return nil
		}

		relPath, err := filepath.Rel(inputDir, path)
		if err != nil {
			return fmt.Errorf("relative path for %s: %w", path, err)
		}
		relPath = filepath.ToSlash(relPath)

		if matchesAny(relPath, name, excludes) {
			return nil
		}

		data, err := os.ReadFile(path)
		if err != nil {
			return fmt.Errorf("read %s: %w", relPath, err)
		}

		compression := chooseCompression(data, mode)

		if err := w.AddFile(relPath, data, compression); err != nil {
			return fmt.Errorf("add %s: %w", relPath, err)
		}

		count++
		totalSize += int64(len(data))
		tag := ""
		if compression == sw3z.CompressLZ4 {
			compCount++
			tag = " [lz4]"
		}
		fmt.Printf("  + %s (%d bytes)%s\n", relPath, len(data), tag)
		return nil
	})
	if err != nil {
		return err
	}

	if err := w.Close(); err != nil {
		return fmt.Errorf("finalize archive: %w", err)
	}

	outInfo, _ := os.Stat(outputPath)
	savedSize = totalSize - outInfo.Size()
	fmt.Printf("Created %s (%d bytes, %d entries, %d/%d compressed",
		outputPath, outInfo.Size(), count, compCount, count)
	if savedSize > 0 {
		fmt.Printf(", saved %d bytes", savedSize)
	}
	fmt.Println(")")
	return nil
}

// chooseCompression decides the compression for a single file.
//
// Auto-detect: compress with LZ4, keep if result is smaller.
// Already-compressed formats (JPG, PNG, etc.) won't benefit from LZ4
// and the auto-detect will store them raw.
func chooseCompression(data []byte, mode compressionMode) uint8 {
	switch mode {
	case compNone:
		return sw3z.CompressNone
	case compLZ4:
		return sw3z.CompressLZ4
	default: // compAuto
		if len(data) == 0 {
			return sw3z.CompressNone
		}
		// Try LZ4 compression. AddFile will do the actual compression.
		// We do a trial compression here to see if it's worth it.
		compressed, err := sw3z.CompressLZ4Data(data)
		if err != nil || len(compressed) >= len(data) {
			return sw3z.CompressNone
		}
		return sw3z.CompressLZ4
	}
}

func listArchive(archivePath string) error {
	f, err := os.Open(archivePath)
	if err != nil {
		return err
	}
	defer f.Close()

	h, err := sw3z.ReadHeader(f)
	if err != nil {
		return err
	}
	entries, err := sw3z.ReadIndex(f, h.EntryCount)
	if err != nil {
		return err
	}
	st, err := sw3z.ReadStringTable(f, h.StringTableSize)
	if err != nil {
		return err
	}

	fmt.Printf("%-50s %12s %12s  %-5s  %10s\n",
		"Path", "Size", "Compressed", "Method", "CRC32C")
	fmt.Println(strings.Repeat("-", 95))

	var totalUncomp, totalComp uint64
	for _, e := range entries {
		path := sw3z.EntryPath(e, st)
		fmt.Printf("%-50s %12d %12d  %-5s  %08X\n",
			path, e.UncompressedSize, e.CompressedSize,
			sw3z.CompressionName(e.Compression), e.CRC32C)
		totalUncomp += uint64(e.UncompressedSize)
		totalComp += uint64(e.CompressedSize)
	}

	fmt.Println(strings.Repeat("-", 95))
	fmt.Printf("%d entries, %d bytes uncompressed, %d bytes compressed\n",
		h.EntryCount, totalUncomp, totalComp)
	return nil
}

func testArchive(archivePath string) error {
	f, err := os.Open(archivePath)
	if err != nil {
		return err
	}
	defer f.Close()

	h, err := sw3z.ReadHeader(f)
	if err != nil {
		return err
	}
	entries, err := sw3z.ReadIndex(f, h.EntryCount)
	if err != nil {
		return err
	}
	st, err := sw3z.ReadStringTable(f, h.StringTableSize)
	if err != nil {
		return err
	}

	var failures int
	for _, e := range entries {
		path := sw3z.EntryPath(e, st)
		_, err := sw3z.VerifyEntry(f, e)
		if err != nil {
			fmt.Printf("  FAIL %s: %v\n", path, err)
			failures++
		} else {
			fmt.Printf("  OK   %s\n", path)
		}
	}

	fmt.Printf("\n%d entries tested, %d OK, %d FAILED\n",
		len(entries), len(entries)-failures, failures)
	if failures > 0 {
		return fmt.Errorf("%d entries failed integrity check", failures)
	}
	return nil
}

func extractArchive(archivePath, outDir string) error {
	f, err := os.Open(archivePath)
	if err != nil {
		return err
	}
	defer f.Close()

	h, err := sw3z.ReadHeader(f)
	if err != nil {
		return err
	}
	entries, err := sw3z.ReadIndex(f, h.EntryCount)
	if err != nil {
		return err
	}
	st, err := sw3z.ReadStringTable(f, h.StringTableSize)
	if err != nil {
		return err
	}

	var failures int
	for _, e := range entries {
		path := sw3z.EntryPath(e, st)

		if !sw3z.IsPathSafe(path) {
			fmt.Printf("  SKIP %s (unsafe path)\n", path)
			failures++
			continue
		}

		if e.Flags&sw3z.FlagSymlink != 0 {
			fmt.Printf("  SKIP %s (symlink not supported)\n", path)
			continue
		}

		data, err := sw3z.VerifyEntry(f, e)
		if err != nil {
			fmt.Printf("  FAIL %s: %v\n", path, err)
			failures++
			continue
		}

		dest := filepath.Join(outDir, filepath.FromSlash(path))

		if err := os.MkdirAll(filepath.Dir(dest), 0755); err != nil {
			fmt.Printf("  FAIL %s: mkdir: %v\n", path, err)
			failures++
			continue
		}

		perm := os.FileMode(0644)
		if e.Flags&sw3z.FlagExecutable != 0 {
			perm = 0755
		}
		if err := os.WriteFile(dest, data, perm); err != nil {
			fmt.Printf("  FAIL %s: write: %v\n", path, err)
			failures++
			continue
		}

		fmt.Printf("  OK   %s (%d bytes)\n", path, len(data))
	}

	fmt.Printf("\n%d entries, %d extracted, %d failed\n",
		len(entries), len(entries)-failures, failures)
	if failures > 0 {
		return fmt.Errorf("%d entries failed", failures)
	}
	return nil
}

func matchesAny(relPath, name string, patterns []string) bool {
	for _, pattern := range patterns {
		if matched, _ := filepath.Match(pattern, relPath); matched {
			return true
		}
		if matched, _ := filepath.Match(pattern, name); matched {
			return true
		}
		if strings.HasPrefix(pattern, "**/") {
			inner := pattern[3:]
			if matched, _ := filepath.Match(inner, name); matched {
				return true
			}
		}
	}
	return false
}
