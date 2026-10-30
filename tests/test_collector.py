"""End-to-end checks for the real collector and its subprocess/checkpoint seam."""
import csv
import fcntl
import gzip
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("collect", ROOT / "analysis/collect.py")
collect = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collect)


class CollectorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cos242-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.data = self.base / "data"
        self.data.mkdir()
        self.out = self.base / "exports"
        self.config = self.base / "config.csv"
        self.binary = Path(os.environ.get("COLLECTOR_BINARY", ROOT / "build/collect_stats"))

    def configure(self, text, weighting="weighted", source="1", compressed=False, labels=""):
        path = self.data / ("tiny.txt.gz" if compressed else "tiny.txt")
        if compressed:
            path.write_bytes(gzip.compress(text.encode()))
        else:
            path.write_text(text)
        with self.config.open("w", newline="") as dst:
            writer = csv.writer(dst)
            writer.writerow(["graph", "weighting", "representations", "labels", "source"])
            writer.writerow(["tiny", weighting, "csr|matrix", labels, source])
        return path

    def run_collect(self, *extra):
        arguments = ["--config", str(self.config), "--data", str(self.data),
                     "--out", str(self.out), "--tmp", str(self.base),
                     "--binary", str(self.binary), "--k", "3", "--sample-nodes", "4",
                     "--sample-edges", "2", "--memory-limit-gb", "1", *extra]
        with mock.patch("sys.stdout", new=io.StringIO()), mock.patch("sys.stderr", new=io.StringIO()):
            return collect.main(arguments)

    def rows(self, job, name):
        path = self.out / "tiny" / job / name
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rt", newline="") as src:
            return list(csv.DictReader(src))

    def job(self, name):
        return json.loads((self.out / "tiny" / name / "job.json").read_text())

    def test_exact_statistics_parallel_edges_loops_components_and_samples(self):
        self.configure("# comment\n\n6\n1 2 1\n2 3 2\n1 3 10\n1 2 4\n4 5 0\n3 3 2\n", compressed=True)
        self.assertEqual(self.run_collect("--phases", "stats"), 0)
        summary = {r["metric"]: float(r["value"]) for r in self.rows("stats", "summary.csv")}
        for name, expected in {"vertices": 6, "edges": 5, "input_edges": 6,
                               "skipped_self_loops": 1, "parallel_edges": 1,
                               "unique_edges": 4, "density_simple": 8 / 30,
                               "degree_min": 0, "degree_max": 3, "degree_mean": 10 / 6,
                               "degree_median": 1.5, "component_count": 3,
                               "component_largest": 3, "component_smallest": 1,
                               "isolated_vertices": 1, "weight_min": 0,
                               "weight_max": 10, "weight_mean": 3.4}.items():
            self.assertAlmostEqual(summary[name], expected, msg=name)
        self.assertEqual(self.rows("stats", "degree_histogram.csv"), [
            {"degree": "0", "vertices": "1"}, {"degree": "1", "vertices": "2"},
            {"degree": "2", "vertices": "1"}, {"degree": "3", "vertices": "2"}])
        self.assertEqual([r["vertices"] for r in self.rows("stats", "components.csv")], ["3", "2", "1"])
        self.assertEqual(len(self.rows("stats", "vertices.csv.gz")), 6)
        self.assertEqual(sum(int(r["edges"]) for r in self.rows("stats", "weight_histogram.csv")), 5)
        diameters = self.rows("stats", "diameter.csv")
        self.assertEqual(diameters[0]["value"], "1")  # triangle, pair, singleton
        for sample in ["neighborhood", "overview"]:
            nodes = self.rows("stats", sample + "_nodes.csv.gz")
            edges = self.rows("stats", sample + "_edges.csv.gz")
            self.assertLessEqual(len(nodes), 4)
            self.assertLessEqual(len(edges), 2)
            ids = {r["id"] for r in nodes}
            self.assertTrue(all(e["source"] in ids and e["target"] in ids for e in edges))
        self.assertFalse(list(self.base.glob("cos242-input-*")))

    def test_raw_sources_match_across_representations_and_resume(self):
        self.configure("6\n1 2\n2 3\n4 5\n", weighting="unweighted")
        self.assertEqual(self.run_collect(), 0)
        jobs = ["timing_" + rep + "_" + alg for rep in ["csr", "matrix"] for alg in ["bfs", "dfs"]]
        sources = [[r["source"] for r in self.rows(j, "timings.csv")] for j in jobs]
        self.assertTrue(all(s == sources[0] for s in sources))
        self.assertEqual(len(set(sources[0])), 3)
        paths = [self.out / "tiny" / j / "job.json" for j in jobs]
        before = [p.stat().st_mtime_ns for p in paths]
        self.assertEqual(self.run_collect(), 0)
        self.assertEqual(before, [p.stat().st_mtime_ns for p in paths])
        with (self.out / "benchmark_summary.csv").open() as src:
            summary = list(csv.DictReader(src))
        self.assertEqual(len(summary), 4)
        self.assertTrue(all(int(r["samples"]) == 3 for r in summary))
        self.assertTrue(all("baseline_rss_bytes" in self.job(j) for j in jobs))
        # A deleted artifact invalidates only that checkpoint.
        (self.out / "tiny" / jobs[0] / "timings.csv").unlink()
        self.assertEqual(self.run_collect(), 0)
        self.assertNotEqual(before[0], paths[0].stat().st_mtime_ns)
        self.assertEqual(before[1:], [p.stat().st_mtime_ns for p in paths[1:]])

    def test_changed_input_invalidates_checkpoint_and_stale_aggregates(self):
        path = self.configure("3\n1 2\n", weighting="unweighted")
        self.assertEqual(self.run_collect(), 0)
        old = self.job("stats")["fingerprint"]
        path.write_text("3\n1 2\n2 3\n")
        self.assertEqual(self.run_collect("--phases", "stats"), 0)
        self.assertNotEqual(self.job("stats")["fingerprint"], old)
        with (self.out / "benchmark_summary.csv").open() as src:
            self.assertEqual(list(csv.DictReader(src)), [])

    def test_memory_and_array_budgets_are_explicit(self):
        self.configure("3\n1 2 1\n")
        self.assertEqual(self.run_collect("--phases", "timing", "--array-work", "0"), 0)
        self.assertEqual(self.job("timing_csr_dijkstra_array")["status"], "budget_exceeded")
        self.assertEqual(self.run_collect("--phases", "stats", "--memory-limit-gb", ".000000001"), 0)
        self.assertEqual(self.job("stats")["status"], "no_memory")

    def test_negative_loop_and_negative_edge_refuse_dijkstra(self):
        for edge in ["1 1 -1", "1 2 -1"]:
            self.configure("3\n1 2 2\n" + edge + "\n")
            self.assertEqual(self.run_collect("--phases", "timing"), 0)
            rows = self.rows("timing_csr_dijkstra_heap", "timings.csv")
            self.assertEqual({r["status"] for r in rows}, {"not_implemented"})
            self.assertTrue(all(r["elapsed_micros"] == "" for r in rows))

    def test_collaboration_names_paths_and_unreachable(self):
        self.configure("6\n1 3 2\n3 4 .5\n4 5 1\n5 6 .25\n", labels="names", source="Edsger W. Dijkstra")
        (self.data / "names.txt").write_text("1,Edsger W. Dijkstra\n2,Alan M. Turing\n3,J. B. Kruskal\n4,Jon M. Kleinberg\n5,Éva Tardos\n6,Daniel R. Figueiredo\n")
        self.assertEqual(self.run_collect("--phases", "stats,cases"), 0)
        rows = self.rows("cases", "cases.csv")
        self.assertEqual(rows[0]["status"], "unreachable")
        self.assertEqual(rows[0]["distance"], "inf")
        self.assertEqual(rows[3]["target_name"], "Éva Tardos")
        self.assertEqual(float(rows[4]["distance"]), 3.75)
        self.assertEqual(rows[4]["path"].split(), ["1", "3", "4", "5", "6"])
        self.assertEqual(rows[4]["hops"], "4")

    def test_weighted_numeric_case_studies(self):
        self.configure("60\n10 20 1.5\n20 30 .25\n30 40 0\n40 50 2\n", source="10")
        self.assertEqual(self.run_collect("--phases", "cases"), 0)
        rows = self.rows("cases", "cases.csv")
        self.assertEqual([r["distance"] for r in rows], ["1.5", "1.75", "1.75", "3.75", "inf"])
        self.assertEqual(rows[3]["path"].split(), ["10", "20", "30", "40", "50"])

    def test_unweighted_worksheet_cases(self):
        self.configure("30\n1 10\n10 20\n20 30\n2 20\n3 30\n", weighting="unweighted")
        self.assertEqual(self.run_collect("--phases", "cases"), 0)
        rows = self.rows("cases", "cases.csv")
        values = {(r["algorithm"], r["source"], r["target"]): r for r in rows}
        self.assertEqual(len(rows), 21)
        self.assertEqual(values["bfs", "1", "10"]["parent"], "1")
        self.assertEqual(values["bfs_distance", "10", "30"]["distance"], "2")
        self.assertEqual(values["dfs", "1", "30"]["distance"], "")

    def test_singleton_empty_and_exact_budget(self):
        for text, n in [("1\n", 1), ("0\n", 0)]:
            self.configure(text, weighting="unweighted", source="")
            self.assertEqual(self.run_collect("--phases", "stats"), 0)
            summary = {r["metric"]: r["value"] for r in self.rows("stats", "summary.csv")}
            self.assertEqual(int(summary["vertices"]), n)
        self.configure("3\n1 2\n2 3\n", weighting="unweighted")
        self.assertEqual(self.run_collect("--phases", "stats", "--exact-work", "0"), 0)
        rows = self.rows("stats", "diameter.csv")
        self.assertEqual(rows[0]["status"], "budget_exceeded")
        self.assertEqual(rows[1]["value"], "2")

    def test_finite_weight_mean_does_not_overflow(self):
        self.configure("3\n1 2 1e308\n2 3 1e308\n")
        self.assertEqual(self.run_collect("--phases", "stats"), 0)
        summary = {r["metric"]: r["value"] for r in self.rows("stats", "summary.csv")}
        self.assertEqual(float(summary["weight_mean"]), 1e308)

    def test_weight_bins_handle_extreme_opposite_signs(self):
        self.configure("3\n1 2 -1e308\n2 3 1e308\n")
        self.assertEqual(self.run_collect("--phases", "stats"), 0)
        bins = self.rows("stats", "weight_histogram.csv")
        self.assertEqual(int(bins[0]["edges"]), 1)
        self.assertEqual(int(bins[-1]["edges"]), 1)

    def test_malformed_input_fails_without_success_checkpoint(self):
        self.configure("3\n1 2 1\n2 broken\n")
        self.assertEqual(self.run_collect("--phases", "stats"), 1)
        self.assertEqual(self.job("stats")["status"], "error")
        self.assertFalse((self.out / "tiny/stats/summary.csv").exists())

    def test_corrupt_gzip_and_missing_inputs_fail(self):
        path = self.configure("3\n1 2 1\n", compressed=True)
        buf = bytearray(path.read_bytes()); buf[-8] ^= 1; path.write_bytes(buf)
        self.assertEqual(self.run_collect("--phases", "stats"), 1)
        self.assertFalse(list(self.base.glob("cos242-input-*")))
        self.assertFalse((self.out / "tiny/stats/job.json").exists())
        path.unlink()
        self.assertEqual(self.run_collect("--phases", "stats"), 1)

    def test_concurrent_writer_is_rejected(self):
        self.configure("3\n1 2 1\n")
        self.out.mkdir()
        with (self.out / ".collection.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertEqual(self.run_collect("--phases", "stats"), 1)
        self.assertFalse((self.out / "tiny/stats/job.json").exists())

    def fake(self, body):
        path = self.base / "fake"
        path.write_text("#!/bin/sh\n" + body)
        path.chmod(0o755)
        return path

    def test_execute_rejects_nonzero_after_ready(self):
        exe = self.fake("printf 'READY\\n'\nread line\necho 'failure' >&2\nexit 7\n")
        with mock.patch.object(collect, "rss_bytes", return_value=1):
            with self.assertRaisesRegex(collect.JobFailure, "code 7"):
                collect.execute([str(exe)], self.base, 5)
        self.assertIn("failure", (self.base / "stderr.log").read_text())

    def test_execute_timeout_before_and_after_ready(self):
        for body in ["sleep 10\n", "printf 'READY\\n'\nread line\nsleep 10\n"]:
            exe = self.fake(body)
            with mock.patch.object(collect, "rss_bytes", return_value=1):
                with self.assertRaises(collect.JobFailure) as e:
                    collect.execute([str(exe)], self.base, .1)
            self.assertEqual(e.exception.status, "timeout")

    def test_sample_exports_are_reproducible(self):
        self.configure("6\n1 2 1\n1 3 2\n2 3 3\n3 4 4\n4 5 5\n5 6 6\n")
        self.assertEqual(self.run_collect("--phases", "stats"), 0)
        names = ["neighborhood_nodes.csv.gz", "neighborhood_edges.csv.gz", "overview_nodes.csv.gz", "overview_edges.csv.gz"]
        before = [(self.out / "tiny/stats" / name).read_bytes() for name in names]
        self.assertEqual(self.run_collect("--phases", "stats", "--force"), 0)
        self.assertEqual(before, [(self.out / "tiny/stats" / name).read_bytes() for name in names])


if __name__ == "__main__":
    unittest.main()
