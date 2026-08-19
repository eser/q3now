// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// win-fault-inject — deliver a real access violation to a running process from
// OUTSIDE it, so a crash handler can be measured without a crash path being
// built into the shipped engine.
//
// Usage:
//
//	win-fault-inject -pid 1234
//	win-fault-inject -name wired-headless.x64.exe
//
// WHY THIS EXISTS
// The crash-evidence harness (tests/crash-evidence-check.sh) faults the engine
// with `kill -SEGV` on POSIX. Windows has no equivalent, and the two obvious
// substitutes are both wrong:
//
//   - `taskkill` (with or without /F) terminates the process without raising a
//     structured exception, so SetUnhandledExceptionFilter never runs. The
//     harness would report a crash that produced no artefacts and read it as a
//     handler defect, when in fact no fault was ever delivered.
//   - A `crash` console command inside the engine would have to exist in
//     RELEASE builds to be reachable here — shipping a way to crash the shipped
//     engine in order to test it. Explicitly rejected.
//
// So the fault is injected: CreateRemoteThread starts a thread in the target at
// an address that cannot be executed, the target takes an access violation on
// its own, and its own unhandled-exception filter runs exactly as it would for
// a genuine fault. Nothing is added to the engine, and the signal comes from
// outside the process — the same posture as kill -SEGV.
//
// Only kernel32 is used (OpenProcess / CreateRemoteThread), which is present on
// every Windows host, so this introduces no new dependency.
package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
	"syscall"
	"unsafe"
)

const (
	// PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION
	// | PROCESS_VM_WRITE | PROCESS_VM_READ — the set CreateRemoteThread needs.
	processAccess = 0x0002 | 0x0400 | 0x0008 | 0x0020 | 0x0010

	th32csSnapProcess = 0x00000002
	maxPath           = 260
)

type processEntry32 struct {
	Size              uint32
	Usage             uint32
	ProcessID         uint32
	DefaultHeapID     uintptr
	ModuleID          uint32
	Threads           uint32
	ParentProcessID   uint32
	PriClassBase      int32
	Flags             uint32
	ExeFile           [maxPath]uint16
}

// findByName returns the NEWEST pid whose image name matches, mirroring the
// `pgrep -n` the POSIX branch of the harness uses. Newest rather than first
// because a stale engine from an earlier run must never be the one faulted —
// the capture would then analyse artefacts from the wrong session.
func findByName(kernel32 *syscall.LazyDLL, name string) (uint32, error) {
	createSnapshot := kernel32.NewProc("CreateToolhelp32Snapshot")
	process32First := kernel32.NewProc("Process32FirstW")
	process32Next := kernel32.NewProc("Process32NextW")
	closeHandle := kernel32.NewProc("CloseHandle")

	snap, _, err := createSnapshot.Call(uintptr(th32csSnapProcess), 0)
	if int(snap) == -1 {
		return 0, fmt.Errorf("CreateToolhelp32Snapshot: %v", err)
	}
	defer closeHandle.Call(snap)

	var entry processEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))

	want := strings.ToLower(name)
	var newest uint32

	ret, _, _ := process32First.Call(snap, uintptr(unsafe.Pointer(&entry)))
	for ret != 0 {
		exe := strings.ToLower(syscall.UTF16ToString(entry.ExeFile[:]))
		if exe == want {
			// Later entries in the snapshot are not ordered by start time, so
			// "newest" is approximated by the highest pid seen. Good enough to
			// separate a fresh launch from a leftover, which is the actual risk.
			if entry.ProcessID > newest {
				newest = entry.ProcessID
			}
		}
		ret, _, _ = process32Next.Call(snap, uintptr(unsafe.Pointer(&entry)))
	}

	if newest == 0 {
		return 0, fmt.Errorf("no process named %q", name)
	}
	return newest, nil
}

func main() {
	pid := flag.Int("pid", 0, "process id to fault")
	name := flag.String("name", "", "image name to fault (newest match), e.g. wired-headless.x64.exe")
	flag.Parse()

	if *pid == 0 && *name == "" {
		fmt.Fprintln(os.Stderr, "usage: win-fault-inject -pid <n> | -name <image.exe>")
		os.Exit(64)
	}

	kernel32 := syscall.NewLazyDLL("kernel32.dll")

	target := uint32(*pid)
	if target == 0 {
		found, err := findByName(kernel32, *name)
		if err != nil {
			fmt.Fprintf(os.Stderr, "win-fault-inject: %v\n", err)
			os.Exit(1)
		}
		target = found
	}

	openProcess := kernel32.NewProc("OpenProcess")
	createRemoteThread := kernel32.NewProc("CreateRemoteThread")
	closeHandle := kernel32.NewProc("CloseHandle")

	h, _, err := openProcess.Call(uintptr(processAccess), 0, uintptr(target))
	if h == 0 {
		fmt.Fprintf(os.Stderr, "win-fault-inject: OpenProcess(%d): %v\n", target, err)
		os.Exit(1)
	}
	defer closeHandle.Call(h)

	// Start a thread at address 0. The target immediately takes an
	// EXCEPTION_ACCESS_VIOLATION trying to execute it — a real fault, raised
	// inside the target process, which its own unhandled-exception filter then
	// handles exactly as it would any other access violation.
	thread, _, err := createRemoteThread.Call(h, 0, 0, 0, 0, 0, 0)
	if thread == 0 {
		fmt.Fprintf(os.Stderr, "win-fault-inject: CreateRemoteThread(%d): %v\n", target, err)
		os.Exit(1)
	}
	closeHandle.Call(thread)

	fmt.Printf("faulted pid %d with an access violation\n", target)
}
