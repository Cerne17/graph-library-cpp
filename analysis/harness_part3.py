"""Part 3 harness: case studies on build/benchmark_part3.

Input:  config_part3.csv, one graph per row; graphs in data/part3/
        (<graph>.txt or <graph>.txt.gz, decompressed once per graph).
Output: data/part3_runs/<graph>/<key>.csv, one file per launch of the binary
        (its stdout, copied line by line), then, merged from the complete
        files only:
        data/results_part3.csv  (times, estimates, negative cycles, counters)
        data/paths_part3.csv    (distances and paths of the case studies)

A launch file is complete when its last row is "done,<mode>". On a restart,
complete files are never rerun; an incomplete one is renamed
<key>.csv.incomplete and its launch is run again. A watchdog kills a launch
only when no row arrived for 2 x the step budget (20 minutes by default).

Per graph, in this order: calibrate, cycle, bf (with the optimization
variants of the config), fw (unless it does not fit in memory: no_memory, or
is turned off: skipped), one bf_check_<t> launch per vertex t of the
Floyd-Warshall set (the cross-check), allpairs.

Usage (from the repository root, after `make benchmark-part3`):
    python3 analysis/harness_part3.py                    # every row
    python3 analysis/harness_part3.py --only grafo_W_1   # one graph
    python3 analysis/harness_part3.py --calibrate-only   # VM-time estimate
    python3 analysis/harness_part3.py --merge-only       # rebuild the CSVs

Several harness processes may run at once on different graphs (--only); each
rewrites the two CSVs from every graph's run files when it ends, and a final
--merge-only gives the complete pair.

Config columns:
    graph             file name in the data directory, without ".txt"/".txt.gz"
    directed          yes/no, required
    bf_target         Bellman-Ford target (case study 1)
    bf_sources        sources whose distance to the target is reported, "|"-separated
    fw_set            Floyd-Warshall vertices (case study 2), "|"-separated
    runs              repetitions per timed step (within the budget)
    budget_seconds    time budget per timed step
    k                 all-pairs Bellman-Ford: targets measured when not all fit
    seed              seed of the all-pairs target order and the calibration graphs
    run_fw            yes/no
    run_allpairs      yes/no
    bf_optimizations  variants to compare, "|"-separated, or empty
"""

import argparse
import contextlib
import csv
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

import pandas as pd
import psutil

sys.path.insert(0, str(Path(__file__).resolve().parent))
from harness_weighted import (  # noqa: E402  (after the path insertion)
    append_csv,  # noqa: F401  (re-exported for callers of this module)
    find_input,
    plain_text,
    select_rows,
)

ROOT = Path(__file__).resolve().parent.parent

RESULT_COLUMNS = ["graph", "mode", "algorithm", "metric", "value", "samples",
                  "status", "note"]
PATH_COLUMNS = ["graph", "algorithm", "source", "target", "distance", "hops",
                "path", "status"]

# number of fields per row kind (time rows may carry one extra parameter)
ROW_FIELDS = {
    "info": {3}, "time": {4, 5}, "calib": {6}, "fit": {4}, "progress": {5},
    "path": {8}, "cycle": {6}, "check": {4}, "done": {2},
}

GIB = 1024 ** 3


# ---- parsing ---------------------------------------------------------------

def parse_row(line: str) -> list[str]:
    """The fields of one output row; raises ValueError for anything else."""
    fields = next(csv.reader([line.rstrip("\n")]))
    if not fields or fields[0] not in ROW_FIELDS:
        raise ValueError(f"unknown row kind: {line!r}")
    if len(fields) not in ROW_FIELDS[fields[0]]:
        raise ValueError(f"wrong number of fields for {fields[0]}: {line!r}")
    return fields


def read_rows(path: Path) -> list[list[str]]:
    with path.open() as f:
        return [parse_row(line) for line in f if line.strip()]


def is_complete(path: Path) -> bool:
    """True when the launch file exists and ends with its done row."""
    if not path.is_file():
        return False
    lines = [line for line in path.read_text().splitlines() if line.strip()]
    return bool(lines) and lines[-1].startswith("done,")


# ---- config and setup ------------------------------------------------------

def read_config(path: Path) -> pd.DataFrame:
    config = pd.read_csv(path, dtype=str, keep_default_na=False)
    for cfg in config.to_dict("records"):
        if cfg.get("directed", "") not in ("yes", "no"):
            sys.exit(f"{path}: {cfg.get('graph', '?')}: 'directed' must be "
                     "yes or no (no default)")
    return config


def check_setup(binary: Path, data: Path, rows: list[dict]) -> list[str]:
    """Every problem that would stop the run, so they can be fixed at once."""
    problems = []
    if not binary.is_file():
        problems.append(f"{binary} not found: build it with "
                        "`make benchmark-part3`")
    for cfg in rows:
        if find_input(data, cfg["graph"]) is None:
            problems.append(f"{cfg['graph']}: no {cfg['graph']}.txt or "
                            f"{cfg['graph']}.txt.gz in {data}")
        if cfg.get("directed", "") not in ("yes", "no"):
            problems.append(f"{cfg['graph']}: 'directed' must be yes or no")
    return problems


def vertex_count(graph: Path) -> int:
    """n, from the first line that is neither blank nor a '#' comment (the
    rule of read_edge_list; harness_weighted.vertex_count reads line 1 only)."""
    with graph.open() as f:
        for line in f:
            text = line.strip()
            if text and not text.startswith("#"):
                return int(text)
    raise ValueError(f"{graph}: no vertex count")


def fw_bytes_needed(graph: Path) -> int:
    """Floyd-Warshall matrices, (n + 1)^2 * (8 + 4) bytes, plus the edge list
    read before them (about the size of the text file)."""
    n = vertex_count(graph)
    return (n + 1) ** 2 * 12 + graph.stat().st_size


def available_memory() -> int:
    """Memory available now, minus a 1 GiB margin."""
    return psutil.virtual_memory().available - GIB


def decide_fw(needed: int, available: int) -> str:
    return "run" if needed <= available else "no_memory"


# ---- launching -------------------------------------------------------------

def prepare(out_file: Path) -> bool:
    """True if the launch must run: complete files are kept, an incomplete
    one is renamed .incomplete first."""
    if is_complete(out_file):
        return False
    if out_file.exists():
        out_file.replace(out_file.with_name(out_file.name + ".incomplete"))
    return True


def launch(args: list[str], out_file: Path, silence_limit: float) -> bool:
    """Runs the binary, copying its stdout line by line to out_file; kills it
    when no line arrived for silence_limit seconds. Returns whether the file
    ends with its done row."""
    out_file.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding="utf-8")
    last_row = [time.monotonic()]
    vertices = [0]

    def copy() -> None:
        with out_file.open("w") as out:
            for line in proc.stdout:
                out.write(line)
                out.flush()
                last_row[0] = time.monotonic()
                if line.startswith("info,vertices,"):
                    vertices[0] = int(line.strip().split(",")[2])
                elif line.startswith("progress,floyd_warshall,"):
                    _, _, k, elapsed, _ = line.strip().split(",")
                    k, elapsed = int(k), float(elapsed)
                    if k > 0 and vertices[0]:
                        eta = elapsed / k * (vertices[0] - k) / 1e6
                        print(f"    floyd_warshall k={k}/{vertices[0]}, "
                              f"ETA {eta / 3600:.2f} h", flush=True)

    reader = threading.Thread(target=copy, daemon=True)
    reader.start()
    while proc.poll() is None:
        if time.monotonic() - last_row[0] > silence_limit:
            proc.kill()
            print(f"    watchdog: no row for {silence_limit:.0f} s, killed",
                  flush=True)
            break
        time.sleep(0.2)
    proc.wait()
    reader.join()
    err = proc.stderr.read()
    proc.stdout.close()
    proc.stderr.close()
    if proc.returncode not in (0, None) and err.strip():
        print(f"    {err.strip().splitlines()[-1]}", flush=True)
    return is_complete(out_file)


def split_list(cell: str) -> str:
    """config "a|b|c" to command-line "a,b,c"."""
    return ",".join(part.strip() for part in cell.split("|") if part.strip())


def launches(cfg: dict, graph: Path, run_dir: Path, fw_decision: str
             ) -> list[tuple[str, list[str]]]:
    """(key, binary arguments after the program name) for every launch of a
    graph, in order."""
    base = [str(graph), "--directed", cfg["directed"],
            "--runs", cfg["runs"], "--budget-seconds", cfg["budget_seconds"]]
    out = []
    calibrate = base + ["calibrate", "--seed", cfg["seed"],
                        "--target", cfg["bf_target"]]
    if is_complete(run_dir / "bf.csv"):
        calibrate.append("--no-bf-pass")
    out.append(("calibrate", calibrate))
    out.append(("cycle", base + ["cycle"]))
    bf = base + ["bf", "--target", cfg["bf_target"],
                 "--sources", split_list(cfg["bf_sources"])]
    if cfg.get("bf_optimizations", ""):
        bf += ["--optimizations", split_list(cfg["bf_optimizations"])]
    out.append(("bf", bf))
    if fw_decision == "run":
        out.append(("fw", base + ["fw", "--set", split_list(cfg["fw_set"])]))
        fw_set = split_list(cfg["fw_set"])
        for t in fw_set.split(","):
            check = [str(graph), "--directed", cfg["directed"], "--runs", "1",
                     "--budget-seconds", cfg["budget_seconds"], "bf",
                     "--target", t, "--sources", fw_set]
            out.append((f"bf_check_{t}", check))
    if cfg.get("run_allpairs", "no") == "yes":
        out.append(("allpairs", base + ["allpairs", "--k", cfg["k"],
                                        "--seed", cfg["seed"]]))
    return out


# ---- statistics ------------------------------------------------------------

def summarize(times_us: list[float], runs: int) -> dict:
    """Mean and median in seconds, the sample count, and ok / fewer_runs."""
    seconds = [t / 1e6 for t in times_us]
    return {
        "mean": statistics.fmean(seconds),
        "median": statistics.median(seconds),
        "samples": len(seconds),
        "status": "ok" if len(seconds) >= runs else "fewer_runs",
    }


def result(graph, mode, algorithm, metric, value, samples=1, status="ok",
           note="") -> dict:
    return {"graph": graph, "mode": mode, "algorithm": algorithm,
            "metric": metric, "value": value, "samples": samples,
            "status": status, "note": note}


def times_of(rows: list[list[str]], algorithm: str) -> list[float]:
    return [float(r[3]) for r in rows if r[0] == "time" and r[1] == algorithm]


def infos(rows: list[list[str]]) -> dict[str, str]:
    return {r[1]: r[2] for r in rows if r[0] == "info"}


def timing_rows(graph, mode, algorithm, times_us, runs) -> list[dict]:
    if not times_us:
        return []
    s = summarize(times_us, runs)
    return [result(graph, mode, algorithm, "mean_s", s["mean"], s["samples"],
                   s["status"]),
            result(graph, mode, algorithm, "median_s", s["median"],
                   s["samples"], s["status"])]


def merge_graph(cfg: dict, run_dir: Path, fw_decision: str,
                fw_need: int | None) -> tuple[list[dict], list[dict]]:
    """Result and path rows of one graph, from its complete launch files."""
    name = cfg["graph"]
    runs = int(cfg["runs"])
    results: list[dict] = [result(name, "-", "matrix", "representation", "",
                                  0, "skipped", "Part 3 runs on CSR only")]
    paths: list[dict] = []
    files = {p.stem: read_rows(p) for p in sorted(run_dir.glob("*.csv"))
             if is_complete(p)}

    def add_paths(rows):
        for r in rows:
            if r[0] == "path":
                paths.append(dict(zip(["graph"] + PATH_COLUMNS[1:],
                                      [name] + r[1:])))

    n = None
    for rows in files.values():
        info = infos(rows)
        if "vertices" in info:
            n = int(info["vertices"])
            if "max_abs_weight" in info:
                results.append(result(name, "-", "-", "max_abs_weight",
                                      info["max_abs_weight"]))
            break

    if "cycle" in files:
        rows = files["cycle"]
        results += timing_rows(name, "cycle", "negative_cycle_check",
                               times_of(rows, "negative_cycle_check"), runs)
        for r in rows:
            if r[0] == "cycle":
                note = (f"weight={r[3]} witness={r[4]}" if r[1] == "yes"
                        else "")
                results.append(result(name, "cycle", "negative_cycle_check",
                                      "negative_cycle", r[1], 1, r[5], note))

    if "bf" in files:
        rows = files["bf"]
        reverse = times_of(rows, "reverse")
        passes = times_of(rows, "bellman_ford_pass")
        total = ([a + b for a, b in zip(reverse, passes)] if reverse
                 else passes)
        results += timing_rows(name, "bf", "bellman_ford", total, runs)
        if reverse:
            s = summarize(reverse, runs)
            results.append(result(name, "bf", "bellman_ford", "reverse_mean_s",
                                  s["mean"], s["samples"], s["status"]))
        info = infos(rows)
        for metric in ("rounds", "relaxations"):
            if metric in info:
                results.append(result(name, "bf", "bellman_ford", metric,
                                      info[metric]))
        for variant in ("textbook", "stop_when_stable", "frontier"):
            results += timing_rows(name, "bf", f"bellman_ford_{variant}",
                                   times_of(rows, f"bellman_ford_pass_{variant}"),
                                   runs)
            for metric in ("rounds", "relaxations"):
                key = f"{metric}_{variant}"
                if key in info:
                    results.append(result(name, "bf", f"bellman_ford_{variant}",
                                          metric, info[key]))
        add_paths(rows)

    fit_c = None
    if "calibrate" in files:
        for r in files["calibrate"]:
            if r[0] == "fit":
                fit_c = float(r[2])

    if fw_decision == "no_memory":
        results.append(result(name, "fw", "floyd_warshall", "memory_bytes",
                              fw_need, 0, "no_memory"))
    elif fw_decision == "skipped":
        results.append(result(name, "fw", "floyd_warshall", "memory_bytes",
                              fw_need, 0, "skipped", "run_fw=no"))
    if "fw" in files:
        rows = files["fw"]
        results += timing_rows(name, "fw", "floyd_warshall",
                               times_of(rows, "floyd_warshall"), runs)
        info = infos(rows)
        if "memory_bytes" in info:
            results.append(result(name, "fw", "floyd_warshall", "memory_bytes",
                                  info["memory_bytes"]))
        add_paths(rows)
        results.append(fw_vs_bf(name, rows, files))
    elif fit_c is not None and n is not None:
        results.append(result(name, "fw", "floyd_warshall", "estimate_s",
                              fit_c * n ** 3, 0, "extrapolated",
                              f"c * n^3, c = {fit_c:.6g} s from calibrate"))

    if "allpairs" in files:
        rows = files["allpairs"]
        info = infos(rows)
        reverse = times_of(rows, "reverse")
        passes = times_of(rows, "allpairs_pass")
        rev_s = reverse[0] / 1e6 if reverse else 0.0
        results.append(result(name, "allpairs", "bellman_ford",
                              "targets_measured", len(passes)))
        if len(passes) > 1:
            results.append(result(name, "allpairs", "bellman_ford",
                                  "pass_spread_s",
                                  statistics.stdev(passes) / 1e6, len(passes)))
        if info.get("allpairs_full") == "yes":
            results.append(result(name, "allpairs", "bellman_ford", "total_s",
                                  rev_s + sum(passes) / 1e6, len(passes), "ok"))
        elif passes and n is not None:
            mean = statistics.fmean(passes) / 1e6
            results.append(result(
                name, "allpairs", "bellman_ford", "estimate_s",
                rev_s + n * mean, len(passes), "extrapolated",
                f"reversal + n x mean pass = {rev_s:.6g} + {n} x {mean:.6g} s"))
    return results, paths


def fw_vs_bf(name: str, fw_rows: list[list[str]], files: dict) -> dict:
    """Compares the Floyd-Warshall path rows with the bf_check launches:
    status and printed distance of every pair (i, t)."""
    fw = {(r[2], r[3]): (r[4], r[7]) for r in fw_rows if r[0] == "path"}
    targets = {t for (_, t) in fw}
    differences = []
    for t in sorted(targets):
        rows = files.get(f"bf_check_{t}")
        if rows is None:
            return result(name, "fw", "floyd_warshall", "fw_vs_bf", "", 0,
                          "error", f"bf_check_{t} missing")
        for r in rows:
            if r[0] != "path" or r[2] == t:
                continue
            got = (r[4], r[7])
            if fw.get((r[2], t)) != got:
                differences.append(f"{r[2]}->{t}")
    if differences:
        return result(name, "fw", "floyd_warshall", "fw_vs_bf", "mismatch", 1,
                      "mismatch", " ".join(differences))
    return result(name, "fw", "floyd_warshall", "fw_vs_bf", "ok")


def vm_estimate_hours(rows: list[dict], fits: dict[str, float],
                      sizes: dict[str, int]) -> float:
    """Upper bound of the full run: c n^3 for each Floyd-Warshall answer run,
    plus one full budget for every other timed step."""
    total = 0.0
    for cfg in rows:
        budget = float(cfg["budget_seconds"])
        steps = 3 + len([v for v in cfg.get("bf_optimizations", "").split("|")
                         if v.strip()])
        if cfg.get("run_fw") == "yes" and cfg["graph"] in fits:
            total += fits[cfg["graph"]] * sizes[cfg["graph"]] ** 3
            steps += len(split_list(cfg["fw_set"]).split(","))
        total += steps * budget
    return total / 3600


# ---- main ------------------------------------------------------------------

def write_csv(rows: list[dict], columns: list[str], path: Path) -> None:
    """Writes atomically, so parallel harness processes never leave a half
    file behind."""
    tmp = path.with_name(path.name + ".tmp")
    pd.DataFrame(rows, columns=columns).to_csv(tmp, index=False)
    tmp.replace(path)


def merge_all(config_rows: list[dict], runs_dir: Path, out: Path) -> None:
    """results_part3.csv and paths_part3.csv from every graph of the config
    that has a run directory (also those run by another harness process)."""
    all_results: list[dict] = []
    all_paths: list[dict] = []
    for cfg in config_rows:
        run_dir = runs_dir / cfg["graph"]
        decision_file = run_dir / "fw_decision.txt"
        if not decision_file.is_file():
            continue
        decision, need = decision_file.read_text().split()
        results, paths = merge_graph(cfg, run_dir, decision, int(need))
        all_results += results
        all_paths += paths
    out.mkdir(parents=True, exist_ok=True)
    write_csv(all_results, RESULT_COLUMNS, out / "results_part3.csv")
    write_csv(all_paths, PATH_COLUMNS, out / "paths_part3.csv")
    print(f"wrote {len(all_results)} result rows, {len(all_paths)} path rows",
          flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=ROOT / "config_part3.csv")
    parser.add_argument("--data", type=Path, default=ROOT / "data" / "part3")
    parser.add_argument("--runs-dir", type=Path,
                        default=ROOT / "data" / "part3_runs")
    parser.add_argument("--out", type=Path, default=ROOT / "data")
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "build" / "benchmark_part3")
    parser.add_argument("--only", action="append", metavar="GRAPH")
    parser.add_argument("--calibrate-only", action="store_true")
    parser.add_argument("--merge-only", action="store_true",
                        help="only rebuild the two CSVs from the run files")
    parser.add_argument("--tmp", type=Path, default=Path(tempfile.gettempdir()))
    args = parser.parse_args()

    config_rows = read_config(args.config).to_dict("records")
    if args.merge_only:
        merge_all(config_rows, args.runs_dir, args.out)
        return

    rows = select_rows(read_config(args.config), args.only)
    problems = check_setup(args.binary, args.data, rows)
    if problems:
        sys.exit("cannot start:\n" + "\n".join(f"  - {p}" for p in problems))

    fits: dict[str, float] = {}
    sizes: dict[str, int] = {}
    for cfg in rows:
        name = cfg["graph"]
        run_dir = args.runs_dir / name
        run_dir.mkdir(parents=True, exist_ok=True)
        silence = 2 * float(cfg["budget_seconds"])
        print(f"processing {name} ...", flush=True)
        with plain_text(find_input(args.data, name), args.tmp) as graph:
            sizes[name] = vertex_count(graph)
            need = fw_bytes_needed(graph)
            # the memory check comes first: no_memory wins over skipped
            decision = decide_fw(need, available_memory())
            if decision == "run" and cfg.get("run_fw", "no") != "yes":
                decision = "skipped"
            (run_dir / "fw_decision.txt").write_text(f"{decision} {need}\n")
            for key, cli in launches(cfg, graph, run_dir, decision):
                if args.calibrate_only and key != "calibrate":
                    continue
                out_file = run_dir / f"{key}.csv"
                if not prepare(out_file):
                    print(f"  {key}: complete, kept", flush=True)
                    continue
                print(f"  {key} ...", flush=True)
                if not launch([str(args.binary)] + cli, out_file, silence):
                    print(f"  {key}: incomplete", flush=True)
        if is_complete(run_dir / "calibrate.csv"):
            for r in read_rows(run_dir / "calibrate.csv"):
                if r[0] == "fit":
                    fits[name] = float(r[2])

    if args.calibrate_only:
        hours = vm_estimate_hours(rows, fits, sizes)
        (args.runs_dir / "vm_estimate.txt").write_text(f"{hours:.2f}\n")
        print(f"estimated VM time for the full run: {hours:.2f} h "
              f"(written to {args.runs_dir / 'vm_estimate.txt'})", flush=True)
        return
    merge_all(config_rows, args.runs_dir, args.out)


if __name__ == "__main__":
    main()
