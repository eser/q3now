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
    with tempfile.TemporaryDirectory(prefix="wired-timeout-selftest-") as temp:
        root = Path(temp)
        pid_file = root / "descendant.pid"
        output = root / "timeout.stdout"

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
    return run_command(
        command,
        args.cwd,
        args.stdout,
        args.timeout,
        args.kill_after,
    )


if __name__ == "__main__":
    raise SystemExit(main())
