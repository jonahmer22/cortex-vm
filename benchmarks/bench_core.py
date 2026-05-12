"""
bench_core.py — BenchmarkResult dataclass and BenchmarkRunner.

Timing strategy:
  - For VM workloads (categories 1–6): the assembly program itself calls
    SYS_TIME_GET before and after the hot loop and prints elapsed milliseconds.
    Python reads that integer from stdout and computes the metric.
  - For assembler throughput (category 7): Python uses time.perf_counter()
    around the subprocess call.
"""

from __future__ import annotations

import os
import statistics
import subprocess
import tempfile
import time
from dataclasses import dataclass, field

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VM_BINARY    = os.path.join(PROJECT_ROOT, "cortex")
ASM_DIR      = os.path.join(os.path.dirname(os.path.abspath(__file__)), "asm")


@dataclass
class BenchmarkResult:
    name:         str
    category:     str          # "alu" | "mext" | "float" | "mem" | "branch" | "realworld" | "asm"
    iterations:   int          # number of operations measured
    elapsed_ms:   float        # median elapsed time across repeats
    metric_value: float        # MIPS / MFLOPS / BOPS / ops/s / MB/s
    metric_unit:  str          # "MIPS" / "MFLOPS" / "BOPS" / "ms" / "MB/s"
    stddev_ms:    float = 0.0
    raw_ms:       list[float] = field(default_factory=list)
    # For real-world programs: optional correctness info
    expected_out: str = ""
    actual_out:   str = ""


class BenchmarkRunner:
    """Assembles and runs a Cortex-VM assembly benchmark, returning metrics."""

    def __init__(self, vm_binary: str = VM_BINARY, timeout: int = 120):
        self.vm_binary = vm_binary
        self.timeout   = timeout

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def run_vm_bench(
        self,
        asm_path: str,
        name: str,
        category: str,
        iterations: int,
        metric_unit: str,
        repeats: int = 5,
    ) -> BenchmarkResult:
        """
        Run a VM workload, timing it from Python with time.perf_counter().
        Assembles once, executes `repeats` times, takes median elapsed ms.
        """
        with tempfile.NamedTemporaryFile(suffix=".out", delete=False) as tf:
            binary_path = tf.name

        try:
            # Assemble once
            self._assemble(asm_path, binary_path)

            raw_ms: list[float] = []
            for _ in range(repeats):
                t0 = time.perf_counter()
                result = subprocess.run(
                    [self.vm_binary, binary_path],
                    capture_output=True, text=True, timeout=self.timeout,
                )
                elapsed_ms = (time.perf_counter() - t0) * 1000.0
                if result.returncode != 0:
                    raise RuntimeError(
                        f"VM crashed (rc={result.returncode}) running {asm_path}\n"
                        f"stderr: {result.stderr}"
                    )
                raw_ms.append(elapsed_ms)
        finally:
            if os.path.exists(binary_path):
                os.unlink(binary_path)

        median_ms = statistics.median(raw_ms)
        stddev    = statistics.stdev(raw_ms) if len(raw_ms) > 1 else 0.0
        metric    = self._compute_metric(iterations, median_ms, metric_unit)

        return BenchmarkResult(
            name=name,
            category=category,
            iterations=iterations,
            elapsed_ms=median_ms,
            metric_value=metric,
            metric_unit=metric_unit,
            stddev_ms=stddev,
            raw_ms=raw_ms,
        )

    def run_realworld(
        self,
        asm_path: str,
        name: str,
        expected_out: str,
        repeats: int = 5,
    ) -> BenchmarkResult:
        """
        Run a real-world program; measure wall-clock time from Python.
        Reports elapsed ms (median). Does NOT use internal VM timing.
        """
        with tempfile.NamedTemporaryFile(suffix=".out", delete=False) as tf:
            binary_path = tf.name

        try:
            self._assemble(asm_path, binary_path)

            raw_ms: list[float] = []
            actual_out = ""
            for i in range(repeats):
                t0 = time.perf_counter()
                result = subprocess.run(
                    [self.vm_binary, binary_path],
                    capture_output=True, text=True, timeout=self.timeout,
                )
                elapsed_ms = (time.perf_counter() - t0) * 1000.0
                if result.returncode != 0:
                    raise RuntimeError(
                        f"VM crashed (rc={result.returncode}) running {asm_path}\n"
                        f"stderr: {result.stderr}"
                    )
                raw_ms.append(elapsed_ms)
                if i == 0:
                    actual_out = result.stdout.strip()
        finally:
            if os.path.exists(binary_path):
                os.unlink(binary_path)

        median_ms = statistics.median(raw_ms)
        stddev    = statistics.stdev(raw_ms) if len(raw_ms) > 1 else 0.0

        return BenchmarkResult(
            name=name,
            category="realworld",
            iterations=0,
            elapsed_ms=median_ms,
            metric_value=median_ms,
            metric_unit="ms",
            stddev_ms=stddev,
            raw_ms=raw_ms,
            expected_out=expected_out,
            actual_out=actual_out,
        )

    def run_assembler_bench(
        self,
        source_kb: int,
        repeats: int = 5,
    ) -> BenchmarkResult:
        """
        Measure assembler throughput: generate a source file of `source_kb` KB,
        assemble it via subprocess, report MB/s.
        """
        source = self._generate_asm_source(source_kb * 1024)
        source_bytes = len(source.encode())

        with tempfile.NamedTemporaryFile(mode="w", suffix=".s", delete=False) as sf:
            sf.write(source)
            src_path = sf.name

        try:
            raw_ms: list[float] = []
            for _ in range(repeats):
                with tempfile.NamedTemporaryFile(suffix=".out", delete=False) as tf:
                    out_path = tf.name
                try:
                    t0 = time.perf_counter()
                    result = subprocess.run(
                        [self.vm_binary, "-a", src_path, "-o", out_path],
                        capture_output=True, text=True, timeout=self.timeout,
                    )
                    elapsed_ms = (time.perf_counter() - t0) * 1000.0
                    if result.returncode != 0:
                        raise RuntimeError(
                            f"Assembler failed:\n{result.stderr}"
                        )
                    raw_ms.append(elapsed_ms)
                finally:
                    if os.path.exists(out_path):
                        os.unlink(out_path)
        finally:
            if os.path.exists(src_path):
                os.unlink(src_path)

        median_ms = statistics.median(raw_ms)
        stddev    = statistics.stdev(raw_ms) if len(raw_ms) > 1 else 0.0
        mb_per_s  = (source_bytes / 1024 / 1024) / (median_ms / 1000.0)

        return BenchmarkResult(
            name=f"assemble_{source_kb}KB",
            category="asm",
            iterations=source_bytes,
            elapsed_ms=median_ms,
            metric_value=mb_per_s,
            metric_unit="MB/s",
            stddev_ms=stddev,
            raw_ms=raw_ms,
        )

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _assemble(self, asm_path: str, binary_path: str) -> None:
        result = subprocess.run(
            [self.vm_binary, "-a", asm_path, "-o", binary_path],
            capture_output=True, text=True, timeout=30,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"Assembly of {asm_path} failed:\n{result.stderr}"
            )

    @staticmethod
    def _compute_metric(iterations: int, elapsed_ms: float, unit: str) -> float:
        if elapsed_ms <= 0:
            return 0.0
        elapsed_s = elapsed_ms / 1000.0
        ops_per_s = iterations / elapsed_s
        if unit in ("MIPS", "MFLOPS", "BOPS"):
            return ops_per_s / 1_000_000
        return ops_per_s  # generic ops/s

    @staticmethod
    def _generate_asm_source(target_bytes: int) -> str:
        """
        Generate a valid assembly source of approximately `target_bytes` bytes.
        Uses a tight addi loop so the program is syntactically valid.
        """
        header = (
            "main:\n"
            "    addi t0, zero, 0\n"
            "    addi t1, zero, 1000\n"
            "loop:\n"
        )
        line   = "    addi t0, t0, 1\n"
        footer = (
            "    blt t0, t1, loop\n"
            "    addi a0, zero, 0\n"
            "    addi a13, zero, 0\n"
            "    syscall\n"
        )
        body_needed = max(0, target_bytes - len(header) - len(footer))
        reps = max(1, body_needed // len(line))
        return header + line * reps + footer
