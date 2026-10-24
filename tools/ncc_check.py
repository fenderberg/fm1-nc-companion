#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run one NCC host check, keep its full log and preserve failures before filtering output."""
import argparse
from pathlib import Path
import platform
import re
import subprocess
import sys


def mac_cpu_noise(output, status, system):
    """Only the two documented Mac baseline discrepancies, within a bounded margin, may pass as notes."""
    if status != 1 or system != "Darwin" or re.search(r"\b(?:FAIL|FAILED|CRASH|ERROR)\b|runtime error:|Sanitizer", output):
        return False
    summary = re.findall(
        r"^regress: \d+ golden renders \(0 changed, 0 gone\), 0 health failures, "
        r"0 voice / routing checks failed, ([12]) CPU entries over budget \(\d+ notes\), 0 crashes;[^\n]*$",
        output, re.M)
    over = [line for line in output.splitlines() if "CPU OVER BUDGET" in line]
    if len(summary) != 1 or len(over) != int(summary[0]):
        return False
    seen = set()
    for line in over:
        item = re.fullmatch(r"regress: CPU OVER BUDGET  (\S+): .* \(\+(\d+) %, limit \+25 %\)", line)
        if not item:
            return False
        name, percent = item.groups()
        if name not in {"cpu/ANALOG/05_SINE_KEY", "cpu/PHASE/00_BRASS"} or name in seen or int(percent) > 35:
            return False
        seen.add(name)
    return True


def run_check(command, log, summary, prefix="", allow_mac_cpu_noise=False):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = result.stdout
    Path(log).write_text(output)
    noise = allow_mac_cpu_noise and mac_cpu_noise(output, result.returncode, platform.system())
    lines = [line for line in output.splitlines() if re.search(summary, line)]
    if (result.returncode and not noise) or not lines:
        print(output, end="" if output.endswith("\n") else "\n")
        print(f"check failed: {command[0]} (exit {result.returncode}; full log: {log})", file=sys.stderr)
        return result.returncode if result.returncode > 0 else 1
    if noise:
        for line in output.splitlines():
            if "CPU OVER BUDGET" in line:
                print(prefix + line)
        print(prefix + "known Mac CPU baseline differences accepted (only these presets, at most +35 %)")
    for line in lines:
        print(prefix + line)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--prefix", default="")
    parser.add_argument("--mac-cpu-noise", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a check command is required after --")
    return run_check(command, args.log, args.summary, args.prefix, args.mac_cpu_noise)


if __name__ == "__main__":
    sys.exit(main())
