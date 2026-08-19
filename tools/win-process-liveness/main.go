// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// win-process-liveness — report what Windows actually says when you ask whether
// a process is still alive.
//
// Usage:
//
//	win-process-liveness -pid 1234
//	win-process-liveness -pid 1234 -access synchronize   (reproduce the old bug)
//
// WHY THIS EXISTS
// "Is that pid still running?" looks like a one-line question on Windows and is
// not, because the two calls involved fail in ways that read as the opposite of
// what they mean. This tool prints the raw answers — OpenProcess success, the
// GetLastError behind a failure, the GetExitCodeProcess return and exit code —
// so the question is settled by measurement rather than by what the API docs
// are assumed to say.
//
// It was written to check a liveness test in sentry-native's crash daemon
// (patches/sentry-native/01-chain-previous-crash-handler.patch). That check
// asked for SYNCHRONIZE only, which is NOT enough for GetExitCodeProcess: the
// call failed with ERROR_ACCESS_DENIED, the daemon read the failure as "parent
// gone", finalized the crash session early, and a real crash arriving afterwards
// produced no minidump at all. The fix asks for
// SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION.
//
// MEASURED, on Windows 11 (-access both, the default):
//
//	live process      OpenProcess OK, ret=1, exit_code=259 (STILL_ACTIVE) -> alive
//	exited process    OpenProcess FAILS, ERROR_INVALID_PARAMETER          -> dead
//	unused pid        OpenProcess FAILS, ERROR_INVALID_PARAMETER          -> dead
//
// With -access synchronize, the exited case instead reports OpenProcess OK and
// GetExitCodeProcess failing with ERROR_ACCESS_DENIED — the failure mode the
// patch exists to prevent. Keeping that reproducible is the point of the flag.
//
// One caveat worth knowing, because it produced a misleading reading once: a
// process object survives while ANY handle to it is open, so a pid whose process
// has exited can still be opened for as long as something (a debugger, a crash
// daemon, another probe) holds a handle. A single "OpenProcess succeeded" is
// therefore not proof the process is running — which is exactly why the exit
// code, not the open, has to decide.
package main

import (
	"flag"
	"fmt"
	"os"
	"syscall"
	"unsafe"
)

const (
	synchronize                    = 0x00100000
	processQueryLimitedInformation = 0x00001000
	stillActive                    = 259

	// Not exported by the syscall package; from winerror.h. This is the true
	// counterpart of POSIX ESRCH — the id names no process at all.
	errorInvalidParameter = syscall.Errno(87)
)

func main() {
	pid := flag.Int("pid", 0, "process id to query")
	access := flag.String("access", "both",
		"rights to open with: \"both\" (SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION) "+
			"or \"synchronize\" (SYNCHRONIZE only, reproduces the pre-fix behaviour)")
	flag.Parse()

	if *pid == 0 {
		fmt.Fprintln(os.Stderr, "usage: win-process-liveness -pid <n> [-access both|synchronize]")
		os.Exit(64)
	}

	var rights uintptr
	switch *access {
	case "both":
		rights = synchronize | processQueryLimitedInformation
	case "synchronize":
		rights = synchronize
	default:
		fmt.Fprintf(os.Stderr, "unknown -access %q (want \"both\" or \"synchronize\")\n", *access)
		os.Exit(64)
	}

	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	openProcess := kernel32.NewProc("OpenProcess")
	getExitCodeProcess := kernel32.NewProc("GetExitCodeProcess")
	closeHandle := kernel32.NewProc("CloseHandle")

	handle, _, openErr := openProcess.Call(rights, 0, uintptr(*pid))
	if handle == 0 {
		// ERROR_INVALID_PARAMETER here is the true counterpart of POSIX ESRCH:
		// no process carries that id. Any other error means the process exists
		// but is not open to us, which is not the same as being gone.
		fmt.Printf("pid %d: OpenProcess(%s) FAILED: %v\n", *pid, *access, openErr)
		fmt.Printf("  verdict: %s\n", verdictFromOpenError(openErr))
		os.Exit(0)
	}
	defer closeHandle.Call(handle)

	var exitCode uint32
	ret, _, gecErr := getExitCodeProcess.Call(handle, uintptr(unsafe.Pointer(&exitCode)))

	fmt.Printf("pid %d: OpenProcess(%s) OK\n", *pid, *access)
	if ret == 0 {
		// A failed query is not an answer of "exited" — it is no answer at all.
		fmt.Printf("  GetExitCodeProcess FAILED: %v\n", gecErr)
		fmt.Printf("  verdict: UNKNOWN (cannot read exit status; treat as alive)\n")
		return
	}
	fmt.Printf("  GetExitCodeProcess ret=1 exit_code=%d\n", exitCode)
	if exitCode == stillActive {
		fmt.Printf("  verdict: ALIVE (STILL_ACTIVE)\n")
	} else {
		fmt.Printf("  verdict: EXITED (code %d)\n", exitCode)
	}
}

func verdictFromOpenError(err error) string {
	if errno, ok := err.(syscall.Errno); ok && errno == errorInvalidParameter {
		return "DEAD (ERROR_INVALID_PARAMETER: no process carries that id)"
	}
	return "ALIVE (the process exists but is not open to us)"
}
