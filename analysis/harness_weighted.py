"""Part 2 harness: weighted graphs on build/benchmark_weighted.

Input:  config_weighted.csv, one graph per row.
Output: data/results_weighted.csv  (memory, Dijkstra timings, graph info)
        data/paths_weighted.csv    (distance and shortest path per target)

Both outputs are appended to, one graph at a time, so a long run that dies
halfway keeps what it already measured. Delete them for a fresh run.

Usage (from the repository root, after `make benchmark-weighted`):
    python3 analysis/harness_weighted.py                     # every row
    python3 analysis/harness_weighted.py --only grafo_W_3    # one graph
    python3 analysis/harness_weighted.py --only a --only b   # a few

Before measuring anything, the harness checks that the benchmark is built and
that every input file of the selected rows exists, and lists all problems at
once. Graphs and names files may be gzipped ("<name>.txt.gz"): each one is
decompressed to a temporary file for its own run and deleted afterwards.

Config columns:
    graph           file name in the data directory, without ".txt"/".txt.gz"
    representation  csr, matrix or both
    array           yes/no: run dijkstra_array (O(n^2): "no" on large graphs)
    labels          names file ("id,name") without ".txt"/".txt.gz", or empty
    source          source vertex: an id, or a name when labels is set
    targets         target vertices separated by "|", ids or names
    k               number of random sources for the timings
    seed            seed for drawing the sources
"""

import argparse
import contextlib
import csv
import gzip
import os
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Iterator
from pathlib import Path

import pandas as pd
import psutil

ROOT = Path(__file__).resolve().parent.parent

RESULT_COLUMNS = [
    "graph", "representation", "metric",
    "value_average", "value_median", "samples", "status",
]
PATH_COLUMNS = [
    "graph", "representation", "source", "target", "source_name",
    "target_name", "distance", "hops", "path", "path_names", "status",
]


def representations_for(spec: str) -> list[str]:
    if spec == "both":
        return ["csr", "matrix"]
    if spec in ("csr", "matrix"):
        return [spec]
    raise ValueError(f"unknown representation spec: {spec!r}")


def result_row(name, rep, metric, value, samples=1, status="ok") -> dict:
    return {
        "graph": name, "representation": rep, "metric": metric,
        "value_average": value, "value_median": value,
        "samples": samples, "status": status,
    }


def run_all(binary: Path, args: list[str]) -> tuple[float, str, str]:
    """Runs the "all" mode. Returns (rss_mb_of_pristine_graph, stdout, stderr)."""
    proc = subprocess.Popen(
        [str(binary), *args],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    # 1. wait for READY (graph loaded, holding, nothing else allocated yet)
    line = proc.stdout.readline()
    if line.strip() != "READY":
        _, err = proc.communicate()
        raise RuntimeError(
            err.strip() or f"no READY, exit {proc.returncode}"
        )
    # 2. snapshot the pristine graph's memory
    rss_mb = psutil.Process(proc.pid).memory_info().rss / (1024 * 1024)
    # 3. release: the process now runs timing + paths
    out, err = proc.communicate("\n")
    if proc.returncode != 0:
        raise RuntimeError(err.strip() or f"exit {proc.returncode}")
    return rss_mb, out, err


def measure_baseline(binary: Path, data: Path, rep: str) -> float:
    """RSS of the process holding a 1-vertex graph: the cost of the program."""
    rss_mb, _, _ = run_all(
        binary, [str(data / "baseline.txt"), rep, "all", "--k", "1"]
    )
    return rss_mb


def find_input(data: Path, stem: str) -> Path | None:
    """data/<stem>.txt, else data/<stem>.txt.gz, else None."""
    for candidate in (data / f"{stem}.txt", data / f"{stem}.txt.gz"):
        if candidate.is_file():
            return candidate
    return None


@contextlib.contextmanager
def plain_text(path: Path, tmp: Path) -> Iterator[Path]:
    """The file itself, or a decompressed temporary copy of a .gz, deleted
    on exit (also when the run fails)."""
    if path.suffix != ".gz":
        yield path
        return
    fd, name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".txt",
                                dir=tmp)
    out = Path(name)
    try:
        print(f"  decompressing {path.name} ...", flush=True)
        with gzip.open(path, "rb") as src, os.fdopen(fd, "wb") as dst:
            shutil.copyfileobj(src, dst, length=1 << 20)
        yield out
    finally:
        out.unlink(missing_ok=True)


def check_setup(binary: Path, data: Path, rows: list[dict]) -> list[str]:
    """Every problem that would stop the run, so they can be fixed at once."""
    problems = []
    if not (binary.is_file() and os.access(binary, os.X_OK)):
        problems.append(f"{binary} not found: build it with "
                        "`make benchmark-weighted`")
    if not (data / "baseline.txt").is_file():
        problems.append(f"{data / 'baseline.txt'} not found")
    for cfg in rows:
        if find_input(data, cfg["graph"]) is None:
            problems.append(f"{cfg['graph']}: no {cfg['graph']}.txt or "
                            f"{cfg['graph']}.txt.gz in {data}")
        if cfg["labels"] and find_input(data, cfg["labels"]) is None:
            problems.append(f"{cfg['graph']}: labels file {cfg['labels']}.txt "
                            f"(or .txt.gz) not in {data}")
    return problems


def select_rows(config: pd.DataFrame, only: list[str] | None) -> list[dict]:
    """The config rows to run: all of them, or those named by --only."""
    rows = config.to_dict("records")
    if not only:
        return rows
    known = {cfg["graph"] for cfg in rows}
    unknown = [name for name in only if name not in known]
    if unknown:
        sys.exit(f"--only: not in the config: {', '.join(unknown)} "
                 f"(known: {', '.join(sorted(known))})")
    return [cfg for cfg in rows if cfg["graph"] in only]


def benchmark_args(cfg: dict, graph: Path, labels: Path | None,
                   rep: str) -> list[str]:
    args = [
        str(graph), rep, "all",
        "--k", str(cfg["k"]), "--seed", str(cfg["seed"]),
        "--array", cfg["array"],
    ]
    if labels is not None:
        args += ["--labels", str(labels)]
    if cfg["source"]:
        args += ["--source", cfg["source"]]
        for target in cfg["targets"].split("|"):
            args += ["--target", target.strip()]
    return args


def parse_output(text: str, name: str, rep: str, results: list, paths: list):
    """Turns the benchmark's info/time/path rows into output rows."""
    for fields in csv.reader(text.splitlines()):
        if not fields:
            continue
        kind, rest = fields[0], fields[1:]
        if kind == "info":
            metric, value = rest
            results.append(result_row(name, rep, metric, float(value)))
        elif kind == "time":
            algorithm, average, median, samples, status = rest
            results.append({
                "graph": name, "representation": rep,
                "metric": f"{algorithm}_micros",
                "value_average": float(average) if average else None,
                "value_median": float(median) if median else None,
                "samples": int(samples), "status": status,
            })
        elif kind == "path":
            paths.append(dict(zip(PATH_COLUMNS, [name, rep, *rest])))
        else:
            raise RuntimeError(f"unexpected benchmark output: {fields}")


def collect_one(cfg: dict, binary: Path, graph: Path, labels: Path | None,
                baselines: dict):
    """Rows of both outputs for one config line, given its (plain) input
    files."""
    name = cfg["graph"]
    results: list[dict] = []
    paths: list[dict] = []
    reps = representations_for(cfg["representation"])

    # Record skipped representations so the gap is explicit in the data.
    for skipped in sorted({"csr", "matrix"} - set(reps)):
        results.append(result_row(name, skipped, "skipped", 1, 0, "skipped"))

    for rep in reps:
        try:
            rss_mb, out, _ = run_all(
                binary, benchmark_args(cfg, graph, labels, rep))
        except Exception as e:
            print(f"  FAILED for {name} ({rep}): {e}", flush=True)
            results.append(result_row(name, rep, "error", None, 0, "error"))
            continue
        base = baselines[rep]
        for metric, value in [
            ("memory_mb_raw", rss_mb),
            ("memory_mb_baseline", base),
            ("memory_mb_graph", rss_mb - base),
        ]:
            results.append(result_row(name, rep, metric, value))

        first = len(results)
        parse_output(out, name, rep, results, paths)
        # The benchmark reports a negative weight as status not_implemented.
        if any(r["status"] == "not_implemented" for r in results[first:]):
            print(f"  {name} ({rep}): negative weight, not implemented",
                  flush=True)

    return (pd.DataFrame(results, columns=RESULT_COLUMNS),
            pd.DataFrame(paths, columns=PATH_COLUMNS))


def append_csv(df: pd.DataFrame, path: Path) -> None:
    df.to_csv(path, mode="a", header=not path.exists(), index=False)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path,
                        default=ROOT / "config_weighted.csv")
    parser.add_argument("--data", type=Path, default=ROOT / "data",
                        help="directory with the graphs and baseline.txt")
    parser.add_argument("--out", type=Path, default=ROOT / "data",
                        help="directory for the output CSVs")
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "build" / "benchmark_weighted")
    parser.add_argument("--only", action="append", metavar="GRAPH",
                        help="run only this config row (repeatable)")
    parser.add_argument("--tmp", type=Path, default=Path(tempfile.gettempdir()),
                        help="where to decompress .gz inputs "
                             "(needs room for the largest graph)")
    args = parser.parse_args()

    # Strings throughout: empty cells stay "" instead of NaN.
    config = pd.read_csv(args.config, dtype=str, keep_default_na=False)
    rows = select_rows(config, args.only)
    problems = check_setup(args.binary, args.data, rows)
    if problems:
        sys.exit("cannot start:\n" + "\n".join(f"  - {p}" for p in problems))
    args.out.mkdir(parents=True, exist_ok=True)
    results_out = args.out / "results_weighted.csv"
    paths_out = args.out / "paths_weighted.csv"

    # Measure the baseline RSS once per representation and reuse everywhere.
    baselines = {rep: measure_baseline(args.binary, args.data, rep)
                 for rep in ("csr", "matrix")}

    for cfg in rows:
        print(f"processing {cfg['graph']} ...", flush=True)
        graph_file = find_input(args.data, cfg["graph"])
        labels_file = (find_input(args.data, cfg["labels"])
                       if cfg["labels"] else None)
        with contextlib.ExitStack() as stack:
            graph = stack.enter_context(plain_text(graph_file, args.tmp))
            labels = (stack.enter_context(plain_text(labels_file, args.tmp))
                      if labels_file else None)
            results, paths = collect_one(cfg, args.binary, graph, labels,
                                         baselines)
        append_csv(results, results_out)
        if not paths.empty:
            append_csv(paths, paths_out)
        print(f"  wrote {len(results)} result rows, {len(paths)} path rows",
              flush=True)


if __name__ == "__main__":
    main()
