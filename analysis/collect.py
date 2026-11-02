"""Collect reproducible course statistics and bounded visualization datasets.

Build: make collector
Run: python3 analysis/collect.py --only grafo_W_3 --phases stats,cases
See docs/COLLECTION.md for the schema, budgets and resume semantics.
Only the Python standard library is required.
"""
from __future__ import annotations

import argparse
import contextlib
import csv
import datetime as dt
import fcntl
import gzip
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
SCHEMA_VERSION = 1


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as src:
        for chunk in iter(lambda: src.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_json(path: Path, data: dict) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
    temporary.replace(path)


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def find_input(data: Path, stem: str) -> Path:
    if not stem or Path(stem).name != stem or stem in (".", ".."):
        raise ValueError(f"invalid input stem: {stem!r}")
    for suffix in (".txt", ".txt.gz"):
        candidate = data / (stem + suffix)
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"missing {stem}.txt or {stem}.txt.gz in {data}")


@contextlib.contextmanager
def plain_input(path: Path, tmp: Path):
    """Decompress and hash in one pass; gzip verifies CRC before yielding."""
    if path.suffix != ".gz":
        yield path, digest(path)
        return
    with tempfile.TemporaryDirectory(prefix="cos242-input-", dir=tmp) as directory:
        out = Path(directory) / path.stem
        h = hashlib.sha256()
        with gzip.open(path, "rb") as src, out.open("wb") as dst:
            for chunk in iter(lambda: src.read(1 << 20), b""):
                h.update(chunk)
                dst.write(chunk)
        yield out, h.hexdigest()


def vertex_count(path: Path) -> int:
    with path.open(encoding="utf-8") as src:
        for line in src:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            if not text.isascii() or not text.lstrip("+").isdigit():
                raise ValueError(f"{path}: invalid vertex count")
            n = int(text)
            if n > (1 << 32) - 3:
                raise ValueError(f"{path}: vertex count out of range")
            return n
    raise ValueError(f"{path}: missing vertex count")


def available_memory(binary: Path) -> int:
    if sys.platform.startswith("linux"):
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith("MemAvailable:"):
                host = int(line.split()[1]) * 1024
                # Account for common container limits as well as host RAM.
                try:
                    limit = Path("/sys/fs/cgroup/memory.max").read_text().strip()
                    if limit != "max":
                        used = int(Path("/sys/fs/cgroup/memory.current").read_text())
                        host = min(host, max(0, int(limit) - used))
                except (OSError, ValueError):
                    pass
                return int(host * .8)
    if sys.platform == "darwin":
        # Conservative physical-RAM budget; do not assume swap is usable RAM.
        total = int(subprocess.check_output([str(binary), "--physical-memory"], text=True))
        return int(total * .6)
    raise RuntimeError("cannot detect memory budget; pass --memory-limit-gb")


def rss_bytes(pid: int) -> int:
    if sys.platform.startswith("linux"):
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1]) * 1024
    value = subprocess.check_output(["ps", "-o", "rss=", "-p", str(pid)], text=True)
    return int(value.strip()) * 1024


class JobFailure(RuntimeError):
    def __init__(self, status: str, message: str):
        super().__init__(message)
        self.status = status


def execute(command: list[str], directory: Path, timeout: float) -> dict:
    """Hold at READY for RSS; bound the whole process lifetime; retain stderr."""
    started = time.monotonic()
    with (directory / "stderr.log").open("w") as stderr:
        proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=stderr, text=True, start_new_session=True)
        responses: queue.Queue = queue.Queue()

        def read_ready():
            try:
                responses.put(proc.stdout.readline())
            except Exception as e:
                responses.put(e)

        reader = threading.Thread(target=read_ready, daemon=True)
        reader.start()
        try:
            try:
                line = responses.get(timeout=timeout)
            except queue.Empty as e:
                raise JobFailure("timeout", "timed out loading graph") from e
            if isinstance(line, Exception) or not (line.strip() == "READY" or line.startswith("READY,")):
                raise JobFailure("error", f"benchmark did not signal READY: {line!r}")
            # The collector samples its current RSS before any algorithm or
            # export allocation. Legacy/fake workers can use external probing.
            memory = int(line.strip().split(",")[1]) if line.startswith("READY,") else rss_bytes(proc.pid)
            remaining = timeout - (time.monotonic() - started)
            if remaining <= 0:
                raise JobFailure("timeout", "time budget exhausted loading graph")
            try:
                out, _ = proc.communicate("\n", timeout=remaining)
            except subprocess.TimeoutExpired as e:
                raise JobFailure("timeout", "timed out collecting exports") from e
            if proc.returncode:
                raise JobFailure("error", f"collector exited with code {proc.returncode}")
            if out.strip():
                raise JobFailure("error", f"unexpected stdout: {out[:200]}")
            return {"graph_rss_bytes": memory, "wall_seconds": time.monotonic() - started}
        finally:
            if proc.poll() is None:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass  # The process may exit between poll and killpg.
            proc.wait()
            reader.join(timeout=1)
            proc.stdin.close()
            proc.stdout.close()


def compress_csvs(directory: Path) -> None:
    """Stream large exports instead of holding vertex/edge rows in Python."""
    for path in directory.glob("*.csv"):
        # Small summary tables remain immediately inspectable.
        if path.name not in {"vertices.csv", "neighborhood_nodes.csv", "neighborhood_edges.csv",
                             "overview_nodes.csv", "overview_edges.csv"}:
            continue
        with path.open("rb") as src, path.with_suffix(".csv.gz").open("wb") as raw:
            # Fixed gzip timestamp makes identical exports byte-reproducible.
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as dst:
                shutil.copyfileobj(src, dst, length=1 << 20)
        path.unlink()


def completed(directory: Path, fingerprint: str) -> bool:
    try:
        record = json.loads((directory / "job.json").read_text())
        if record["fingerprint"] != fingerprint or record["status"] not in {
                "ok", "no_memory", "budget_exceeded"}:
            return False
        # Output hashes detect incomplete, deleted or edited exports on resume.
        return all((directory / name).is_file() and digest(directory / name) == checksum
                   for name, checksum in record.get("artifacts", {}).items())
    except (OSError, ValueError, KeyError):
        return False


def fingerprint(data: dict) -> str:
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()


def memory_estimate(n: int, text_bytes: int, weighted: bool, rep: str, phase: str) -> int:
    # Conservative staging estimate: vector capacity, input edges, offsets,
    # algorithm workspace and (when selected) the dense matrix. This is a
    # preflight estimate, never reported as measured RSS.
    staging = text_bytes * 4 + (n + 2) * (96 if phase == "stats" else 80)
    matrix = ((n + 1) ** 2 * 8 if weighted else (n + 1) * ((n + 64) // 64) * 8)
    return staging + (matrix if rep == "matrix" else 0)


def jobs(cfg: dict, phases: list[str], selected_algorithms=None, selected_representations=None):
    if "stats" in phases:
        yield "stats", "csr", ""
    if "cases" in phases:
        yield "cases", "csr", ""
    if "timing" in phases:
        algorithms = ["bfs", "dfs"]
        if cfg["weighting"] == "weighted":
            algorithms += ["dijkstra_array", "dijkstra_heap", "dijkstra_lazy"]
        if selected_algorithms is not None:
            algorithms = [a for a in algorithms if a in selected_algorithms]
        for rep in (selected_representations or cfg["representations"].split("|")):
            for algorithm in algorithms:
                yield "timing", rep, algorithm


def baseline(args, weighting: str, rep: str, binary_hash: str,
             run_metadata: dict) -> int:
    """One-vertex process RSS, cached only for the same binary and host."""
    directory = args.out / "baselines" / f"{weighting}_{rep}"
    fp = fingerprint({"binary": binary_hash, "environment": run_metadata["environment_fingerprint"]})
    try:
        saved = json.loads((directory / "baseline.json").read_text())
        if saved["fingerprint"] == fp:
            return saved["rss_bytes"]
    except (OSError, ValueError, KeyError):
        pass
    directory.mkdir(parents=True, exist_ok=True)
    tiny = directory / "input.txt"
    tiny.write_text("1\n")
    measured = execute([str(args.binary), str(tiny), weighting, rep, "mem", str(directory)],
                       directory, args.timeout)
    value = measured["graph_rss_bytes"]
    write_json(directory / "baseline.json", {"fingerprint": fp, "rss_bytes": value,
                                             "run": run_metadata})
    return value


def collect_graph(cfg: dict, args, binary_hash: str, run_metadata: dict) -> bool:
    graph_name = cfg["graph"]
    graph_out = args.out / graph_name
    graph_out.mkdir(parents=True, exist_ok=True)
    success = True
    with contextlib.ExitStack() as stack:
        source = find_input(args.data, graph_name)
        print(f"{graph_name}: reading input/checksum", flush=True)
        graph, graph_hash = stack.enter_context(plain_input(source, args.tmp))
        n = vertex_count(graph)
        labels, label_hash = None, None
        if cfg.get("labels"):
            labels, label_hash = stack.enter_context(plain_input(find_input(args.data, cfg["labels"]), args.tmp))
        write_json(graph_out / "input.json", {
            "graph": graph_name, "weighting": cfg["weighting"], "path": str(source.resolve()),
            "sha256_plaintext": graph_hash, "labels_sha256_plaintext": label_hash,
            "vertices": n, "text_bytes": graph.stat().st_size,
            "implementation": "gl_header_only", "self_loops": "dropped_and_counted",
            "parallel_edges": "CSR keeps all; matrix keeps minimum weight or one unweighted edge",
        })
        for phase, rep, algorithm in jobs(cfg, args.phases, args.algorithms, args.representations):
            key = phase if phase != "timing" else f"timing_{rep}_{algorithm}"
            final = graph_out / key
            backup = graph_out / ("." + key + ".previous")
            if not final.exists() and backup.exists():
                backup.rename(final)
            repetitions = int(cfg.get("k_" + rep) or args.k) if phase == "timing" and not args.ignore_config_counts else args.k
            settings = {"schema_version": SCHEMA_VERSION, "binary_sha256": binary_hash,
                        "runner_sha256": run_metadata["runner_sha256"],
                        "graph_sha256": graph_hash, "labels_sha256": label_hash,
                        "weighting": cfg["weighting"], "phase": phase, "representation": rep,
                        "algorithm": algorithm, "source": cfg.get("source", ""),
                        "seed": args.seed, "k": repetitions, "sample_nodes": args.sample_nodes,
                        "sample_edges": args.sample_edges, "exact_work": args.exact_work,
                        "array_work": args.array_work, "memory_limit_bytes": args.memory_limit,
                        "timeout_seconds": args.timeout,
                        "environment": run_metadata["environment_fingerprint"]}
            fp = fingerprint(settings)
            if not args.force and completed(final, fp):
                print(f"  {key}: already complete", flush=True)
                continue
            stage = Path(tempfile.mkdtemp(prefix=f".{key}-", dir=graph_out))
            record = {"fingerprint": fp, "settings": settings, "started_at": utc_now(),
                      "run": run_metadata, "status": "error"}
            needed = memory_estimate(n, graph.stat().st_size, cfg["weighting"] == "weighted", rep, phase)
            record["memory_estimate_bytes"] = needed
            try:
                if needed > args.memory_limit:
                    record.update(status="no_memory", reason="preflight memory estimate exceeds budget")
                elif algorithm == "dijkstra_array" and n * n > args.array_work:
                    record.update(status="budget_exceeded", reason="n^2 per-source work exceeds array budget")
                else:
                    base = baseline(args, cfg["weighting"], rep, binary_hash, run_metadata)
                    cmd = [str(args.binary), str(graph), cfg["weighting"], rep, phase, str(stage),
                           "--seed", str(args.seed), "--k", str(repetitions),
                           "--sample-nodes", str(args.sample_nodes), "--sample-edges", str(args.sample_edges),
                           "--exact-work", str(args.exact_work)]
                    if algorithm:
                        cmd += ["--algorithm", algorithm]
                    if cfg.get("source"):
                        cmd += ["--source", cfg["source"]]
                    if labels:
                        cmd += ["--labels", str(labels)]
                    record.update(execute(cmd, stage, args.timeout))
                    record["baseline_rss_bytes"] = base
                    record["graph_rss_minus_baseline_bytes"] = record["graph_rss_bytes"] - base
                    compress_csvs(stage)
                    record["status"] = "ok"
            except JobFailure as e:
                record.update(status=e.status, reason=str(e))
                success = False
            except Exception as e:
                record.update(status="error", reason=str(e))
                success = False
            record["finished_at"] = utc_now()
            record["artifacts"] = {p.name: digest(p) for p in stage.iterdir() if p.is_file()}
            write_json(stage / "job.json", record)
            # The old complete job remains available until its replacement has
            # been fully written; only the final rename publishes this job.
            if backup.exists():
                shutil.rmtree(backup)
            if final.exists():
                final.rename(backup)
            stage.rename(final)
            if backup.exists():
                shutil.rmtree(backup)
            print(f"  {key}: {record['status']}", flush=True)
    if success:
        (graph_out / "failure.json").unlink(missing_ok=True)
    return success


def aggregate(out: Path) -> None:
    """Rebuild derived tables from checkpoints, never append duplicate runs."""
    rows = []
    summaries = []
    def current(path, job):
        if any(part.startswith(".") for part in path.relative_to(out).parts):
            return False
        try:
            source = json.loads((path.parent.parent / "input.json").read_text())
            return (job["settings"]["graph_sha256"] == source["sha256_plaintext"]
                    and job["settings"]["labels_sha256"] == source["labels_sha256_plaintext"])
        except (OSError, ValueError, KeyError):
            return False

    for path in sorted(out.glob("*/stats/summary.csv")):
        job = json.loads((path.parent / "job.json").read_text())
        if job["status"] == "ok" and current(path, job):
            with path.open() as src:
                for row in csv.DictReader(src):
                    summaries.append({"graph": path.parent.parent.name,
                                      "implementation": "gl_header_only",
                                      "binary_sha256": job["settings"]["binary_sha256"], **row})
    for path in sorted(out.glob("*/*/job.json")):
        job = json.loads(path.read_text())
        if not current(path, job):
            continue
        setting = job["settings"]
        timing = path.parent / "timings.csv"
        good = []
        if job["status"] == "ok" and timing.is_file():
            with timing.open() as src:
                sample = list(csv.DictReader(src))
            good = [float(r["elapsed_micros"]) for r in sample if r["status"] == "ok"]
        if setting["phase"] == "timing":
            import statistics
            rows.append({"graph": path.parent.parent.name, "representation": setting["representation"],
                         "implementation": "gl_header_only", "seed": setting["seed"],
                         "requested_sources": setting["k"],
                         "binary_sha256": setting["binary_sha256"],
                         "host": job["run"]["hostname"],
                         "algorithm": setting["algorithm"], "samples": len(good),
                         "mean_micros": statistics.mean(good) if good else "",
                         "median_micros": statistics.median(good) if good else "",
                         "status": job["status"] if job["status"] != "ok" or good else
                                   (sample[0]["status"] if sample else "empty"),
                         "graph_rss_bytes": job.get("graph_rss_bytes", ""),
                         "baseline_rss_bytes": job.get("baseline_rss_bytes", ""),
                         "graph_rss_minus_baseline_bytes": job.get("graph_rss_minus_baseline_bytes", "")})
    for name, fields, data in [
        ("graph_summary.csv", ["graph", "implementation", "binary_sha256", "metric", "value"], summaries),
        ("benchmark_summary.csv", ["graph", "implementation", "binary_sha256", "host", "representation", "algorithm", "seed", "requested_sources", "samples", "mean_micros", "median_micros", "status", "graph_rss_bytes", "baseline_rss_bytes", "graph_rss_minus_baseline_bytes"], rows),
    ]:
        target = out / name
        with target.with_suffix(".csv.tmp").open("w", newline="") as dst:
            writer = csv.DictWriter(dst, fieldnames=fields)
            writer.writeheader(); writer.writerows(data)
        target.with_suffix(".csv.tmp").replace(target)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config", type=Path, default=ROOT / "config_collect.csv")
    p.add_argument("--data", type=Path, default=ROOT / "data")
    p.add_argument("--out", type=Path, default=ROOT / "data" / "collected")
    p.add_argument("--binary", type=Path, default=ROOT / "build" / "collect_stats")
    p.add_argument("--tmp", type=Path, default=Path(tempfile.gettempdir()))
    p.add_argument("--only", action="append")
    p.add_argument("--phases", default="stats,cases,timing")
    p.add_argument("--algorithms", help="Comma-separated timing algorithms; default: all supported")
    p.add_argument("--representations", help="Comma-separated timing representations; default: config")
    p.add_argument("--seed", type=int, default=2025)
    p.add_argument("--k", type=int, default=100)
    p.add_argument("--ignore-config-counts", action="store_true", help="Use --k for every timing job, including short pilots")
    p.add_argument("--sample-nodes", type=int, default=1000)
    p.add_argument("--sample-edges", type=int, default=4000)
    p.add_argument("--exact-work", type=int, default=100000000)
    p.add_argument("--array-work", type=int, default=10000000000)
    p.add_argument("--timeout", type=float, default=3600)
    p.add_argument("--memory-limit-gb", type=float)
    p.add_argument("--force", action="store_true")
    return p


def main(argv=None) -> int:
    p = parser(); args = p.parse_args(argv)
    args.algorithms = args.algorithms.split(",") if args.algorithms else None
    args.representations = args.representations.split(",") if args.representations else None
    if args.algorithms and set(args.algorithms) - {"bfs", "dfs", "dijkstra_array", "dijkstra_heap", "dijkstra_lazy"}:
        p.error("invalid timing algorithms")
    if args.representations and set(args.representations) - {"csr", "matrix"}:
        p.error("invalid timing representations")
    args.phases = args.phases.split(",")
    if not args.phases or set(args.phases) - {"stats", "cases", "timing"}:
        p.error("phases must contain stats, cases and/or timing")
    if not 0 <= args.seed < 2 ** 32 or args.k <= 0 or min(args.sample_nodes, args.sample_edges, args.exact_work, args.array_work) < 0:
        p.error("invalid seed, sample size or work budget")
    if not 0 < args.timeout < float("inf") or (args.memory_limit_gb is not None and not 0 < args.memory_limit_gb < float("inf")):
        p.error("timeout and memory budget must be finite and positive")
    args.binary = args.binary.resolve()
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        p.error("collector missing: run make collector")
    with args.config.open(newline="") as src:
        reader = csv.DictReader(src)
        if not {"graph", "weighting", "representations"} <= set(reader.fieldnames or []):
            p.error("config requires graph, weighting, representations columns")
        configs = list(reader)
    known = {c["graph"] for c in configs}
    if len(known) != len(configs):
        p.error("duplicate graph names in config")
    if args.only and set(args.only) - known:
        p.error("unknown --only graph: " + ", ".join(sorted(set(args.only) - known)))
    configs = [c for c in configs if not args.only or c["graph"] in args.only]
    problems = []
    for cfg in configs:
        try:
            if cfg["weighting"] not in ("weighted", "unweighted") or set(cfg["representations"].split("|")) - {"csr", "matrix"}:
                raise ValueError("invalid weighting or representations")
            for rep in (args.representations or cfg["representations"].split("|")):
                if int(cfg.get("k_" + rep) or args.k) <= 0:
                    raise ValueError("timing sample counts must be positive")
            find_input(args.data, cfg["graph"])
            if cfg.get("labels"):
                find_input(args.data, cfg["labels"])
        except (OSError, ValueError) as e:
            problems.append(str(e))
    if problems:
        print("Cannot start:\n" + "\n".join(problems), file=sys.stderr)
        return 1
    args.tmp.mkdir(parents=True, exist_ok=True)
    args.out.mkdir(parents=True, exist_ok=True)
    # One writer per output directory: checkpoint and aggregate publication
    # must not race another collector targeting the same directory.
    with (args.out / ".collection.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("another collector is using this output directory", file=sys.stderr)
            return 1
        # Freeze the executable so rebuilding during a long run cannot mix binaries.
        with tempfile.TemporaryDirectory(prefix="cos242-worker-", dir=args.tmp) as worker_dir:
            original_binary = args.binary
            binary_hash_before_copy = digest(original_binary)
            args.binary = Path(worker_dir) / "collect_stats"
            shutil.copy2(original_binary, args.binary)
            if digest(args.binary) != binary_hash_before_copy:
                raise RuntimeError("collector changed while taking executable snapshot; retry")
            args.memory_limit = int(args.memory_limit_gb * 1024 ** 3) if args.memory_limit_gb else available_memory(args.binary)
            binary_hash = digest(args.binary)
            commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True)
            changes = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True)
            compiler = subprocess.run([os.environ.get("CXX", "g++"), "--version"], capture_output=True, text=True)
            run = {"started_at": utc_now(), "commit": commit.stdout.strip(),
                   "runner_sha256": digest(Path(__file__)),
                   "working_tree_status": changes.stdout, "binary_sha256": binary_hash,
                   "platform": platform.platform(), "machine": platform.machine(),
                   "hostname": platform.node(),
                   "processor": platform.processor(), "cpu_count": os.cpu_count(),
                   "compiler_observed_at_run": compiler.stdout.splitlines()[0] if compiler.stdout else "unknown",
                   "memory_budget_bytes": args.memory_limit, "argv": sys.argv if argv is None else argv}
            run["environment_fingerprint"] = fingerprint({key: run[key] for key in
                ("platform", "machine", "hostname", "processor", "cpu_count", "memory_budget_bytes")})
            failures = 0
            for cfg in configs:
                try:
                    failures += not collect_graph(cfg, args, binary_hash, run)
                except Exception as e:
                    failures += 1
                    write_json(args.out / cfg["graph"] / "failure.json", {"status": "error", "reason": str(e), "run": run})
                    print(f"{cfg['graph']}: FAILED: {e}", file=sys.stderr)
                aggregate(args.out)
            return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
