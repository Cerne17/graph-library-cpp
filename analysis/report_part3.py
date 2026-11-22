"""Part 3 report tables, from the harness outputs.

Reads results_part3.csv and paths_part3.csv (default: data/azure_part3/) and
writes the four case-study tables as Markdown (default:
docs/results/part3_tables.md): distances with 2 decimals, times in seconds
with 3 significant digits and the number of runs; estimated cells are marked
and their formula is given in a footnote.

Usage (from the repository root):
    python3 analysis/report_part3.py [--results DIR] [--out FILE]
"""

import argparse
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parent.parent

BF_SOURCES = ["10", "20", "30"]
BF_TARGET = "100"
FW_SET = ["10", "20", "30", "40", "50"]


def seconds(x: float) -> str:
    return f"{x:.3g}"


class Notes:
    """Footnotes for estimated cells, numbered in order of use."""

    def __init__(self) -> None:
        self.items: list[str] = []

    def mark(self, text: str) -> str:
        self.items.append(text)
        return f"[^{len(self.items)}]"

    def render(self) -> list[str]:
        return [f"[^{i}]: {t}" for i, t in enumerate(self.items, start=1)]


def metric(results: pd.DataFrame, graph: str, algorithm: str, name: str):
    rows = results[(results.graph == graph) & (results.algorithm == algorithm)
                   & (results.metric == name)]
    return None if rows.empty else rows.iloc[0]


def path_cell(paths: pd.DataFrame, graph: str, algorithm: str, source: str,
              target: str) -> str:
    rows = paths[(paths.graph == graph) & (paths.algorithm == algorithm)
                 & (paths.source == source) & (paths.target == target)]
    if rows.empty:
        return "—"
    row = rows.iloc[0]
    if row.status == "ok":
        return row.distance
    return {"unreachable": "∞ (unreachable)",
            "negative_cycle": "−∞ (negative cycle)",
            "out_of_range": "(no such vertex)"}.get(row.status, row.status)


def time_cell(results: pd.DataFrame, graph: str, algorithm: str,
              notes: Notes) -> str:
    mean = metric(results, graph, algorithm, "mean_s")
    if mean is not None:
        runs = int(mean.samples)
        return f"{seconds(float(mean.value))} ({runs} run{'s' if runs != 1 else ''})"
    estimate = metric(results, graph, algorithm, "estimate_s")
    if estimate is not None:
        return (f"≈ {seconds(float(estimate.value))}"
                + notes.mark(f"{graph}, {algorithm}: {estimate.note}"))
    memory = metric(results, graph, algorithm, "memory_bytes")
    if memory is not None and memory.status in ("no_memory", "skipped"):
        return memory.status
    return "—"


def build(results: pd.DataFrame, paths: pd.DataFrame) -> str:
    graphs = list(dict.fromkeys(results.graph))
    notes = Notes()
    out = ["# Part 3 case-study tables", ""]

    out += ["## 1. Bellman-Ford: distance to vertex 100", "",
            "| graph | " + " | ".join(f"{s} → {BF_TARGET}" for s in BF_SOURCES)
            + " |", "|---" * (len(BF_SOURCES) + 1) + "|"]
    for g in graphs:
        cells = [path_cell(paths, g, "bellman_ford", s, BF_TARGET)
                 for s in BF_SOURCES]
        out.append(f"| {g} | " + " | ".join(cells) + " |")

    out += ["", "## 2. Floyd-Warshall: distances among {10, 20, 30, 40, 50}", ""]
    for g in graphs:
        if paths[(paths.graph == g) & (paths.algorithm == "floyd_warshall")].empty:
            memory = metric(results, g, "floyd_warshall", "memory_bytes")
            why = memory.status if memory is not None else "not run"
            out.append(f"- {g}: {why}")
            continue
        out += [f"### {g}", "", "| from \\ to | " + " | ".join(FW_SET) + " |",
                "|---" * (len(FW_SET) + 1) + "|"]
        for i in FW_SET:
            # the diagonal is not a case-study pair (and is not 0 on a vertex
            # that lies on a negative cycle)
            cells = ["—" if i == j else
                     path_cell(paths, g, "floyd_warshall", i, j) for j in FW_SET]
            out.append(f"| {i} | " + " | ".join(cells) + " |")
        out.append("")

    out += ["", "## 3. Mean running time (seconds, without disk I/O)", "",
            "| graph | Bellman-Ford | Floyd-Warshall | negative-cycle check |",
            "|---|---|---|---|"]
    for g in graphs:
        out.append(f"| {g} | {time_cell(results, g, 'bellman_ford', notes)} | "
                   f"{time_cell(results, g, 'floyd_warshall', notes)} | "
                   f"{time_cell(results, g, 'negative_cycle_check', notes)} |")

    out += ["", "## 4. All pairs: n × Bellman-Ford against Floyd-Warshall", "",
            "| graph | all-pairs Bellman-Ford (s) | Floyd-Warshall (s) |",
            "|---|---|---|"]
    for g in graphs:
        total = metric(results, g, "bellman_ford", "total_s")
        if total is not None and total["mode"] == "allpairs":
            bf = f"{seconds(float(total.value))} (measured)"
        else:
            estimate = results[(results.graph == g) & (results["mode"] == "allpairs")
                               & (results.metric == "estimate_s")]
            if estimate.empty:
                bf = "—"
            else:
                row = estimate.iloc[0]
                bf = (f"≈ {seconds(float(row.value))}"
                      + notes.mark(f"{g}, all-pairs Bellman-Ford: {row.note} "
                                   f"({int(row.samples)} targets measured)"))
        out.append(f"| {g} | {bf} | "
                   f"{time_cell(results, g, 'floyd_warshall', notes)} |")

    if notes.items:
        out += [""] + notes.render()
    return "\n".join(out) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--results", type=Path, default=ROOT / "data" / "azure_part3")
    parser.add_argument("--out", type=Path,
                        default=ROOT / "docs" / "results" / "part3_tables.md")
    args = parser.parse_args()
    results = pd.read_csv(args.results / "results_part3.csv", dtype=str,
                          keep_default_na=False)
    paths = pd.read_csv(args.results / "paths_part3.csv", dtype=str,
                        keep_default_na=False)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(build(results, paths))
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
