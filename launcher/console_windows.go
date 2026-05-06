//go:build windows

package main

import (
	"os"
	"syscall"
)

// attachParentConsole reattaches stdout/stderr/stdin to the parent
// process's console, if any. Required because Wails builds the
// launcher as a Windows GUI-subsystem binary (-H=windowsgui), which
// detaches std handles when launched from a terminal — CLI subcommands
// would otherwise produce no visible output.
//
// Standard pattern for Go GUI/CLI hybrid binaries on Windows. No-op
// when there's no parent console (e.g. launcher invoked via shortcut).
//
// See docs/launcher.md "Console output on Windows".
func attachParentConsole() {
	const ATTACH_PARENT_PROCESS = ^uint32(0) // -1, per Win32 docs

	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	attach := kernel32.NewProc("AttachConsole")

	r1, _, _ := attach.Call(uintptr(ATTACH_PARENT_PROCESS))
	if r1 == 0 {
		// No parent console (launched from Explorer, scheduled task,
		// etc.). Leave std handles as Wails set them up.
		return
	}

	// Reopen Go's high-level os.Stdout/Stderr/Stdin against the
	// console handles we just attached to. Without this, fmt.Println
	// goes to the original (NULL) handles even though the kernel-level
	// handles are now valid.
	if h, err := syscall.GetStdHandle(syscall.STD_OUTPUT_HANDLE); err == nil && h != 0 && h != syscall.InvalidHandle {
		os.Stdout = os.NewFile(uintptr(h), "/dev/stdout")
	}
	if h, err := syscall.GetStdHandle(syscall.STD_ERROR_HANDLE); err == nil && h != 0 && h != syscall.InvalidHandle {
		os.Stderr = os.NewFile(uintptr(h), "/dev/stderr")
	}
	if h, err := syscall.GetStdHandle(syscall.STD_INPUT_HANDLE); err == nil && h != 0 && h != syscall.InvalidHandle {
		os.Stdin = os.NewFile(uintptr(h), "/dev/stdin")
	}
}
