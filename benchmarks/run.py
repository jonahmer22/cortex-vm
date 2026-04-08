#!/usr/bin/env python3
"""
run.py — Cortex-VM benchmarking suite entry point.

Usage:
    python benchmarks/run.py
    python benchmarks/run.py --category alu
    python benchmarks/run.py --no-graphs
    python benchmarks/run.py --repeats 3
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import sys
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bench_core import BenchmarkResult, BenchmarkRunner, VM_BINARY, ASM_DIR
from report import print_table, generate_graphs, RESULTS_DIR

CATEGORIES = ("alu", "mext", "float", "mem", "branch", "realworld", "asm")


def _asm(filename: str) -> str:
    return os.path.join(ASM_DIR, filename)


# ---------------------------------------------------------------------------
# Per-category runners
# ---------------------------------------------------------------------------

def run_alu(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[alu] Integer ALU benchmarks...")
    results = []
    for name, fname, iters in [
        ("addi",    "alu_addi.s",    100_000_000),
        ("add",     "alu_add.s",     100_000_000),
        ("shift",   "alu_shift.s",   100_000_000),
        ("bitwise", "alu_bitwise.s", 100_000_000),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_vm_bench(_asm(fname), name, "alu", iters, "MIPS", repeats)
        results.append(r)
        print(f" {r.metric_value:.1f} MIPS")
    return results


def run_mext(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[mext] M extension benchmarks...")
    results = []
    for name, fname, iters in [
        ("mul", "mext_mul.s", 50_000_000),
        ("div", "mext_div.s", 10_000_000),
        ("rem", "mext_rem.s", 10_000_000),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_vm_bench(_asm(fname), name, "mext", iters, "MIPS", repeats)
        results.append(r)
        print(f" {r.metric_value:.1f} MIPS")
    return results


def run_float(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[float] Float ALU benchmarks...")
    results = []
    for name, fname, iters in [
        ("fadd",  "float_fadd.s",  50_000_000),
        ("fmul",  "float_fmul.s",  50_000_000),
        ("fdiv",  "float_fdiv.s",  10_000_000),
        ("fsqrt", "float_fsqrt.s", 10_000_000),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_vm_bench(_asm(fname), name, "float", iters, "MFLOPS", repeats)
        results.append(r)
        print(f" {r.metric_value:.1f} MFLOPS")
    return results


def run_mem(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[mem] Memory benchmarks...")
    results = []
    for name, fname, iters in [
        ("sequential", "mem_sequential.s", 10_000_000),
        ("stride-64",  "mem_stride.s",     10_000_000),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_vm_bench(_asm(fname), name, "mem", iters, "MIPS", repeats)
        results.append(r)
        print(f" {r.metric_value:.1f} MIPS")
    return results


def run_branch(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[branch] Branch benchmarks...")
    results = []
    for name, fname, iters in [
        ("taken",     "branch_taken.s",    100_000_000),
        ("not-taken", "branch_nottaken.s", 100_000_000),
        ("mixed",     "branch_mixed.s",    100_000_000),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_vm_bench(_asm(fname), name, "branch", iters, "BOPS", repeats)
        results.append(r)
        print(f" {r.metric_value:.1f} BOPS")
    return results


def run_realworld(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[realworld] Real-world benchmarks...")
    results = []
    for name, fname, expected in [
        ("fib(40) iter",   "realworld_fib_iter.s", "102334155"),
        ("fib(30) rec",    "realworld_fib_rec.s",  "832040"),
        ("sieve(1000)",    "realworld_sieve.s",    "168"),
        ("newton sqrt(2)", "realworld_newton.s",   "1.414214"),
    ]:
        print(f"  {name}...", end="", flush=True)
        r = runner.run_realworld(_asm(fname), name, expected, repeats)
        actual = r.actual_out
        ok = "OK" if actual.startswith(expected[:6]) else f"WRONG (got {actual!r})"
        results.append(r)
        print(f" {r.elapsed_ms:.1f} ms  [{ok}]")
    return results


def run_asm_bench(runner: BenchmarkRunner, repeats: int) -> list[BenchmarkResult]:
    print("[asm] Assembler throughput benchmarks...")
    results = []
    for kb in (1, 10, 100):
        print(f"  {kb}KB source...", end="", flush=True)
        r = runner.run_assembler_bench(kb, repeats)
        results.append(r)
        print(f" {r.metric_value:.2f} MB/s")
    return results


CATEGORY_RUNNERS = {
    "alu":       run_alu,
    "mext":      run_mext,
    "float":     run_float,
    "mem":       run_mem,
    "branch":    run_branch,
    "realworld": run_realworld,
    "asm":       run_asm_bench,
}


# ---------------------------------------------------------------------------
# JSON output
# ---------------------------------------------------------------------------

def save_json(results: list[BenchmarkResult]) -> str:
    os.makedirs(RESULTS_DIR, exist_ok=True)
    timestamp = datetime.now().strftime("%Y-%m-%dT%H%M%S")
    path = os.path.join(RESULTS_DIR, f"{timestamp}.json")
    payload = {
        "timestamp": timestamp,
        "host": f"{platform.system()} {platform.machine()}",
        "results": [
            {
                "name":         r.name,
                "category":     r.category,
                "iterations":   r.iterations,
                "elapsed_ms":   r.elapsed_ms,
                "metric_value": r.metric_value,
                "metric_unit":  r.metric_unit,
                "stddev_ms":    r.stddev_ms,
            }
            for r in results
        ],
    }
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)
    return path


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="Cortex-VM benchmark suite")
    parser.add_argument(
        "--category", "-c",
        choices=CATEGORIES,
        default=None,
        help="Run only the specified category (default: all)",
    )
    parser.add_argument(
        "--no-graphs", action="store_true",
        help="Skip generating matplotlib graphs",
    )
    parser.add_argument(
        "--repeats", "-r",
        type=int, default=5,
        help="Timed repetitions per benchmark (default: 5)",
    )
    args = parser.parse_args()

    if not os.path.isfile(VM_BINARY):
        print(f"[error] VM binary not found: {VM_BINARY}")
        print("        Run `make` from the project root first.")
        sys.exit(1)

    runner = BenchmarkRunner()
    results: list[BenchmarkResult] = []

    cats = [args.category] if args.category else list(CATEGORIES)
    for cat in cats:
        results.extend(CATEGORY_RUNNERS[cat](runner, args.repeats))

    print_table(results)

    json_path = save_json(results)
    print(f"[run] Results saved to {json_path}")

    if not args.no_graphs:
        generate_graphs(results)


if __name__ == "__main__":
    main()
