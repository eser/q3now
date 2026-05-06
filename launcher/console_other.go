//go:build !windows

package main

// attachParentConsole is a no-op on non-Windows platforms — Linux and
// macOS don't distinguish GUI vs console subsystems at the binary
// level, so stdout/stderr are always usable.
//
// See docs/launcher.md "Console output on Windows".
func attachParentConsole() {}
