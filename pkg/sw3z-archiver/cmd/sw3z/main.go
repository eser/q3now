// sw3z is a CLI tool for creating SW3Z game asset archives.
//
// Usage:
//
//	sw3z a [-Z none|lz4] [-0] [-x pattern]... <output.sw3z> <input-dir>
//
// Subcommands:
//
//	a    Archive: recursively pack a directory into an sw3z file.
//	     Skips dotfiles and dotdirs (.DS_Store, .git/, etc.).
//	     Use -x to exclude additional glob patterns.
//	     Use -Z to control compression (default: auto-detect).
//	     Use -0 for uncompressed (shorthand for -Z none).
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	sw3z "github.com/eser/q3now/pkg/sw3z-archiver"
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
