#!/usr/bin/env python3
"""
run_cross.py — Cross-language benchmark: cortex-vm vs lua vs wasm3.

Runs the same workload (recursive fib, iterative fib, sieve, Newton's method)
in three non-JIT bytecode/instruction interpreters:

  cortex-vm   custom register ISA, threaded-dispatch interpreter
  lua         PUC-Rio Lua 5.x, register-based bytecode interpreter
  wasm3       WebAssembly interpreter (no JIT)

These three are roughly comparable in implementation strategy: all are
non-JIT interpreters of a typed instruction stream. CPython is intentionally
left out — it pays a much heavier per-op tax (dynamic type dispatch, refcount
maintenance) so the comparison wasn't apples-to-apples.

Usage:
    python3 benchmarks/cross_lang/run_cross.py
    python3 benchmarks/cross_lang/run_cross.py --repeats 11
    python3 benchmarks/cross_lang/run_cross.py --no-graphs
    python3 benchmarks/cross_lang/run_cross.py --only fib_rec,sieve

Output:
    benchmarks/results/cross_lang_<timestamp>.json   raw timings
    benchmarks/results/cross_lang_bar.png            grouped bar chart
    benchmarks/results/cross_lang_speedup.png        speedup-vs-wasm3 chart
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, asdict
from datetime import datetime


HERE         = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
ASM_DIR      = os.path.join(PROJECT_ROOT, "benchmarks", "asm")
RESULTS_DIR  = os.path.join(PROJECT_ROOT, "benchmarks", "results")
VM_BINARY    = os.path.join(PROJECT_ROOT, "cortex")


# ---------------------------------------------------------------------------
# Benchmark catalog
# ---------------------------------------------------------------------------

@dataclass
class CrossBench:
    name: str
    description: str
    asm_file: str        # cortex-vm assembly source
    lua_file: str
    wat_file: str        # WebAssembly text source (compiled to .wasm at startup)
    expected: str        # stripped stdout we expect from every runtime


BENCHES: list[CrossBench] = [
    CrossBench(
        name="fib_rec",
        description="Recursive fib(35) -- ~29M calls",
        asm_file="realworld_fib_rec.s",
        lua_file="fib_rec.lua",
        wat_file="fib_rec.wat",
        expected="9227465",
    ),
    CrossBench(
        name="fib_iter",
        description="Iterative fib(40) x1,000,000 outer loops",
        asm_file="realworld_fib_iter.s",
        lua_file="fib_iter.lua",
        wat_file="fib_iter.wat",
        expected="102334155",
    ),
    CrossBench(
        name="sieve",
        description="Sieve of Eratosthenes (N=1000) x500",
        asm_file="realworld_sieve.s",
        lua_file="sieve.lua",
        wat_file="sieve.wat",
        expected="168",
    ),
    CrossBench(
        name="newton",
        description="Newton's method, sqrt(2), 1,000,000 iters",
        asm_file="realworld_newton.s",
        lua_file="newton.lua",
        wat_file="newton.wat",
        expected="1.414214",
    ),
]

RUNTIMES = ["cortex", "lua", "wasm3"]
COLORS = {"cortex": "#4C72B0", "lua": "#55A868", "wasm3": "#8172B2"}
BASELINE = "wasm3"   # speedup chart references this runtime


# ---------------------------------------------------------------------------
# Result type
# ---------------------------------------------------------------------------

@dataclass
class CrossResult:
    bench: str
    runtime: str
    median_ms: float
    min_ms: float
    stddev_ms: float
    repeats: int
    output: str


# ---------------------------------------------------------------------------
# Subprocess timing
# ---------------------------------------------------------------------------

def _time_run(cmd: list[str]) -> tuple[float, str, str]:
    """Run a command, return (wall_ms, stdout, stderr). Raises on failure."""
    t0 = time.perf_counter()
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    elapsed_ms = (time.perf_counter() - t0) * 1000.0
    if res.returncode != 0:
        raise RuntimeError(
            f"command failed (rc={res.returncode}): {' '.join(cmd)}\n"
            f"stderr:\n{res.stderr}"
        )
    return elapsed_ms, res.stdout, res.stderr


def _normalize_output(runtime: str, stdout: str, stderr: str) -> str:
    """
    Pull the answer out of whichever stream the runtime decided to use.

    wasm3 prints `Result: <value>` to STDERR (not stdout) when invoked with
    `--func`. cortex-vm and lua print to stdout normally.
    """
    if runtime == "wasm3":
        # try stderr first (where Result: lives), then fall back to stdout
        for stream in (stderr, stdout):
            for line in stream.strip().splitlines()[::-1]:
                line = line.strip()
                if line.startswith("Result:"):
                    return line[len("Result:"):].strip()
        return (stdout + stderr).strip()
    return stdout.strip()


def _bench_runtime(
    cmd: list[str],
    expected: str,
    repeats: int,
    bench_name: str,
    runtime_name: str,
) -> CrossResult:
    # warm-up + correctness check
    _, so, se = _time_run(cmd)
    out = _normalize_output(runtime_name, so, se)
    if out != expected:
        # newton: allow precision wobble in the last digits
        if not (bench_name == "newton" and out.startswith(expected[:6])):
            raise RuntimeError(
                f"{runtime_name}/{bench_name} output mismatch\n"
                f"  expected: {expected!r}\n"
                f"  got:      {out!r}\n"
                f"  stdout:   {so!r}\n"
                f"  stderr:   {se!r}"
            )

    samples: list[float] = []
    for _ in range(repeats):
        ms, _, _ = _time_run(cmd)
        samples.append(ms)

    return CrossResult(
        bench=bench_name,
        runtime=runtime_name,
        median_ms=statistics.median(samples),
        min_ms=min(samples),
        stddev_ms=statistics.stdev(samples) if len(samples) > 1 else 0.0,
        repeats=repeats,
        output=out,
    )


# ---------------------------------------------------------------------------
# Build helpers (excluded from timing)
# ---------------------------------------------------------------------------

def _assemble_cortex(asm_path: str, out_path: str) -> None:
    res = subprocess.run(
        [VM_BINARY, "-a", asm_path, "-o", out_path],
        capture_output=True, text=True, timeout=30,
    )
    if res.returncode != 0:
        raise RuntimeError(f"assembling {asm_path} failed:\n{res.stderr}")


def _build_wasm(wat2wasm: str, wat_path: str, out_path: str) -> None:
    res = subprocess.run(
        [wat2wasm, wat_path, "-o", out_path],
        capture_output=True, text=True, timeout=30,
    )
    if res.returncode != 0:
        raise RuntimeError(f"wat2wasm {wat_path} failed:\n{res.stderr}")


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def _print_table(results: list[CrossResult]) -> None:
    by_bench: dict[str, dict[str, CrossResult]] = {}
    for r in results:
        by_bench.setdefault(r.bench, {})[r.runtime] = r

    cw_name = max(8, max((len(b) for b in by_bench), default=8))
    col_rt  = 12

    header = f"{'benchmark':<{cw_name}}  " + "  ".join(f"{rt:>{col_rt}}" for rt in RUNTIMES) \
             + f"  {'cortex/lua':>{col_rt}}  {'cortex/wasm3':>{col_rt + 2}}"
    sep = "-" * len(header)
    print()
    print(sep)
    print(header)
    print(sep)
    for bench, runs in by_bench.items():
        cells = []
        for rt in RUNTIMES:
            r = runs.get(rt)
            cells.append(f"{r.median_ms:>{col_rt - 3}.2f}ms" if r else f"{'—':>{col_rt}}")
        c = runs.get("cortex"); l = runs.get("lua"); w = runs.get("wasm3")
        ratio_l = f"{l.median_ms / c.median_ms:>{col_rt - 1}.2f}×" if c and l and c.median_ms > 0 else f"{'—':>{col_rt}}"
        ratio_w = f"{w.median_ms / c.median_ms:>{col_rt + 1}.2f}×" if c and w and c.median_ms > 0 else f"{'—':>{col_rt + 2}}"
        print(f"{bench:<{cw_name}}  " + "  ".join(cells) + f"  {ratio_l}  {ratio_w}")
    print(sep)
    print("(ratio columns: > 1× means cortex is FASTER than that runtime)")
    print()


def _generate_graphs(results: list[CrossResult], out_dir: str) -> None:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("[cross] matplotlib not installed — skipping graphs.")
        return

    os.makedirs(out_dir, exist_ok=True)

    by_bench: dict[str, dict[str, CrossResult]] = {}
    for r in results:
        by_bench.setdefault(r.bench, {})[r.runtime] = r
    bench_names = list(by_bench.keys())

    # ---------- chart 1: grouped bars (wall ms, log scale) ----------
    fig, ax = plt.subplots(figsize=(max(7, len(bench_names) * 2.2), 5.4))
    x = list(range(len(bench_names)))
    n_rt = len(RUNTIMES)
    w = 0.8 / n_rt
    for i, rt in enumerate(RUNTIMES):
        vals = [by_bench[b].get(rt).median_ms if by_bench[b].get(rt) else 0
                for b in bench_names]
        errs = [by_bench[b].get(rt).stddev_ms if by_bench[b].get(rt) else 0
                for b in bench_names]
        offsets = [xi + (i - (n_rt - 1) / 2) * w for xi in x]
        ax.bar(offsets, vals, w, label=rt, color=COLORS[rt], edgecolor="white")
        if any(errs):
            ax.errorbar(offsets, vals, yerr=errs, fmt="none",
                        color="black", capsize=3, linewidth=1.0)
        for off, v in zip(offsets, vals):
            if v > 0:
                ax.text(off, v * 1.04, f"{v:.1f}",
                        ha="center", va="bottom", fontsize=7, color="#333")

    ax.set_xticks(x)
    ax.set_xticklabels(bench_names, fontsize=10)
    ax.set_ylabel("median wall-clock time (ms)  —  lower is better")
    ax.set_yscale("log")
    ax.set_title("Cross-runtime benchmarks (non-JIT interpreters)",
                 fontsize=12, fontweight="bold")
    ax.legend(loc="best", frameon=False)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.grid(True, which="both", linestyle="--", alpha=0.4)
    ax.set_axisbelow(True)
    fig.tight_layout()
    out = os.path.join(out_dir, "cross_lang_bar.png")
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"[cross]   wrote {out}")

    # ---------- chart 2: speedup vs baseline ----------
    fig, ax = plt.subplots(figsize=(max(7, len(bench_names) * 1.6), 5.0))
    x = list(range(len(bench_names)))
    others = [rt for rt in RUNTIMES if rt != BASELINE]
    w = 0.8 / max(len(others), 1)

    for i, rt in enumerate(others):
        speeds: list[float] = []
        for b in bench_names:
            base = by_bench[b].get(BASELINE)
            cur  = by_bench[b].get(rt)
            if base and cur and cur.median_ms > 0:
                speeds.append(base.median_ms / cur.median_ms)
            else:
                speeds.append(0)
        offsets = [xi + (i - (len(others) - 1) / 2) * w for xi in x]
        ax.bar(offsets, speeds, w, label=rt, color=COLORS[rt], edgecolor="white")
        for off, v in zip(offsets, speeds):
            if v > 0:
                ax.text(off, v * 1.02, f"{v:.2f}×",
                        ha="center", va="bottom", fontsize=8)

    ax.axhline(1.0, color=COLORS[BASELINE], linewidth=1.4, linestyle="--",
               label=f"{BASELINE} (=1×)")
    ax.set_xticks(x)
    ax.set_xticklabels(bench_names, fontsize=10)
    ax.set_ylabel(f"speedup vs {BASELINE}  —  higher is better")
    ax.set_title(f"Speedup over {BASELINE}",
                 fontsize=12, fontweight="bold")
    ax.legend(loc="best", frameon=False)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.grid(True, linestyle="--", alpha=0.4)
    ax.set_axisbelow(True)
    fig.tight_layout()
    out = os.path.join(out_dir, "cross_lang_speedup.png")
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"[cross]   wrote {out}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    p.add_argument("--repeats", type=int, default=7,
                   help="timed runs per (benchmark, runtime). Default 7.")
    p.add_argument("--only", default=None,
                   help="comma-separated subset, e.g. --only fib_rec,sieve")
    p.add_argument("--no-graphs", action="store_true",
                   help="skip graph generation")
    p.add_argument("--lua", default=shutil.which("lua") or "lua",
                   help="lua interpreter path (default: $(which lua))")
    p.add_argument("--wasm3", default=shutil.which("wasm3") or "wasm3",
                   help="wasm3 interpreter path (default: $(which wasm3))")
    p.add_argument("--wat2wasm", default=shutil.which("wat2wasm") or "wat2wasm",
                   help="wat2wasm path (from wabt) (default: $(which wat2wasm))")
    p.add_argument("--vm", default=VM_BINARY,
                   help=f"cortex-vm binary path (default: {VM_BINARY})")
    args = p.parse_args(argv)

    # sanity checks
    if not os.path.isfile(args.vm) or not os.access(args.vm, os.X_OK):
        print(f"error: cortex binary not found at {args.vm} — run `make` first",
              file=sys.stderr)
        return 1
    for label, path in [("lua", args.lua), ("wasm3", args.wasm3), ("wat2wasm", args.wat2wasm)]:
        if not shutil.which(path):
            print(f"error: {label} not found at {path}", file=sys.stderr)
            if label in ("wasm3", "wat2wasm"):
                print("  install on macOS:  brew install wasm3 wabt", file=sys.stderr)
                print("  install on linux:  apt install wasm3 wabt   (or build from source)",
                      file=sys.stderr)
            return 1

    selection = set(args.only.split(",")) if args.only else None
    benches = [b for b in BENCHES if selection is None or b.name in selection]
    if not benches:
        print(f"error: no benchmarks selected (filter: {args.only})", file=sys.stderr)
        return 1

    print(f"[cross] cortex-vm: {args.vm}")
    print(f"[cross] lua:       {args.lua}")
    print(f"[cross] wasm3:     {args.wasm3}")
    print(f"[cross] repeats:   {args.repeats}")

    # pre-build all artifacts into a tmpdir (excluded from timing)
    results: list[CrossResult] = []
    with tempfile.TemporaryDirectory(prefix="cross_lang_") as tmp:
        for b in benches:
            print(f"\n[cross] {b.name}  —  {b.description}")

            asm_path  = os.path.join(ASM_DIR, b.asm_file)
            cxb_path  = os.path.join(tmp, b.name + ".cxb")
            wat_path  = os.path.join(HERE, b.wat_file)
            wasm_path = os.path.join(tmp, b.name + ".wasm")

            print(f"  building cortex/{b.asm_file}...", end="", flush=True)
            _assemble_cortex(asm_path, cxb_path)
            print(" ok")
            print(f"  building wasm/{b.wat_file}...", end="", flush=True)
            _build_wasm(args.wat2wasm, wat_path, wasm_path)
            print(" ok")

            for runtime, cmd in [
                ("cortex", [args.vm, cxb_path]),
                ("lua",    [args.lua, os.path.join(HERE, b.lua_file)]),
                ("wasm3",  [args.wasm3, "--func", "main", wasm_path]),
            ]:
                print(f"  {runtime:<7}", end="", flush=True)
                try:
                    r = _bench_runtime(cmd, b.expected, args.repeats, b.name, runtime)
                except Exception as e:
                    print(f"  FAILED: {e}")
                    continue
                results.append(r)
                print(f"  median {r.median_ms:7.2f} ms  "
                      f"min {r.min_ms:7.2f} ms  "
                      f"stddev {r.stddev_ms:5.2f} ms")

    _print_table(results)

    # write raw json
    os.makedirs(RESULTS_DIR, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%dT%H%M%S")
    json_path = os.path.join(RESULTS_DIR, f"cross_lang_{ts}.json")
    with open(json_path, "w") as f:
        json.dump({
            "timestamp": ts,
            "host": f"{platform.system()} {platform.machine()}",
            "lua_path": args.lua,
            "wasm3_path": args.wasm3,
            "vm_path": args.vm,
            "repeats": args.repeats,
            "results": [asdict(r) for r in results],
        }, f, indent=2)
    print(f"[cross] wrote {json_path}")

    if not args.no_graphs:
        _generate_graphs(results, RESULTS_DIR)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
