#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Portable process-tree watchdog used by shell integration gates."""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import List, Optional, Sequence


TIMEOUT_EXIT_CODE = 124

WIRED_WIDESCREEN_BINARY_MARKERS = (
    b"Automated window request was not explicit 16:9; using 1280x720",
    b"Window became non-16:9 (logical=%dx%d pixels=%dx%d); refusing it",
    b"Automated window extent fell below 1280x720",
    b"window-extent schema=2 requested=%dx%d logical=%dx%d pixels=%dx%d exact16x9=1 publish-ready=1",
)

WIRED_PROFILE_HOST_WIDESCREEN_BINARY_MARKER = (
    b"RAL_PROFILE_HOST window-extent logical=%dx%d pixels=%dx%d "
    b"exact16x9=1 publish-ready=1"
)


def _wired_client_widescreen_error(command: Sequence[str]) -> Optional[str]:
    """Reject GUI harness launches that do not carry exact 16:9 authority."""
    if not command:
        return None

    cvars = {}
    index = 1
    while index + 2 < len(command):
        if command[index].lower() == "+set":
            cvars[command[index + 1].lower()] = command[index + 2]
            index += 3
            continue
        index += 1

    executable = Path(command[0]).name.lower()
    is_headless = "headless" in executable
    is_profile_host = executable.startswith("wired_profile_host")
    is_wired_gui = executable.startswith("wired") and not is_headless and not is_profile_host
    is_automated_gui = cvars.get("com_automated") == "1" and not is_headless
    if not is_wired_gui and not is_automated_gui:
        return None

    required = {
        "r_fullscreen": "0",
        "r_mode": "-1",
    }
    for name, expected in required.items():
        if cvars.get(name) != expected:
            return f"{name} must be {expected}"

    try:
        width = int(cvars["r_customwidth"])
        height = int(cvars["r_customheight"])
    except (KeyError, ValueError):
        return "r_customwidth/r_customheight must be explicit integers"
    if width <= 0 or height <= 0 or width * 9 != height * 16:
        return f"requested window {width}x{height} is not exact 16:9"
    if width < 1280 or height < 720:
        return f"requested automated window {width}x{height} is below 1280x720"
    return None


def _wired_client_binary_guard_error(
    command: Sequence[str], cwd: Optional[Path] = None
) -> Optional[str]:
    """Reject a stale GUI binary before it can publish a legacy 4:3 window."""
    if not command:
        return None

    executable = Path(command[0])
    if not executable.is_absolute() and cwd is not None:
        executable = cwd / executable
    executable_name = executable.name.lower()
    is_headless = "headless" in executable_name

    cvars = {}
    index = 1
    while index + 2 < len(command):
        if command[index].lower() == "+set":
            cvars[command[index + 1].lower()] = command[index + 2]
            index += 3
            continue
        index += 1
    is_profile_host = executable_name.startswith("wired_profile_host")
    is_wired_gui = executable_name.startswith("wired") and not is_headless and not is_profile_host
    is_automated_gui = cvars.get("com_automated") == "1" and not is_headless
    if is_profile_host:
        try:
            binary = executable.read_bytes()
        except (FileNotFoundError, IsADirectoryError, OSError):
            return None
        if WIRED_PROFILE_HOST_WIDESCREEN_BINARY_MARKER not in binary:
            return "profile host lacks hidden-until-validated 16:9 publication guard"
        return None
    if not is_wired_gui and not is_automated_gui:
        return None
    try:
        binary = executable.read_bytes()
    except (FileNotFoundError, IsADirectoryError, OSError):
        # Let subprocess report an absent executable. Existing launch wrappers
        # are still covered by their explicit extent authority.
        return None
    missing = [marker for marker in WIRED_WIDESCREEN_BINARY_MARKERS
               if marker not in binary]
    if missing:
        return "GUI binary lacks the current hidden-until-validated 16:9 guard"
    return None


def _posix_group_exists(pgid: int) -> bool:
    """Return true while any process still belongs to *pgid*."""
    try:
        os.killpg(pgid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def _reap_leader(process: subprocess.Popen) -> None:
    """Reap the direct child when it has exited, without blocking."""
    process.poll()


def _terminate_posix_group(
    process: subprocess.Popen, pgid: int, kill_after: float
) -> None:
    """TERM, then KILL the original session even if its leader exits first."""
    try:
        os.killpg(pgid, signal.SIGTERM)
    except ProcessLookupError:
        _reap_leader(process)
        return

    deadline = time.monotonic() + kill_after
    while _posix_group_exists(pgid) and time.monotonic() < deadline:
        _reap_leader(process)
        time.sleep(0.02)

    # Do not key escalation on process.wait(): the direct child can exit on TERM
    # while a descendant ignores it. The saved process-group id remains valid and
    # must be killed independently of the leader's lifetime.
    if _posix_group_exists(pgid):
        try:
            os.killpg(pgid, signal.SIGKILL)
        except ProcessLookupError:
            pass

    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        # This should be unreachable after a successful group SIGKILL, but keep
        # the watchdog bounded if a platform reports a stale group/member state.
        process.kill()
        process.wait()


def _taskkill_tree(pid: int) -> bool:
    """Force-kill a Windows process and every descendant known to taskkill."""
    try:
        completed = subprocess.run(
            ["taskkill", "/PID", str(pid), "/T", "/F"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except OSError:
        return False
    return completed.returncode == 0


def _terminate_windows_tree(process: subprocess.Popen) -> None:
    # CREATE_NEW_PROCESS_GROUP alone does not provide ownership semantics:
    # terminate()/kill() affect only the group leader. taskkill /T /F walks and
    # forcefully terminates the descendant tree before the parent pid disappears.
    # If taskkill is unavailable, retain the best-effort direct-child fallback.
    if not _taskkill_tree(process.pid) and process.poll() is None:
        process.kill()
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_command(
    command: Sequence[str],
    cwd: Path,
    stdout_path: Path,
    timeout: float,
    kill_after: float,
) -> int:
    widescreen_error = _wired_client_widescreen_error(command)
    if widescreen_error is not None:
        raise ValueError(
            "refusing Wired GUI harness before spawn: " + widescreen_error
        )
    binary_guard_error = _wired_client_binary_guard_error(command, cwd)
    if binary_guard_error is not None:
        raise ValueError(
            "refusing Wired GUI harness before spawn: " + binary_guard_error
        )

    creationflags = 0
    start_new_session = False
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        start_new_session = True

    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    with stdout_path.open("wb") as output:
        process = subprocess.Popen(
            list(command),
            cwd=str(cwd),
            stdout=output,
            stderr=subprocess.STDOUT,
            creationflags=creationflags,
            start_new_session=start_new_session,
        )
        pgid: Optional[int] = None
        if os.name != "nt":
            # Capture this while the leader certainly exists. On timeout the
            # leader may exit immediately on TERM, but descendants retain pgid.
            pgid = os.getpgid(process.pid)
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                _terminate_windows_tree(process)
            else:
                assert pgid is not None
                _terminate_posix_group(process, pgid, kill_after)
            print(
                f"timeout: command exceeded {timeout:g}s: {command[0]}",
                file=sys.stderr,
            )
            return TIMEOUT_EXIT_CODE


def _pid_is_live(pid: int) -> bool:
    """Cross-platform self-test probe; zombies do not count as live."""
    if os.name == "nt":
        completed = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
            text=True,
        )
        return completed.returncode == 0 and f'"{pid}"' in completed.stdout

    proc_stat = Path(f"/proc/{pid}/stat")
    try:
        # Linux: a killed orphan can briefly remain as a zombie under a slow
        # container init. It is no longer executable work and cannot leak.
        fields = proc_stat.read_text(encoding="ascii").split()
        if len(fields) > 2 and fields[2] == "Z":
            return False
    except (FileNotFoundError, OSError, UnicodeError):
        pass
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True

    # BSD/macOS has no /proc. Ask ps whether the extant pid is only a zombie.
    try:
        state = subprocess.check_output(
            ["ps", "-o", "stat=", "-p", str(pid)],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return False
    return bool(state) and not state.startswith("Z")


def _self_test() -> int:
    failures: List[str] = []
    good_gui = [
        "/tmp/wired.arm64",
        "+set", "r_fullscreen", "0",
        "+set", "r_mode", "-1",
        "+set", "r_customwidth", "1280",
        "+set", "r_customheight", "720",
    ]
    bad_gui = list(good_gui)
    bad_gui[-4] = "640"
    bad_gui[-1] = "480"
    if _wired_client_widescreen_error(good_gui) is not None:
        failures.append("exact-16:9 Wired client launch was rejected")
    if _wired_client_widescreen_error(bad_gui) is None:
        failures.append("640x480-equivalent Wired client launch was accepted")
    small_widescreen_gui = list(good_gui)
    small_widescreen_gui[-4] = "640"
    small_widescreen_gui[-1] = "360"
    if _wired_client_widescreen_error(small_widescreen_gui) is None:
        failures.append("sub-1280x720 automated Wired client launch was accepted")
    renamed_bad_gui = list(bad_gui)
    renamed_bad_gui[0] = "/tmp/client-under-test"
    renamed_bad_gui[1:1] = ["+set", "com_automated", "1"]
    if _wired_client_widescreen_error(renamed_bad_gui) is None:
        failures.append("renamed automated 640x480 client launch was accepted")
    if _wired_client_widescreen_error(["/tmp/wired", "+set", "r_fullscreen", "0"]) is None:
        failures.append("incomplete Wired client extent authority was accepted")
    if _wired_client_widescreen_error(["/tmp/wired-headless"]) is not None:
        failures.append("headless Wired process was treated as a GUI client")
    with tempfile.TemporaryDirectory(prefix="wired-timeout-selftest-") as temp:
        root = Path(temp)
        pid_file = root / "descendant.pid"
        output = root / "timeout.stdout"

        guarded_gui = root / "wired-guarded"
        guarded_gui.write_bytes(b"\0".join(WIRED_WIDESCREEN_BINARY_MARKERS))
        stale_gui = root / "wired-stale"
        stale_gui.write_bytes(b"legacy mode-3 fallback")
        profile_host = root / "wired_profile_host"
        profile_host.write_bytes(WIRED_PROFILE_HOST_WIDESCREEN_BINARY_MARKER)
        stale_profile_host = root / "wired_profile_host_stale"
        stale_profile_host.write_bytes(b"legacy profile host window")
        guarded_command = [str(guarded_gui), *good_gui[1:]]
        stale_command = [str(stale_gui), *good_gui[1:]]
        if _wired_client_binary_guard_error(guarded_command) is not None:
            failures.append("current widescreen-guarded GUI binary was rejected")
        if _wired_client_binary_guard_error(stale_command) is None:
            failures.append("stale GUI binary without pre-show 16:9 guards was accepted")
        if _wired_client_widescreen_error([str(profile_host), "--frames", "1"]) is not None:
            failures.append("profile host was misclassified as a cvar-driven client")
        if _wired_client_binary_guard_error([str(profile_host)]) is not None:
            failures.append("current widescreen-guarded profile host was rejected")
        if _wired_client_binary_guard_error([str(stale_profile_host)]) is None:
            failures.append("stale profile host without pre-show guard was accepted")

        if os.name == "nt":
            child_code = "import time; time.sleep(60)"
        else:
            child_code = (
                "import signal,time; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "time.sleep(60)"
            )
        parent_code = (
            "import pathlib,subprocess,sys,time; "
            f"child=subprocess.Popen([sys.executable,'-c',{child_code!r}]); "
            f"pathlib.Path({str(pid_file)!r}).write_text(str(child.pid),encoding='ascii'); "
            "time.sleep(60)"
        )

        started = time.monotonic()
        rc = run_command(
            [sys.executable, "-c", parent_code],
            root,
            output,
            timeout=0.5,
            kill_after=0.25,
        )
        elapsed = time.monotonic() - started
        if rc != TIMEOUT_EXIT_CODE:
            failures.append(f"timeout returned {rc}, expected {TIMEOUT_EXIT_CODE}")
        if elapsed > 8.0:
            failures.append(f"timeout teardown was not bounded ({elapsed:.2f}s)")
        try:
            descendant_pid = int(pid_file.read_text(encoding="ascii"))
        except (FileNotFoundError, OSError, ValueError) as exc:
            failures.append(f"descendant pid evidence missing: {exc}")
        else:
            deadline = time.monotonic() + 3.0
            while _pid_is_live(descendant_pid) and time.monotonic() < deadline:
                time.sleep(0.05)
            if _pid_is_live(descendant_pid):
                failures.append(f"descendant {descendant_pid} survived timeout teardown")

    if failures:
        for failure in failures:
            print(f"SELF-TEST FAIL: {failure}", file=sys.stderr)
        return 1
    print(
        "SELF-TEST PASS: timeout returned 124 and descendant teardown completed"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--timeout", type=float)
    parser.add_argument("--kill-after", type=float, default=15.0)
    parser.add_argument("--cwd", type=Path)
    parser.add_argument("--stdout", type=Path)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.self_test:
        if any(
            value is not None
            for value in (args.timeout, args.cwd, args.stdout)
        ) or args.command:
            parser.error("--self-test does not accept run arguments")
        return _self_test()

    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if (
        not command
        or args.timeout is None
        or args.timeout <= 0
        or args.kill_after < 0
        or args.cwd is None
        or args.stdout is None
    ):
        parser.error(
            "--cwd, --stdout, a command, and a positive --timeout are required"
        )
    try:
        return run_command(
            command,
            args.cwd,
            args.stdout,
            args.timeout,
            args.kill_after,
        )
    except ValueError as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
