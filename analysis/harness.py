import subprocess
import sys
from pathlib import Path

import pandas as pd
import psutil

ROOT = Path(__file__).resolve().parent.parent
BINARY = ROOT / "app" / "benchmark"
DATA = ROOT / "data"
BASELINE = DATA / "baseline.txt"


def representations_for(spec: str) -> list[str]:
    if spec == "both":
        return ["list", "matrix"]
    if spec in ("list", "matrix"):
        return [spec]
    raise ValueError(f"unknown representation spec: {spec!r}")


# --- Granular modes (kept for standalone debugging) ---


def run_timing(graph_path: Path, representation: str) -> str:
    result = subprocess.run(
        [str(BINARY), str(graph_path), representation, "time"],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


def run_memory(graph_path: Path, representation: str) -> float:
    proc = subprocess.Popen(
        [str(BINARY), str(graph_path), representation, "mem"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    line = proc.stdout.readline()
    if line.strip() != "READY":
        proc.wait()
        err = proc.stderr.read()
        raise RuntimeError(
            f"benchmark mem failed for {graph_path} ({representation}): "
            f"{err.strip() or 'no output, exit ' + str(proc.returncode)}"
        )
    rss_bytes = psutil.Process(proc.pid).memory_info().rss
    proc.stdin.write("\n")
    proc.stdin.flush()
    proc.wait()
    return rss_bytes / (1024 * 1024)


def run_report(graph_path: Path, representation: str) -> str:
    result = subprocess.run(
        [str(BINARY), str(graph_path), representation, "report"],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


# --- Combined mode: one load, memory + timing + report in a single process ---


def run_all(graph_path: Path, representation: str) -> tuple[float, str]:
    """Returns (rss_mb_of_pristine_graph, csv_of_timing_and_report)."""
    proc = subprocess.Popen(
        [str(BINARY), str(graph_path), representation, "all"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    # 1. wait for READY (graph loaded, holding, nothing else allocated yet)
    line = proc.stdout.readline()
    if line.strip() != "READY":
        proc.wait()
        err = proc.stderr.read()
        raise RuntimeError(
            f"benchmark all failed for {graph_path} ({representation}): "
            f"{err.strip() or 'no output, exit ' + str(proc.returncode)}"
        )
    # 2. snapshot the pristine graph's memory
    rss_mb = psutil.Process(proc.pid).memory_info().rss / (1024 * 1024)
    # 3. release — process now runs timing + report
    proc.stdin.write("\n")
    proc.stdin.flush()
    # 4. drain the rest of stdout (timing + report CSV) and wait for exit
    remaining_stdout, _ = proc.communicate()
    return rss_mb, remaining_stdout


def _parse_csv_rows(text: str, name: str, rep: str, rows: list) -> None:
    """Append graph,rep,metric,value_average,value_median rows from benchmark
    CSV output.

    Timing rows look like  <name>,<rep>,bfs,<avg>,<median>  -> metric bfs_micros
    Report rows look like  <name>,<rep>,<metric>,<v>         -> avg == median == v
    """
    for line in text.strip().splitlines():
        if not line:
            continue
        parts = line.split(",")
        metric = parts[2]
        if metric in ("bfs", "dfs"):
            metric = f"{metric}_micros"
            average, median = float(parts[3]), float(parts[4])
        else:
            average = median = float(parts[3])
        rows.append(
            {
                "graph": name,
                "representation": rep,
                "metric": metric,
                "value_average": average,
                "value_median": median,
            }
        )


def collect_one(name: str, rep_spec: str, baselines: dict[str, float]) -> pd.DataFrame:
    """Compute all rows for a single graph, per its representation spec."""
    rows = []
    graph_path = DATA / f"{name}.txt"
    reps = representations_for(rep_spec)

    # Record skipped representations so the gap is explicit in the data.
    for skipped in sorted({"list", "matrix"} - set(reps)):
        rows.append(
            {
                "graph": name,
                "representation": skipped,
                "metric": "skipped",
                "value_average": 1,
                "value_median": 1,
            }
        )

    for rep in reps:
        rss_mb, csv_out = run_all(graph_path, rep)

        # memory: raw, baseline (pre-measured, reused), isolated difference
        mem_base = baselines[rep]
        for metric, value in [
            ("memory_mb_raw", rss_mb),
            ("memory_mb_baseline", mem_base),
            ("memory_mb_graph", rss_mb - mem_base),
        ]:
            rows.append(
                {
                    "graph": name,
                    "representation": rep,
                    "metric": metric,
                    "value_average": value,
                    "value_median": value,
                }
            )

        # timing + report, parsed from the same process's output
        _parse_csv_rows(csv_out, name, rep, rows)

    return pd.DataFrame(rows)


# --- Retime: refresh only bfs_micros/dfs_micros in an existing results.csv,
#     leaving memory/report rows untouched. Uses the granular "time" mode so
#     nothing but the two traversals is recomputed. ---


def retime_all() -> None:
    config = pd.read_csv(ROOT / "config.csv")
    out = DATA / "results.csv"
    existing = pd.read_csv(out)

    new_rows: list[dict] = []
    for _, cfg_row in config.iterrows():
        name = cfg_row["graph"]
        graph_path = DATA / f"{name}.txt"
        for rep in representations_for(cfg_row["representation"]):
            print(f"retiming {name} ({rep}) ...", flush=True)
            csv_out = run_timing(graph_path, rep)
            _parse_csv_rows(csv_out, name, rep, new_rows)

    new_df = pd.DataFrame(new_rows)
    stale_keys = set(zip(new_df["graph"], new_df["representation"]))
    is_stale_timing = existing.apply(
        lambda r: (r["graph"], r["representation"]) in stale_keys
        and r["metric"] in ("bfs_micros", "dfs_micros"),
        axis=1,
    )
    merged = pd.concat([existing[~is_stale_timing], new_df], ignore_index=True)
    merged.to_csv(out, index=False)
    print(f"replaced {is_stale_timing.sum()} stale rows with {len(new_df)} new rows",
          flush=True)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "retime":
        retime_all()
        sys.exit(0)

    config = pd.read_csv(ROOT / "config.csv")
    out = DATA / "results.csv"

    # Measure the baseline RSS once per representation and reuse everywhere.
    baselines = {rep: run_memory(BASELINE, rep) for rep in ("list", "matrix")}

    for _, cfg_row in config.iterrows():
        name = cfg_row["graph"]
        print(f"processing {name} ...", flush=True)
        try:
            df = collect_one(name, cfg_row["representation"], baselines)
        except Exception as e:
            print(f"  FAILED for {name}: {e}", flush=True)
            continue
        df.to_csv(out, mode="a", header=not out.exists(), index=False)
        print(f"  wrote {len(df)} rows for {name}", flush=True)
