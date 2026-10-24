#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The NCC check runner must preserve failures even when a success summary was printed."""
from contextlib import redirect_stdout, redirect_stderr
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ncc_check import mac_cpu_noise, run_check


MAC_LOG = (
    "regress: CPU OVER BUDGET  cpu/ANALOG/05_SINE_KEY: 928 instructions / sample over the idle mix, "
    "baseline 734 (+26 %, limit +25 %)\n"
    "regress: CPU OVER BUDGET  cpu/PHASE/00_BRASS: 797 instructions / sample over the idle mix, "
    "baseline 616 (+29 %, limit +25 %)\n"
    "regress: 113 golden renders (0 changed, 0 gone), 0 health failures, 0 voice / routing checks failed, "
    "2 CPU entries over budget (0 notes), 0 crashes; 1.0 s\n"
)


class CheckRunnerTest(unittest.TestCase):
    def run_child(self, output, status, summary="^ncc: all ok$", noise=False, system="Darwin"):
        stdout, stderr = io.StringIO(), io.StringIO()
        command = [sys.executable, "-c", "import sys; print(sys.argv[1], end=''); sys.exit(int(sys.argv[2]))",
                   output, str(status)]
        with tempfile.TemporaryDirectory() as tmp, redirect_stdout(stdout), redirect_stderr(stderr), \
                patch("ncc_check.platform.system", return_value=system):
            log = Path(tmp) / "check.log"
            result = run_check(command, log, summary, "(test) ", noise)
            self.assertEqual(log.read_text(), output)
        return result, stdout.getvalue(), stderr.getvalue()

    def test_success_keeps_full_log_and_prints_summary_only(self):
        result, stdout, stderr = self.run_child("details\nncc: all ok\n", 0)
        self.assertEqual((result, stdout, stderr), (0, "(test) ncc: all ok\n", ""))

    def test_failure_is_not_hidden_by_success_summary(self):
        for status in (1, 2, 139):
            with self.subTest(status=status):
                result, stdout, stderr = self.run_child("FAIL: details\nncc: all ok\n", status)
                self.assertEqual(result, status)
                self.assertIn("FAIL: details", stdout)
                self.assertIn(f"exit {status}", stderr)

    def test_missing_success_summary_fails(self):
        self.assertNotEqual(self.run_child("incomplete output\n", 0)[0], 0)

    def test_signal_fails(self):
        with tempfile.TemporaryDirectory() as tmp, redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()), \
                patch("ncc_check.subprocess.run", return_value=subprocess.CompletedProcess([], -11, "ncc: all ok\n")):
            self.assertEqual(run_check(["check"], Path(tmp) / "check.log", "all ok"), 1)

    def test_cpu_exception_requires_explicit_option_and_mac(self):
        summary = "^regress: [0-9]+ golden renders"
        self.assertEqual(self.run_child(MAC_LOG, 1, summary, noise=True)[0], 0)
        self.assertEqual(self.run_child(MAC_LOG, 1, summary, noise=False)[0], 1)
        self.assertEqual(self.run_child(MAC_LOG, 1, summary, noise=True, system="Linux")[0], 1)

    def test_cpu_exception_never_accepts_other_failures(self):
        for text in (
            MAC_LOG.replace("0 changed", "1 changed"),
            MAC_LOG.replace("0 gone", "1 gone"),
            MAC_LOG.replace("0 health failures", "1 health failures"),
            MAC_LOG.replace("0 voice / routing checks failed", "1 voice / routing checks failed"),
            MAC_LOG.replace("0 crashes", "1 crashes"),
            MAC_LOG.replace("cpu/PHASE/00_BRASS", "cpu/FM6/00_TEST"),
            MAC_LOG.replace("+29 %", "+36 %"),
            MAC_LOG.replace("2 CPU entries", "1 CPU entries"),
            MAC_LOG.replace("cpu/PHASE/00_BRASS", "cpu/ANALOG/05_SINE_KEY"),
            MAC_LOG + "regress: other check FAIL\n",
            MAC_LOG + "runtime error: invalid memory access\n",
            MAC_LOG + "SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior\n",
            MAC_LOG + MAC_LOG.splitlines()[-1] + "\n",
            "",
        ):
            with self.subTest(output=text):
                self.assertFalse(mac_cpu_noise(text, 1, "Darwin"))
        self.assertFalse(mac_cpu_noise(MAC_LOG, 2, "Darwin"))
        self.assertFalse(mac_cpu_noise(MAC_LOG, -11, "Darwin"))


if __name__ == "__main__":
    unittest.main()
