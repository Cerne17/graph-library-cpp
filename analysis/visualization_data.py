"""Load and validate saved course graph data for visualization scripts."""
from __future__ import annotations

import csv
import gzip
import hashlib
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
ALGORITHMS = ("dijkstra_heap", "dijkstra_lazy", "dijkstra_array")


def read_rows(path: Path) -> list[dict]:
    # DFS paths can contain hundreds of thousands of vertex IDs in one field.
    csv.field_size_limit(128 * 1024 * 1024)
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8", newline="") as src:
        return list(csv.DictReader(src))


def number(value: str) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"expected a finite statistic, got {value!r}")
    return result


def checkpoint(directory: Path) -> dict:
    record = json.loads((directory / "job.json").read_text())
    if record["status"] != "ok":
        raise ValueError(f"{directory}: collection did not succeed")
    for name, expected in record["artifacts"].items():
        path = directory / name
        if not path.is_file():
            raise ValueError(f"{path}: missing checkpoint artifact")
        # Full per-vertex exports are not embedded or read by this report.
        if name == "vertices.csv.gz":
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"{path}: changed since collection")
    return record


def network(directory: Path, view: str) -> dict:
    nodes = [{"id": int(r["id"]), "label": r["label"], "degree": int(r["degree"]),
              "component": int(r["component"]), "component_size": int(r["component_size"])}
             for r in read_rows(directory / f"{view}_nodes.csv.gz")]
    edges = [[int(r["source"]), int(r["target"]), number(r["weight"])]
             for r in read_rows(directory / f"{view}_edges.csv.gz")]
    meta = read_rows(directory / f"{view}_sample.csv")[0]
    ids = {n["id"] for n in nodes}
    if len(ids) != len(nodes) or any(u not in ids or v not in ids for u, v, _ in edges):
        raise ValueError("network sample contains duplicate nodes or missing endpoints")
    if len(nodes) != int(meta["nodes"]) or len(edges) != int(meta["exported_edges"]):
        raise ValueError("network sample counts differ from metadata")
    return {"nodes": nodes, "edges": edges, "method": meta["method"],
            "induced_edges": int(meta["induced_edges"]), "seed": int(meta["seed"]),
            "node_limit": int(meta["node_limit"]), "edge_limit": int(meta["edge_limit"]),
            "seeds": [int(v) for v in meta["seeds"].split()]}


def path_labels(data: Path, stem: str, wanted: set[int]) -> dict[int, str]:
    if not stem:
        return {}
    for suffix in (".txt", ".txt.gz"):
        path = data / (stem + suffix)
        if not path.is_file():
            continue
        result = {}
        opener = gzip.open if suffix.endswith("gz") else open
        with opener(path, "rt", encoding="utf-8") as src:
            for line in src:
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                vertex, name = line.rstrip("\r\n").split(",", 1)
                if int(vertex) in wanted:
                    result[int(vertex)] = name
        return result
    return {}  # IDs remain usable if optional name inputs are unavailable.


def graph_data(cfg: dict, collected: Path, data: Path) -> dict:
    name = cfg["graph"]
    directory = collected / name
    source = json.loads((directory / "input.json").read_text())
    stats = checkpoint(directory / "stats")
    case_job = checkpoint(directory / "cases")
    for job in (stats, case_job):
        if job["settings"]["graph_sha256"] != source["sha256_plaintext"]:
            raise ValueError(f"{name}: checkpoint is for a different input")
        if job["settings"]["labels_sha256"] != source["labels_sha256_plaintext"]:
            raise ValueError(f"{name}: checkpoint is for different labels")
    summary = {r["metric"]: number(r["value"]) for r in read_rows(directory / "stats/summary.csv")}
    degrees = [[int(r["degree"]), int(r["vertices"])] for r in read_rows(directory / "stats/degree_histogram.csv")]
    components = [[int(r["vertices"]), int(r["components"])] for r in read_rows(directory / "stats/component_histogram.csv")]
    if sum(count for _, count in degrees) != summary["vertices"]:
        raise ValueError(f"{name}: degree frequencies do not sum to vertex count")
    if sum(degree * count for degree, count in degrees) != 2 * summary["edges"]:
        raise ValueError(f"{name}: degree total differs from twice the edge count")
    if sum(size * count for size, count in components) != summary["vertices"]:
        raise ValueError(f"{name}: component membership totals differ")
    weighted = cfg["weighting"] == "weighted"
    weights = ([[number(r["lower"]), number(r["upper"]), int(r["edges"])]
                for r in read_rows(directory / "stats/weight_histogram.csv")] if weighted else [])
    if weighted and sum(count for _, _, count in weights) != summary["edges"]:
        raise ValueError(f"{name}: weight histogram differs from edge count")
    cases = []
    for r in read_rows(directory / "cases/cases.csv"):
        distance = None if not r["distance"] or r["distance"] == "inf" else number(r["distance"])
        path = [int(v) for v in r["path"].split()]
        source_id, target = int(r["source"]), int(r["target"])
        hops = int(r["hops"]) if r["hops"] else None
        if r["status"] == "ok" and (not path or path[0] != source_id or path[-1] != target or hops != len(path) - 1 or (r["algorithm"] != "dfs" and distance is None)):
            raise ValueError(f"{name}: invalid case-study path")
        cases.append({"algorithm": r["algorithm"], "source": source_id, "target": target, "source_name": r["source_name"],
                      "target_name": r["target_name"], "distance": distance,
                      "hops": hops, "path": path, "status": r["status"]})
    wanted = {v for r in cases for v in [r["source"], r["target"], *r["path"]]}
    labels = path_labels(data, cfg.get("labels", ""), wanted)
    for case in cases:
        labels.setdefault(case["source"], case["source_name"])
        labels.setdefault(case["target"], case["target_name"])
    return {"id": name, "name": "Collaboration" if name == "rede_colaboracao" else name.removeprefix("grafo_"),
            "weighting": cfg["weighting"], "summary": summary, "degrees": degrees, "components": components, "weights": weights,
            "diameter": read_rows(directory / "stats/diameter.csv"), "cases": cases,
            "path_labels": labels,
            "networks": {view: network(directory / "stats", view) for view in ("neighborhood", "overview")},
            "provenance": {"input_sha256": source["sha256_plaintext"], "binary_sha256": stats["settings"]["binary_sha256"],
                           "library_commit": stats["run"]["commit"],
                           "local_changes": bool(stats["run"].get("working_tree_status", "").strip()),
                           "finished_at": stats["finished_at"]}}


def azure_data(directory: Path, graphs: list[dict], filename="results_weighted.csv", algorithms=ALGORITHMS, environment_file="environment.txt", representation="csr") -> dict:
    result = {"available": False, "environment": "", "graphs": {}}
    path = directory / filename
    if not path.is_file():
        return result
    rows = read_rows(path)
    grouped = {}
    for row in rows:
        key = (row["graph"], row["representation"], row["metric"])
        if key in grouped:
            raise ValueError(f"Azure input has duplicate measurements: {key}")
        grouped[key] = row
    for graph in graphs:
        name = graph["id"]
        for key in ("vertices", "edges"):
            record = grouped.get((name, "csr", key))
            if record and number(record["value_average"]) != graph["summary"][key]:
                raise ValueError(f"{name}: Azure {key} differs from collected graph")
        measurements = {}
        for algorithm in algorithms:
            record = grouped.get((name, representation, algorithm + "_micros"))
            if not record:
                measurements[algorithm] = {"status": "unavailable"}
                continue
            value = {"status": record["status"], "samples": int(record["samples"])}
            if record["status"] == "ok":
                value.update(mean_seconds=number(record["value_average"]) / 1e6,
                             median_seconds=number(record["value_median"]) / 1e6)
                if value["samples"] <= 0 or min(value["mean_seconds"], value["median_seconds"]) < 0:
                    raise ValueError("invalid Azure timing")
            measurements[algorithm] = value
        for key, rep, metric in [("csr_mib", "csr", "memory_mb_graph"),
                                 ("matrix_estimate_mib", "matrix", "memory_mb_estimate"),
                                 ("matrix_mib", "matrix", "memory_mb_graph")]:
            record = grouped.get((name, rep, metric))
            measurements[key] = number(record["value_average"]) if record else None
        result["graphs"][name] = measurements
    case_path = directory / "paths_weighted.csv"
    if case_path.is_file():
        expected = {(r["graph"], int(r["source"]), int(r["target"])): r for r in read_rows(case_path)}
        comparisons = 0
        for graph in graphs:
            for case in graph["cases"]:
                reference = expected.get((graph["id"], case["source"], case["target"]))
                if reference is None:
                    continue
                same = reference["status"] == case["status"]
                if case["status"] == "ok":
                    same = same and math.isclose(number(reference["distance"]), case["distance"], rel_tol=1e-9, abs_tol=1e-9)
                if not same:
                    raise ValueError("Azure and current case-study results disagree")
                comparisons += 1
        result["path_comparisons"] = comparisons
    env = directory / environment_file
    result["environment"] = env.read_text() if env.is_file() else "Environment metadata unavailable."
    match = re.search(r"commit:\s*(\S+)", result["environment"])
    result["commit"] = match.group(1) if match else "unknown"
    result["available"] = True
    return result


def load_data(collected: Path, azure: Path, data: Path, config: Path) -> dict:
    with config.open(newline="") as src:
        configs = list(csv.DictReader(src))
    if not configs or any(c["weighting"] not in {"weighted", "unweighted"} for c in configs):
        raise ValueError("visualizations require a nonempty valid graph configuration")
    graphs = [graph_data(c, collected, data) for c in configs]
    return {"schema_version": 1, "graphs": graphs, "azure": azure_data(azure, graphs)}
