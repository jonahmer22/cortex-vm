"""
benchmarks/conftest.py — pytest fixtures and performance-floor tests.

Run with:
    pytest benchmarks/ -m benchmark

Each test assembles and runs a benchmark once and asserts a minimum
performance floor. Floors are intentionally conservative so they pass
on a wide range of hardware.
"""

from __future__ import annotations

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bench_core import BenchmarkRunner, ASM_DIR

VM_BINARY = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "cortex"
)


# ---------------------------------------------------------------------------
# Session fixture: skip everything if the binary isn't built
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session", autouse=True)
def require_binary():
    if not os.path.isfile(VM_BINARY):
        pytest.skip(
            f"cortex binary not found at {VM_BINARY}. Run `make` first."
        )


@pytest.fixture(scope="session")
def runner():
    return BenchmarkRunner(timeout=120)


def _asm(filename: str) -> str:
    return os.path.join(ASM_DIR, filename)


# ---------------------------------------------------------------------------
# ALU performance floors
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_addi_throughput(runner):
    r = runner.run_vm_bench(_asm("alu_addi.s"), "addi", "alu", 100_000_000, "MIPS", repeats=1)
    assert r.metric_value >= 10, f"addi too slow: {r.metric_value:.1f} MIPS (expected >= 10)"


@pytest.mark.benchmark
def test_add_throughput(runner):
    r = runner.run_vm_bench(_asm("alu_add.s"), "add", "alu", 100_000_000, "MIPS", repeats=1)
    assert r.metric_value >= 10, f"add too slow: {r.metric_value:.1f} MIPS"


# ---------------------------------------------------------------------------
# M extension performance floors
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_mul_throughput(runner):
    r = runner.run_vm_bench(_asm("mext_mul.s"), "mul", "mext", 50_000_000, "MIPS", repeats=1)
    assert r.metric_value >= 5, f"mul too slow: {r.metric_value:.1f} MIPS"


@pytest.mark.benchmark
def test_div_throughput(runner):
    r = runner.run_vm_bench(_asm("mext_div.s"), "div", "mext", 10_000_000, "MIPS", repeats=1)
    assert r.metric_value >= 1, f"div too slow: {r.metric_value:.1f} MIPS"


# ---------------------------------------------------------------------------
# Float performance floors
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_fadd_throughput(runner):
    r = runner.run_vm_bench(_asm("float_fadd.s"), "fadd", "float", 50_000_000, "MFLOPS", repeats=1)
    assert r.metric_value >= 5, f"fadd too slow: {r.metric_value:.1f} MFLOPS"


@pytest.mark.benchmark
def test_fsqrt_throughput(runner):
    r = runner.run_vm_bench(_asm("float_fsqrt.s"), "fsqrt", "float", 10_000_000, "MFLOPS", repeats=1)
    assert r.metric_value >= 1, f"fsqrt too slow: {r.metric_value:.1f} MFLOPS"


# ---------------------------------------------------------------------------
# Branch performance floors
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_branch_taken_throughput(runner):
    r = runner.run_vm_bench(
        _asm("branch_taken.s"), "taken", "branch", 100_000_000, "BOPS", repeats=1
    )
    assert r.metric_value >= 10, f"branch (taken) too slow: {r.metric_value:.1f} BOPS"


# ---------------------------------------------------------------------------
# Real-world correctness + timing
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_fib_iter_correctness(runner):
    r = runner.run_realworld(_asm("realworld_fib_iter.s"), "fib(40) iter", "102334155", repeats=1)
    assert r.actual_out == "102334155", f"fib(40) wrong: {r.actual_out!r}"
    assert r.elapsed_ms < 30_000, f"fib(40) iter too slow: {r.elapsed_ms:.0f} ms"


@pytest.mark.benchmark
def test_fib_rec_correctness(runner):
    r = runner.run_realworld(_asm("realworld_fib_rec.s"), "fib(30) rec", "832040", repeats=1)
    assert r.actual_out == "832040", f"fib(30) rec wrong: {r.actual_out!r}"
    assert r.elapsed_ms < 60_000, f"fib(30) rec too slow: {r.elapsed_ms:.0f} ms"


@pytest.mark.benchmark
def test_sieve_correctness(runner):
    r = runner.run_realworld(_asm("realworld_sieve.s"), "sieve(1000)", "168", repeats=1)
    assert r.actual_out == "168", f"sieve(1000) wrong: {r.actual_out!r}"


@pytest.mark.benchmark
def test_newton_correctness(runner):
    r = runner.run_realworld(_asm("realworld_newton.s"), "newton sqrt(2)", "1.414214", repeats=1)
    assert r.actual_out.startswith("1.41421"), f"newton wrong: {r.actual_out!r}"


# ---------------------------------------------------------------------------
# Assembler throughput floor
# ---------------------------------------------------------------------------

@pytest.mark.benchmark
def test_assembler_throughput(runner):
    r = runner.run_assembler_bench(source_kb=10, repeats=1)
    assert r.metric_value >= 0.1, f"assembler too slow: {r.metric_value:.2f} MB/s"
