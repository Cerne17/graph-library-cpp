"""Compares two Part 3 result directories (for example a local run and an
Azure run of the smoke config): every answer must agree exactly, timings may
differ.

Compared: paths_part3.csv (distance, hops, path, status of every row) and the
non-timing metrics of results_part3.csv (negative cycle and its witness,
rounds and relaxations of each Bellman-Ford variant, fw_vs_bf, max_abs_weight,
memory_bytes, and whether all-pairs was measured in full).

Usage: python3 analysis/compare_part3.py DIR_A DIR_B   (exit 1 on a difference)
"""

import sys
from pathlib import Path

import pandas as pd

ANSWER_METRICS = {"negative_cycle", "rounds", "relaxations", "fw_vs_bf",
                  "max_abs_weight", "memory_bytes"}


def load(directory: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    read = lambda name: pd.read_csv(directory / name, dtype=str,  # noqa: E731
                                    keep_default_na=False)
    return read("results_part3.csv"), read("paths_part3.csv")


def answers(results: pd.DataFrame) -> dict[tuple, tuple]:
    out = {}
    for r in results.itertuples(index=False):
        if r.metric in ANSWER_METRICS:
            out[(r.graph, r.mode, r.algorithm, r.metric)] = (r.value, r.status,
                                                             r.note)
        elif r.mode == "allpairs" and r.metric in ("total_s", "estimate_s"):
            out[(r.graph, r.mode, r.algorithm, "measured_in_full")] = (
                r.metric == "total_s",)
    return out


def path_rows(paths: pd.DataFrame) -> dict[tuple, tuple]:
    return {(r.graph, r.algorithm, r.source, r.target):
            (r.distance, r.hops, r.path, r.status)
            for r in paths.itertuples(index=False)}


def compare(a: dict, b: dict, what: str) -> list[str]:
    problems = []
    for key in sorted(set(a) | set(b)):
        if a.get(key) != b.get(key):
            problems.append(f"{what} {key}: {a.get(key)} != {b.get(key)}")
    return problems


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    (res_a, paths_a), (res_b, paths_b) = (load(Path(d)) for d in sys.argv[1:])
    problems = (compare(path_rows(paths_a), path_rows(paths_b), "path")
                + compare(answers(res_a), answers(res_b), "metric"))
    if problems:
        print("\n".join(problems))
        sys.exit(f"{len(problems)} differences")
    print(f"agree: {len(paths_a)} path rows and "
          f"{len(answers(res_a))} answer metrics")


if __name__ == "__main__":
    main()
