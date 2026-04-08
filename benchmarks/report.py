"""
report.py — Console table formatting and matplotlib graph generation.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from bench_core import BenchmarkResult

RESULTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")


# ---------------------------------------------------------------------------
# Console table
# ---------------------------------------------------------------------------

def print_table(results: list["BenchmarkResult"]) -> None:
    """Print a formatted results table to stdout."""
    col_name   = 22
    col_cat    = 10
    col_metric = 12
    col_unit   = 8
    col_ms     = 10
    col_std    = 10

    header = (
        f"{'Benchmark':<{col_name}}"
        f"{'Category':<{col_cat}}"
        f"{'Value':>{col_metric}}"
        f"  {'Unit':<{col_unit}}"
        f"{'Median ms':>{col_ms}}"
        f"{'Stddev ms':>{col_std}}"
    )
    sep = "-" * len(header)

    print()
    print(sep)
    print(header)
    print(sep)

    prev_cat = None
    for r in results:
        if r.category != prev_cat:
            if prev_cat is not None:
                print()
            prev_cat = r.category

        value_str = f"{r.metric_value:>12.2f}"
        ms_str    = f"{r.elapsed_ms:>10.1f}"
        std_str   = f"{r.stddev_ms:>10.1f}"

        print(
            f"{r.name:<{col_name}}"
            f"{r.category:<{col_cat}}"
            f"{value_str}"
            f"  {r.metric_unit:<{col_unit}}"
            f"{ms_str}"
            f"{std_str}"
        )

    print(sep)
    print()


# ---------------------------------------------------------------------------
# Graph generation
# ---------------------------------------------------------------------------

def generate_graphs(results: list["BenchmarkResult"]) -> None:
    """Generate PNG graphs for each benchmark category."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("[report] matplotlib not installed — skipping graphs.")
        return

    os.makedirs(RESULTS_DIR, exist_ok=True)

    by_cat: dict[str, list["BenchmarkResult"]] = {}
    for r in results:
        by_cat.setdefault(r.category, []).append(r)

    if "alu" in by_cat:
        _bar_chart(
            by_cat["alu"],
            title="Integer ALU Throughput",
            ylabel="MIPS",
            filename="alu_bar.png",
            color="#4C72B0",
            plt=plt,
        )

    if "mext" in by_cat or "float" in by_cat:
        combined = by_cat.get("mext", []) + by_cat.get("float", [])
        labels   = [r.name for r in combined]
        values   = [r.metric_value for r in combined]
        colors   = ["#DD8452"] * len(by_cat.get("mext", [])) + \
                   ["#55A868"] * len(by_cat.get("float", []))
        _bar_chart_raw(
            labels, values, colors,
            title="M / F Extension Throughput",
            ylabel="MIPS / MFLOPS",
            filename="mext_float_bar.png",
            plt=plt,
        )

    if "mem" in by_cat:
        alu_baseline = next(
            (r for r in by_cat.get("alu", []) if r.name == "addi"), None
        )
        mem_results = by_cat["mem"]
        if alu_baseline:
            all_r  = [alu_baseline] + mem_results
            colors = ["#4C72B0"] + ["#C44E52"] * len(mem_results)
        else:
            all_r  = mem_results
            colors = ["#C44E52"] * len(mem_results)
        _bar_chart_raw(
            [r.name for r in all_r],
            [r.metric_value for r in all_r],
            colors,
            title="Memory vs ALU Throughput (MIPS)",
            ylabel="MIPS",
            filename="memory_vs_alu.png",
            plt=plt,
        )

    if "branch" in by_cat:
        _bar_chart(
            by_cat["branch"],
            title="Branch Throughput",
            ylabel="BOPS",
            filename="branch_bar.png",
            color="#8172B2",
            plt=plt,
        )

    if "realworld" in by_cat:
        _bar_chart(
            by_cat["realworld"],
            title="Real-World Programs (wall-clock time)",
            ylabel="Elapsed ms",
            filename="realworld.png",
            color="#937860",
            plt=plt,
        )

    if "asm" in by_cat:
        _bar_chart(
            by_cat["asm"],
            title="Assembler Throughput",
            ylabel="MB/s",
            filename="assembler_throughput.png",
            color="#DA8BC3",
            plt=plt,
        )

    print(f"[report] Graphs written to {RESULTS_DIR}/")


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _bar_chart(
    results: list["BenchmarkResult"],
    title: str,
    ylabel: str,
    filename: str,
    color: str,
    plt,
) -> None:
    _bar_chart_raw(
        [r.name for r in results],
        [r.metric_value for r in results],
        [color] * len(results),
        title=title,
        ylabel=ylabel,
        filename=filename,
        plt=plt,
        errors=[r.stddev_ms for r in results] if any(r.stddev_ms for r in results) else None,
    )


def _bar_chart_raw(
    labels: list[str],
    values: list[float],
    colors: list[str],
    title: str,
    ylabel: str,
    filename: str,
    plt,
    errors: list[float] | None = None,
) -> None:
    fig, ax = plt.subplots(figsize=(max(6, len(labels) * 1.2), 5))
    x = range(len(labels))

    bars = ax.bar(x, values, color=colors, edgecolor="white", linewidth=0.8)

    if errors:
        ax.errorbar(
            x, values, yerr=errors,
            fmt="none", color="black", capsize=4, linewidth=1.2,
        )

    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=20, ha="right", fontsize=9)
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontsize=12, fontweight="bold")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.grid(True, linestyle="--", alpha=0.5)
    ax.set_axisbelow(True)

    for bar, val in zip(bars, values):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() * 1.01,
            f"{val:.1f}",
            ha="center", va="bottom", fontsize=8,
        )

    fig.tight_layout()
    out = os.path.join(RESULTS_DIR, filename)
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"[report]   wrote {out}")
