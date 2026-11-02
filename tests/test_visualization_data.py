"""Independent small fixtures for visualization data/provenance and unit conversion."""
import csv
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("report", ROOT / "analysis/visualization_data.py")
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class VisualizationDataTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cos242-visualization-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.collected = self.base / "collected"
        self.azure = self.base / "azure"
        self.data = self.base / "data"
        self.azure.mkdir(); self.data.mkdir()
        self.stats = self.collected / "tiny/stats"
        self.cases = self.collected / "tiny/cases"
        self.stats.mkdir(parents=True); self.cases.mkdir()
        self.config = self.base / "config.csv"
        self.config.write_text("graph,weighting,representations,labels,source\ntiny,weighted,csr,,1\n")
        (self.collected / "tiny/input.json").write_text(json.dumps({"sha256_plaintext": "abc", "labels_sha256_plaintext": None}))
        summary = {"vertices": 3, "edges": 2, "parallel_edges": 0, "unique_edges": 2,
                   "density_simple": 2/3, "degree_mean": 4/3, "degree_median": 1,
                   "degree_min": 1, "degree_max": 2, "isolated_vertices": 0,
                   "component_count": 1, "component_largest": 3, "component_smallest": 3,
                   "weight_min": 1, "weight_max": 2, "weight_mean": 1.5, "zero_weight_edges": 0}
        self.csv(self.stats / "summary.csv", ["metric", "value"], list(summary.items()))
        self.csv(self.stats / "degree_histogram.csv", ["degree", "vertices"], [(1, 2), (2, 1)])
        self.csv(self.stats / "component_histogram.csv", ["vertices", "components"], [(3, 1)])
        self.csv(self.stats / "weight_histogram.csv", ["lower", "upper", "edges"], [(1, 2, 2)])
        self.csv(self.stats / "diameter.csv", ["method", "scope", "distance_kind", "value", "status", "work_estimate"], [("exact", "all_components", "hops", 2, "ok", 21)])
        for view in ("neighborhood", "overview"):
            self.csv(self.stats / f"{view}_nodes.csv.gz", ["id", "label", "degree", "component", "component_size"], [(1, "", 1, 1, 3), (2, "", 2, 1, 3), (3, "", 1, 1, 3)])
            self.csv(self.stats / f"{view}_edges.csv.gz", ["source", "target", "weight"], [(1, 2, 1), (2, 3, 2)])
            self.csv(self.stats / f"{view}_sample.csv", ["method", "seed", "nodes", "node_limit", "induced_edges", "exported_edges", "edge_limit", "seeds"], [("bounded_bfs", 2025, 3, 3, 2, 2, 2, "1 ")])
        self.csv(self.cases / "cases.csv", ["algorithm", "source", "target", "source_name", "target_name", "parent", "distance", "hops", "path", "status"], [("dijkstra_heap", 1, 3, "", "", 2, 3, 2, "1 2 3 ", "ok")])
        self.seal(self.stats); self.seal(self.cases)

    def csv(self, path, header, rows):
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "wt", encoding="utf-8", newline="") as dst:
            writer = csv.writer(dst); writer.writerow(header); writer.writerows(rows)

    def seal(self, directory):
        record = {"status": "ok", "settings": {"graph_sha256": "abc", "labels_sha256": None, "binary_sha256": "def"},
                  "run": {"commit": "12345678", "working_tree_status": ""}, "finished_at": "2026-10-30T20:00:00+00:00",
                  "artifacts": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir() if p.name != "job.json"}}
        (directory / "job.json").write_text(json.dumps(record))

    def build(self):
        return report.load_data(self.collected, self.azure, self.data, self.config)

    def azure_rows(self, rows):
        self.csv(self.azure / "results_weighted.csv", ["graph", "representation", "metric", "value_average", "value_median", "samples", "status"], rows)

    def test_exact_small_graph(self):
        model = self.build()
        self.assertEqual(len(model["graphs"]), 1)
        graph = model["graphs"][0]
        self.assertEqual(graph["degrees"], [[1, 2], [2, 1]])
        self.assertAlmostEqual(graph["summary"]["degree_mean"], 4/3)
        self.assertEqual(graph["cases"][0]["path"], [1, 2, 3])
        self.assertFalse(model["azure"]["available"])

    def test_long_dfs_path_field_exceeds_default_csv_limit(self):
        path = self.base / "long.csv"
        text = "1 " * 100000
        self.csv(path, ["path"], [(text,)])
        csv.field_size_limit(131072)
        self.assertEqual(report.read_rows(path)[0]["path"], text)

    def test_unweighted_dfs_has_depth_but_no_shortest_distance(self):
        self.config.write_text(self.config.read_text().replace("weighted", "unweighted"))
        (self.stats / "weight_histogram.csv").unlink()
        self.csv(self.cases / "cases.csv", ["algorithm", "source", "target", "source_name", "target_name", "parent", "distance", "hops", "path", "status"], [
            ("bfs", 1, 3, "", "", 2, 2, 2, "1 2 3", "ok"),
            ("dfs", 1, 3, "", "", 2, "", 2, "1 2 3", "ok")])
        self.seal(self.stats); self.seal(self.cases)
        graph = self.build()["graphs"][0]
        self.assertEqual(graph["weights"], [])
        self.assertEqual(graph["weighting"], "unweighted")
        self.assertIsNone(graph["cases"][1]["distance"])
        self.assertEqual(graph["cases"][1]["hops"], 2)

    def test_azure_seconds_and_missing_array_preserved(self):
        self.azure_rows([("tiny", "csr", "vertices", 3, 3, 1, "ok"),
                         ("tiny", "csr", "edges", 2, 2, 1, "ok"),
                         ("tiny", "csr", "dijkstra_heap_micros", 2500000, 2000000, 100, "ok"),
                         ("tiny", "csr", "dijkstra_lazy_micros", 1000000, 900000, 100, "ok"),
                         ("tiny", "csr", "dijkstra_array_micros", "", "", 0, "skipped"),
                         ("tiny", "csr", "memory_mb_graph", 10, 10, 1, "ok"),
                         ("tiny", "matrix", "memory_mb_estimate", 20000, 20000, 0, "no_memory")])
        model = self.build()["azure"]["graphs"]["tiny"]
        self.assertAlmostEqual(model["dijkstra_heap"]["mean_seconds"], 2.5)
        self.assertAlmostEqual(model["dijkstra_heap"]["median_seconds"], 2)
        self.assertEqual(model["dijkstra_heap"]["samples"], 100)
        self.assertEqual(model["dijkstra_array"]["status"], "skipped")
        self.assertNotIn("mean_seconds", model["dijkstra_array"])
        self.assertEqual(model["csr_mib"], 10)
        self.assertEqual(model["matrix_estimate_mib"], 20000)
        self.assertIsNone(model["matrix_mib"])

    def test_duplicate_or_wrong_graph_azure_data_is_rejected(self):
        for rows in [[("tiny", "csr", "vertices", 99, 99, 1, "ok")],
                     [("tiny", "csr", "vertices", 3, 3, 1, "ok")] * 2]:
            self.azure_rows(rows)
            with self.assertRaises(ValueError): self.build()

    def test_disagreeing_azure_path_is_rejected(self):
        self.azure_rows([])
        self.csv(self.azure / "paths_weighted.csv", ["graph", "source", "target", "distance", "status"], [("tiny", 1, 3, 4, "ok")])
        with self.assertRaisesRegex(ValueError, "disagree"): self.build()

    def test_corrupt_artifact_and_stale_input_are_rejected(self):
        path = self.stats / "summary.csv"
        path.write_text(path.read_text()+"extra,1\n")
        with self.assertRaisesRegex(ValueError, "changed since"): self.build()
        self.seal(self.stats)
        (self.collected / "tiny/input.json").write_text(json.dumps({"sha256_plaintext": "other", "labels_sha256_plaintext": None}))
        with self.assertRaisesRegex(ValueError, "different input"): self.build()

    def test_inconsistent_histogram_and_network_endpoints_are_rejected(self):
        path = self.stats / "degree_histogram.csv"
        self.csv(path, ["degree", "vertices"], [(1, 3)])
        self.seal(self.stats)
        with self.assertRaisesRegex(ValueError, "degree total"): self.build()
        self.csv(path, ["degree", "vertices"], [(1, 2), (2, 1)])
        self.csv(self.stats / "neighborhood_edges.csv.gz", ["source", "target", "weight"], [(1, 9, 1), (2, 3, 2)])
        self.seal(self.stats)
        with self.assertRaisesRegex(ValueError, "missing endpoints"): self.build()

    def test_unicode_labels_are_preserved(self):
        text = '</script><script>alert("x")</script> Éva'
        self.csv(self.stats / "neighborhood_nodes.csv.gz", ["id", "label", "degree", "component", "component_size"], [(1, text, 1, 1, 3), (2, "", 2, 1, 3), (3, "", 1, 1, 3)])
        self.seal(self.stats)
        model = self.build()
        self.assertEqual(model["graphs"][0]["networks"]["neighborhood"]["nodes"][0]["label"], text)

    def test_nonfinite_stats_and_failed_jobs_are_rejected(self):
        path = self.stats / "summary.csv"
        path.write_text(path.read_text().replace('vertices,3', 'vertices,nan'))
        self.seal(self.stats)
        with self.assertRaisesRegex(ValueError, "finite"): self.build()
        record = json.loads((self.stats / "job.json").read_text());record["status"] = "timeout"
        (self.stats / "job.json").write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "did not succeed"): self.build()


if __name__ == "__main__": unittest.main()
