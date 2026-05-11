"""
compare.py — Compare two benchmark log JSON files and produce comparison
graphs + a console diff table.

Usage:
    python3 benchmarks/compare.py <baseline.json> <new.json> \
        [--out-dir DIR] [--label-a NAME] [--label-b NAME] [--no-graphs]

Each input JSON is expected to be a file written by run.py — a top-level
object with a "results" array, each entry having at least:
    name, category, metric_value, metric_unit, elapsed_ms, stddev_ms

Benchmarks are matched by name. Anything present in only one file is listed
separately and skipped from the graphs.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

@dataclass
class Entry:
    name: str
    category: str
    metric_value: float
    metric_unit: str
    elapsed_ms: float
    stddev_ms: float


def _load(path: str) -> dict[str, Entry]:
    with open(path, "r") as f:
        data = json.load(f)
    out: dict[str, Entry] = {}
    for r in data.get("results", []):
        out[r["name"]] = Entry(
            name=r["name"],
            category=r.get("category", "misc"),
            metric_value=float(r.get("metric_value", 0.0)),
            metric_unit=r.get("metric_unit", ""),
            elapsed_ms=float(r.get("elapsed_ms", 0.0)),
            stddev_ms=float(r.get("stddev_ms", 0.0)),
        )
    return out


# ---------------------------------------------------------------------------
# For some metrics higher is better (MIPS, MFLOPS, MB/s, BOPS); for some
# lower is better (elapsed ms). We auto-detect from metric_unit.
# ---------------------------------------------------------------------------

_LOWER_IS_BETTER_UNITS = {"ms", "s", "elapsed_ms"}


def _lower_is_better(unit: str) -> bool:
    return unit.lower() in _LOWER_IS_BETTER_UNITS


def _delta_pct(a: float, b: float, lower_is_better: bool) -> float:
    """Return percent improvement of b over a, signed so positive == better."""
    if a == 0:
        return 0.0
    raw = (b - a) / a * 100.0
    return -raw if lower_is_better else raw


# ---------------------------------------------------------------------------
# Console diff table
# ---------------------------------------------------------------------------

_GREEN = "\x1b[32m"
_RED   = "\x1b[31m"
_DIM   = "\x1b[2m"
_RESET = "\x1b[0m"


def _color(s: str, c: str) -> str:
    if not sys.stdout.isatty():
        return s
    return f"{c}{s}{_RESET}"


def print_diff_table(
    a: dict[str, Entry],
    b: dict[str, Entry],
    label_a: str,
    label_b: str,
) -> None:
    common = sorted(set(a) & set(b), key=lambda n: (a[n].category, n))
    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))

    col_name   = 22
    col_cat    = 10
    col_val    = 12
    col_unit   = 8
    col_delta  = 12
    col_pct    = 10

    header = (
        f"{'Benchmark':<{col_name}}"
        f"{'Category':<{col_cat}}"
        f"{label_a:>{col_val}}"
        f"{label_b:>{col_val}}"
        f"  {'Unit':<{col_unit}}"
        f"{'Δ':>{col_delta}}"
        f"{'%':>{col_pct}}"
    )
    sep = "-" * len(header)

    print()
    print(sep)
    print(header)
    print(sep)

    prev_cat = None
    for n in common:
        ea, eb = a[n], b[n]
        if ea.category != prev_cat:
            if prev_cat is not None:
                print()
            prev_cat = ea.category

        lower = _lower_is_better(ea.metric_unit)
        delta = eb.metric_value - ea.metric_value
        pct   = _delta_pct(ea.metric_value, eb.metric_value, lower)

        # color: green if pct positive (b improved), red if negative
        pct_str = f"{pct:+.1f}%"
        delta_str = f"{delta:+.2f}"
        if pct > 1.0:
            pct_str = _color(pct_str, _GREEN)
            delta_str = _color(delta_str, _GREEN)
        elif pct < -1.0:
            pct_str = _color(pct_str, _RED)
            delta_str = _color(delta_str, _RED)
        else:
            pct_str = _color(pct_str, _DIM)
            delta_str = _color(delta_str, _DIM)

        print(
            f"{ea.name:<{col_name}}"
            f"{ea.category:<{col_cat}}"
            f"{ea.metric_value:>{col_val}.2f}"
            f"{eb.metric_value:>{col_val}.2f}"
            f"  {ea.metric_unit:<{col_unit}}"
            f"{delta_str:>{col_delta + (len(delta_str) - len(f'{delta:+.2f}'))}}"
            f"{pct_str:>{col_pct + (len(pct_str) - len(f'{pct:+.1f}%'))}}"
        )

    print(sep)

    if only_a:
        print(f"\nOnly in {label_a}: {', '.join(only_a)}")
    if only_b:
        print(f"Only in {label_b}: {', '.join(only_b)}")

    if common:
        # geometric mean of the per-benchmark improvement ratios — gives a
        # single "average speedup" number that doesn't get dragged around by
        # outliers the way an arithmetic mean would.
        ratios = []
        for n in common:
            ea, eb = a[n], b[n]
            if ea.metric_value <= 0 or eb.metric_value <= 0:
                continue
            lower = _lower_is_better(ea.metric_unit)
            r = ea.metric_value / eb.metric_value if lower else eb.metric_value / ea.metric_value
            ratios.append(r)
        if ratios:
            from math import log, exp
            gmean = exp(sum(log(r) for r in ratios) / len(ratios))
            speedup_pct = (gmean - 1.0) * 100.0
            tag = (
                _color(f"{speedup_pct:+.1f}%", _GREEN if speedup_pct > 0 else _RED)
                if abs(speedup_pct) > 0.5 else f"{speedup_pct:+.1f}%"
            )
            print(f"\nGeomean speedup ({label_b} vs {label_a}): {tag}  "
                  f"(ratio {gmean:.3f}×, n={len(ratios)})")
    print()


# ---------------------------------------------------------------------------
# Graphs
# ---------------------------------------------------------------------------

def generate_compare_graphs(
    a: dict[str, Entry],
    b: dict[str, Entry],
    label_a: str,
    label_b: str,
    out_dir: str,
) -> None:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("[compare] matplotlib not installed — skipping graphs.")
        return

    os.makedirs(out_dir, exist_ok=True)

    common = set(a) & set(b)
    if not common:
        print("[compare] no overlapping benchmarks — nothing to plot.")
        return

    # group by category, preserving the order from file a
    by_cat: dict[str, list[str]] = {}
    seen: set[str] = set()
    for n, ea in a.items():
        if n in common and n not in seen:
            by_cat.setdefault(ea.category, []).append(n)
            seen.add(n)

    color_a = "#4C72B0"
    color_b = "#DD8452"

    for cat, names in by_cat.items():
        unit = a[names[0]].metric_unit
        lower = _lower_is_better(unit)

        values_a = [a[n].metric_value for n in names]
        values_b = [b[n].metric_value for n in names]
        err_a    = [a[n].stddev_ms for n in names]
        err_b    = [b[n].stddev_ms for n in names]

        fig, ax = plt.subplots(figsize=(max(7, len(names) * 1.4), 5.2))
        x = list(range(len(names)))
        w = 0.4

        bars_a = ax.bar([xi - w/2 for xi in x], values_a, w,
                        label=label_a, color=color_a, edgecolor="white")
        bars_b = ax.bar([xi + w/2 for xi in x], values_b, w,
                        label=label_b, color=color_b, edgecolor="white")

        if any(err_a):
            ax.errorbar([xi - w/2 for xi in x], values_a, yerr=err_a,
                        fmt="none", color="black", capsize=3, linewidth=1.0)
        if any(err_b):
            ax.errorbar([xi + w/2 for xi in x], values_b, yerr=err_b,
                        fmt="none", color="black", capsize=3, linewidth=1.0)

        # delta % labels above each pair
        for i, n in enumerate(names):
            pct = _delta_pct(a[n].metric_value, b[n].metric_value, lower)
            top = max(values_a[i], values_b[i])
            color = "#2a8a2a" if pct > 1.0 else ("#b22222" if pct < -1.0 else "#666666")
            ax.text(i, top * 1.04, f"{pct:+.1f}%",
                    ha="center", va="bottom", fontsize=9,
                    color=color, fontweight="bold")

        for bar, val in list(zip(bars_a, values_a)) + list(zip(bars_b, values_b)):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() * 1.005,
                    f"{val:.1f}",
                    ha="center", va="bottom", fontsize=7, color="#333")

        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=20, ha="right", fontsize=9)
        ax.set_ylabel(unit + ("  (lower is better)" if lower else "  (higher is better)"))
        ax.set_title(f"{cat} — {label_a} vs {label_b}",
                     fontsize=12, fontweight="bold")
        ax.legend(loc="best", frameon=False)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.yaxis.grid(True, linestyle="--", alpha=0.5)
        ax.set_axisbelow(True)
        ax.set_ylim(top=max(values_a + values_b) * 1.18)

        fig.tight_layout()
        out = os.path.join(out_dir, f"compare_{cat}.png")
        fig.savefig(out, dpi=150)
        plt.close(fig)
        print(f"[compare]   wrote {out}")

    # one summary chart: per-benchmark % delta across all categories
    names_all: list[str] = []
    pcts: list[float] = []
    cats:  list[str] = []
    for cat, names in by_cat.items():
        for n in names:
            lower = _lower_is_better(a[n].metric_unit)
            names_all.append(n)
            cats.append(cat)
            pcts.append(_delta_pct(a[n].metric_value, b[n].metric_value, lower))

    if names_all:
        fig, ax = plt.subplots(figsize=(max(8, len(names_all) * 0.6), 5.2))
        bar_colors = ["#2a8a2a" if p > 0 else "#b22222" for p in pcts]
        ax.bar(range(len(names_all)), pcts, color=bar_colors, edgecolor="white")
        ax.axhline(0, color="black", linewidth=0.8)
        ax.set_xticks(range(len(names_all)))
        ax.set_xticklabels(names_all, rotation=45, ha="right", fontsize=8)
        ax.set_ylabel(f"% change ({label_b} vs {label_a}, positive = better)")
        ax.set_title("Per-benchmark improvement", fontsize=12, fontweight="bold")
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.yaxis.grid(True, linestyle="--", alpha=0.5)
        ax.set_axisbelow(True)
        for i, p in enumerate(pcts):
            ax.text(i, p + (1 if p >= 0 else -1) * 0.5,
                    f"{p:+.1f}%",
                    ha="center", va="bottom" if p >= 0 else "top",
                    fontsize=7)
        fig.tight_layout()
        out = os.path.join(out_dir, "compare_summary.png")
        fig.savefig(out, dpi=150)
        plt.close(fig)
        print(f"[compare]   wrote {out}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _default_label(path: str) -> str:
    return os.path.splitext(os.path.basename(path))[0]


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Compare two benchmark JSON logs.")
    p.add_argument("file_a", help="baseline JSON")
    p.add_argument("file_b", help="new JSON to compare against the baseline")
    p.add_argument("--label-a", default=None, help="label for file_a (default: filename)")
    p.add_argument("--label-b", default=None, help="label for file_b (default: filename)")
    p.add_argument("--out-dir", default=None,
                   help="directory to write graphs (default: benchmarks/results/compare_<a>_vs_<b>)")
    p.add_argument("--no-graphs", action="store_true",
                   help="skip graph generation, print table only")
    args = p.parse_args(argv)

    label_a = args.label_a or _default_label(args.file_a)
    label_b = args.label_b or _default_label(args.file_b)

    a = _load(args.file_a)
    b = _load(args.file_b)

    print_diff_table(a, b, label_a, label_b)

    if args.no_graphs:
        return 0

    out_dir = args.out_dir
    if out_dir is None:
        results_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
        out_dir = os.path.join(results_dir, f"compare_{label_a}_vs_{label_b}")

    generate_compare_graphs(a, b, label_a, label_b, out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
