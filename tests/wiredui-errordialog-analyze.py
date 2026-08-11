#!/usr/bin/env python3
"""Strict lifecycle analyzer and defect fixtures for the WiredUI error gate."""

import argparse
import copy
import json
import re
import sys
import tempfile
from pathlib import Path


def load_jsonl(path):
    rows = []
    malformed = 0
    with open(path, encoding="utf-8", errors="replace") as stream:
        for raw in stream:
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except ValueError:
                malformed += 1
                continue
            if not isinstance(row, dict):
                malformed += 1
            else:
                rows.append(row)
    return rows, malformed


def lifecycle_failures(log_path, layout_path, payload):
    failures = []
    try:
        rows, malformed = load_jsonl(log_path)
    except OSError:
        return ["log-unreadable"]
    if malformed:
        failures.append("log-malformed")

    expected_error = "Error: " + payload
    probe_errors = [row for row in rows
                    if str(row.get("sev", "")).upper() == "ERROR"
                    and str(row.get("msg", "")).strip() == expected_error]
    if len(probe_errors) != 2:
        failures.append("probe-error-count")
    bad_severity = [row for row in rows
                    if (str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
                        and not (str(row.get("sev", "")).upper() == "ERROR"
                                 and str(row.get("msg", "")).strip() == expected_error))
                    or (str(row.get("sev", "")).upper() == "WARN"
                        and str(row.get("cat", "")).lower() == "ui")]
    if bad_severity:
        failures.append("unexpected-severity")

    messages = [str(row.get("msg", "")) for row in rows]
    steps = [
        ("first-popup", rf"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{re.escape(payload)}'"),
        ("back-main", r"WiredUI: pop menu \(depth 1\)"),
        ("back-dispatch", r"wui_menu_nav: K_ESCAPE dispatched"),
        ("back-clear", r'"com_errorMessage" is:"(?:\^7)?"'),
        ("arena1-first", r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena1(?:\.bsp)?(?:\s|$)"),
        ("second-popup", rf"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{re.escape(payload)}'"),
        ("arena7-first", r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?(?:\s|$)"),
        ("arena7-clear", r'"com_errorMessage" is:"(?:\^7)?"'),
    ]
    cursor = 0
    for code, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            failures.append(code)
            break

    try:
        layout, layout_malformed = load_jsonl(layout_path)
    except OSError:
        return failures + ["layout-unreadable"]
    if layout_malformed:
        failures.append("layout-malformed")
    frames = {}
    menus = {}
    for row in layout:
        frame = row.get("frame", 0)
        frames.setdefault(frame, False)
        menus.setdefault(frame, set())
        if row.get("menu"):
            menus[frame].add(str(row.get("menu")))
        if row.get("menu") == "error_popup" or row.get("region") == "error_popup":
            frames[frame] = True
    order = sorted(frames)
    shown = [frame for frame in order if frames[frame]]
    if not shown:
        failures.append("popup-absent")
    segments = []
    for frame in shown:
        if not segments or frame > segments[-1][-1] + 1:
            segments.append([frame])
        else:
            segments[-1].append(frame)
    separated = (len(segments) >= 2 and
                 any(not frames[frame] and "main" in menus[frame]
                     for frame in order
                     if segments[0][-1] < frame < segments[1][0]))
    if not separated:
        failures.append("popup-separation")
    if not order or frames[order[-1]]:
        failures.append("final-popup")
    return failures


def analyze(log_path, layout_path, payload):
    failures = lifecycle_failures(log_path, layout_path, payload)
    if failures:
        print("  FAIL lifecycle-analyzer: " + ",".join(failures))
        return 1
    print("  PASS lifecycle-analyzer: strict trace/severity + two popup cohorts + final clear")
    return 0


def write_jsonl(path, rows):
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def self_test():
    payload = "probe_error_payload_12345"
    error = {"sev": "ERROR", "cat": "system", "msg": "Error: " + payload}
    clean_log = [
        copy.deepcopy(error),
        {"sev": "INFO", "cat": "ui", "msg": f"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{payload}'"},
        {"sev": "DEBUG", "cat": "ui", "msg": "WiredUI: pop menu (depth 1)"},
        {"sev": "DEBUG", "cat": "ui", "msg": "wui_menu_nav: K_ESCAPE dispatched"},
        {"sev": "INFO", "cat": "system", "msg": '"com_errorMessage" is:""'},
        {"sev": "INFO", "cat": "client", "msg": "FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp"},
        copy.deepcopy(error),
        {"sev": "INFO", "cat": "ui", "msg": f"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{payload}'"},
        {"sev": "INFO", "cat": "client", "msg": "FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp"},
        {"sev": "INFO", "cat": "system", "msg": '"com_errorMessage" is:""'},
    ]
    clean_layout = [
        {"frame": 1, "menu": "error_popup", "region": "error_popup"},
        {"frame": 2, "menu": "main", "region": "main"},
        {"frame": 3, "menu": "error_popup", "region": "error_popup"},
        {"frame": 4, "menu": "main", "region": "main"},
    ]
    defects = {}
    missing_back = [row for row in clean_log if row.get("msg") != "WiredUI: pop menu (depth 1)"]
    defects["missing-back"] = (missing_back, clean_layout, None)
    server_only = copy.deepcopy(clean_log)
    server_only[5]["msg"] = "Server: arena1"
    defects["server-not-first"] = (server_only, clean_layout, None)
    bad_severity = copy.deepcopy(clean_log)
    bad_severity.append({"sev": "FATAL", "cat": "renderer", "msg": "boom"})
    defects["unexpected-fatal"] = (bad_severity, clean_layout, None)
    one_cohort = [row for row in clean_layout if row["frame"] != 3]
    defects["one-popup-cohort"] = (clean_log, one_cohort, None)
    no_main_gap = copy.deepcopy(clean_layout)
    no_main_gap[1] = {"frame": 2, "menu": "loading", "region": "loading"}
    defects["no-main-gap"] = (clean_log, no_main_gap, None)
    final_popup = copy.deepcopy(clean_layout)
    final_popup[-1] = {"frame": 4, "menu": "error_popup", "region": "error_popup"}
    defects["final-popup"] = (clean_log, final_popup, None)
    malformed = (clean_log, clean_layout, "log")
    defects["malformed-json"] = malformed

    result = 0
    with tempfile.TemporaryDirectory(prefix="wired-error-analyze-") as temp:
        root = Path(temp)
        log_path = root / "log.jsonl"
        layout_path = root / "layout.jsonl"
        write_jsonl(log_path, clean_log)
        write_jsonl(layout_path, clean_layout)
        if lifecycle_failures(log_path, layout_path, payload):
            print("SELF-TEST FAIL: clean fixture rejected")
            result = 1
        for name, (log_rows, layout_rows, malformed_kind) in defects.items():
            write_jsonl(log_path, log_rows)
            write_jsonl(layout_path, layout_rows)
            if malformed_kind == "log":
                with log_path.open("a", encoding="utf-8") as stream:
                    stream.write("{broken\n")
            failures = lifecycle_failures(log_path, layout_path, payload)
            if not failures:
                print(f"SELF-TEST FAIL: defect accepted: {name}")
                result = 1
            else:
                print(f"  rejected {name}: {','.join(failures)}")
    if result == 0:
        print("SELF-TEST PASS: clean lifecycle accepted; seven defects rejected")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--lifecycle", nargs=3, metavar=("LOG", "LAYOUT", "PAYLOAD"))
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.lifecycle:
        return analyze(*args.lifecycle)
    parser.error("choose --self-test or --lifecycle LOG LAYOUT PAYLOAD")
    return 2


if __name__ == "__main__":
    sys.exit(main())
