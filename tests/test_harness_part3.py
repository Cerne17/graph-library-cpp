"""Part 3 program and harness tests (make test-part3).

The binary tests run the checked build named by PART3_BINARY (default
build/benchmark_part3_checked) on hand-built fixtures whose expected rows are
written below, worked out by hand. The harness tests use fake launch files
and a fake binary.
"""
import importlib.util
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location(
    "harness_part3", ROOT / "analysis/harness_part3.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)

BINARY = Path(os.environ.get("PART3_BINARY",
                             ROOT / "build/benchmark_part3_checked"))
SMALL = ROOT / "tests/data/part3_small.txt"
CYCLE = ROOT / "tests/data/part3_cycle.txt"


def run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([str(BINARY), *args], capture_output=True, text=True)


def rows(out: str) -> list[list[str]]:
    return [harness.parse_row(line) for line in out.splitlines() if line]


def paths(out: str) -> dict[tuple[str, str], tuple[str, str, str]]:
    """(source, target) -> (distance, path, status)"""
    return {(r[2], r[3]): (r[4], r[6], r[7]) for r in rows(out) if r[0] == "path"}


@unittest.skipUnless(BINARY.is_file(), f"{BINARY} not built")
class BinaryTests(unittest.TestCase):
    def test_bf_toward_100(self):
        # 10 -> 100 (1.25); 20 -> 10 -> 100 (0.50 + 1.25); 30 has no arcs
        out = run(str(SMALL), "--directed", "yes", "bf", "--target", "100",
                  "--sources", "10,20,30", "--runs", "1")
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(paths(out.stdout), {
            ("10", "100"): ("1.25", "10 100", "ok"),
            ("20", "100"): ("1.75", "20 10 100", "ok"),
            ("30", "100"): ("inf", "", "unreachable"),
        })

    def test_fw_set(self):
        # only cycle 10 -> 100 -> 50 -> 40 -> 20 -> 10 (4.50); 30 isolated
        out = run(str(SMALL), "--directed", "yes", "fw", "--set",
                  "10,20,30,40,50", "--runs", "1")
        self.assertEqual(out.returncode, 0, out.stderr)
        got = {k: v[0] for k, v in paths(out.stdout).items()}
        expected = {
            ("10", "20"): "4.00", ("10", "40"): "4.25", ("10", "50"): "2.25",
            ("20", "10"): "0.50", ("20", "40"): "4.75", ("20", "50"): "2.75",
            ("40", "10"): "0.25", ("40", "20"): "-0.25", ("40", "50"): "2.50",
            ("50", "10"): "2.25", ("50", "20"): "1.75", ("50", "40"): "2.00",
        }
        for i in ("10", "20", "30", "40", "50"):
            for j in ("10", "20", "30", "40", "50"):
                if i != j and "30" in (i, j):
                    expected[(i, j)] = "inf"
        self.assertEqual(got, expected)
        self.assertEqual(paths(out.stdout)[("40", "50")][1], "40 20 10 100 50")

    def test_negative_cycle_fixture(self):
        # 2 -> 3 (-3), 3 -> 2 (1): cycle -2.00, reaches 4 through 3 -> 4
        out = run(str(CYCLE), "--directed", "yes", "cycle", "--runs", "1")
        cycle = [r for r in rows(out.stdout) if r[0] == "cycle"][0]
        self.assertEqual(cycle[1], "yes")
        self.assertEqual(sorted(cycle[4].split()), ["2", "3"])
        self.assertEqual(cycle[3], "-2.00")
        self.assertEqual(cycle[5], "ok")
        check = [r for r in rows(out.stdout) if r[0] == "check"][0]
        self.assertEqual(check[1:3], ["witness", "ok"])

        bf = run(str(CYCLE), "--directed", "yes", "bf", "--target", "4",
                 "--sources", "1,5", "--runs", "1")
        self.assertEqual(paths(bf.stdout), {
            ("1", "4"): ("-inf", "", "negative_cycle"),
            ("5", "4"): ("2.00", "5 4", "ok"),
        })

        fw = run(str(CYCLE), "--directed", "yes", "fw", "--set", "1,4,5",
                 "--runs", "1")
        got = paths(fw.stdout)
        self.assertEqual(got[("1", "4")][2], "negative_cycle")
        self.assertEqual(got[("5", "4")][:2], ("2.00", "5 4"))
        self.assertEqual(got[("4", "1")][2], "unreachable")

    def test_budget_zero_gives_one_run_per_step(self):
        out = run(str(SMALL), "--directed", "yes", "bf", "--target", "100",
                  "--sources", "10", "--runs", "10", "--budget-seconds", "0",
                  "--optimizations", "textbook,frontier")
        times = [r[1] for r in rows(out.stdout) if r[0] == "time"]
        self.assertEqual(sorted(times), sorted([
            "reverse", "bellman_ford_pass", "bellman_ford_pass_textbook",
            "bellman_ford_pass_frontier"]))
        self.assertEqual(rows(out.stdout)[-1], ["done", "bf"])

    def test_ten_runs_within_a_large_budget(self):
        out = run(str(SMALL), "--directed", "yes", "cycle", "--runs", "10")
        times = [r for r in rows(out.stdout) if r[0] == "time"]
        self.assertEqual(len(times), 10)

    def test_progress_and_done_rows(self):
        out = run(str(SMALL), "--directed", "yes", "fw", "--set", "10,20",
                  "--runs", "1", "--progress-every", "50")
        kinds = [r[0] for r in rows(out.stdout)]
        self.assertGreaterEqual(kinds.count("progress"), 3)  # k = 50, 100, 120
        self.assertEqual(kinds[-1], "done")

    def test_allpairs_full_and_stopped(self):
        out = run(str(SMALL), "--directed", "yes", "allpairs", "--k", "5")
        info = {r[1]: r[2] for r in rows(out.stdout) if r[0] == "info"}
        self.assertEqual(info["allpairs_full"], "yes")
        self.assertEqual(info["targets_measured"], "120")

        out = run(str(SMALL), "--directed", "yes", "allpairs", "--k", "5",
                  "--budget-seconds", "0")
        info = {r[1]: r[2] for r in rows(out.stdout) if r[0] == "info"}
        self.assertEqual(info["allpairs_full"], "no")
        self.assertEqual(info["targets_measured"], "1")
        passes = [r for r in rows(out.stdout) if r[1] == "allpairs_pass"]
        self.assertEqual(len(passes[0]), 5)  # the target column

    def test_calibrate(self):
        out = run(str(SMALL), "--directed", "yes", "calibrate",
                  "--budget-seconds", "1", "--target", "100")
        kinds = [r[0] for r in rows(out.stdout)]
        self.assertIn("calib", kinds)
        self.assertIn("fit", kinds)
        self.assertIn("time", kinds)
        out = run(str(SMALL), "--directed", "yes", "calibrate",
                  "--budget-seconds", "1", "--no-bf-pass")
        self.assertNotIn("time", [r[0] for r in rows(out.stdout)])

    def test_errors(self):
        missing = run(str(SMALL), "bf", "--target", "100", "--sources", "10")
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn("--directed", missing.stderr)

        out = run(str(SMALL), "--directed", "yes", "bf", "--target", "100",
                  "--sources", "999", "--runs", "1")
        self.assertEqual(paths(out.stdout)[("999", "100")][2], "out_of_range")

        bad = run(str(ROOT / "tests/data/three_decimals.txt"), "--directed",
                  "yes", "cycle")
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn("three_decimals.txt:2:", bad.stderr)

    def test_undirected_has_no_reverse_row(self):
        # weighted_small: 1 - 2 (0.5), 2 - 3 (1.5), read as undirected
        out = run(str(ROOT / "tests/data/weighted_small.txt"), "--directed",
                  "no", "bf", "--target", "1", "--sources", "3", "--runs", "1")
        times = [r[1] for r in rows(out.stdout) if r[0] == "time"]
        self.assertEqual(times, ["bellman_ford_pass"])
        self.assertEqual(paths(out.stdout)[("3", "1")][:2], ("2.00", "3 2 1"))

    def test_undirected_negative_edge_is_a_cycle(self):
        # part3_small read as undirected: 20 - 40 (-0.25) is the cycle
        # 20 - 40 - 20 (-0.50), and 50 reaches it
        out = run(str(SMALL), "--directed", "no", "bf", "--target", "100",
                  "--sources", "50", "--runs", "1")
        self.assertEqual(paths(out.stdout)[("50", "100")][2], "negative_cycle")


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cos242-part3-")
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)

    def write(self, name: str, text: str) -> Path:
        path = self.dir / name
        path.write_text(textwrap.dedent(text).lstrip())
        return path

    def test_parse_row(self):
        self.assertEqual(harness.parse_row("time,fw,1,12.5"),
                         ["time", "fw", "1", "12.5"])
        self.assertEqual(len(harness.parse_row("time,allpairs_pass,1,3,42")), 5)
        for line in ("info,a,b", "calib,floyd_warshall,500,10,1,3",
                     "fit,floyd_warshall_n3,1e-10,2", "progress,fw,1,2,3",
                     "path,bf,1,2,3.00,1,1 2,ok", "cycle,no,0,,,ok",
                     "check,witness,ok,x", "done,bf"):
            harness.parse_row(line)
        with self.assertRaises(ValueError):
            harness.parse_row("bogus,1,2")
        with self.assertRaises(ValueError):
            harness.parse_row("time,fw,1")

    def test_summarize(self):
        self.assertEqual(harness.summarize([1e6] * 10, 10)["status"], "ok")
        s = harness.summarize([1e6, 2e6, 3e6, 4e6, 5e6, 6e6], 10)
        self.assertEqual((s["status"], s["samples"]), ("fewer_runs", 6))
        self.assertEqual(s["mean"], 3.5)
        self.assertEqual(s["median"], 3.5)

    def test_read_config_rejects_blank_directed(self):
        config = self.write("c.csv", """
            graph,directed,bf_target
            g,,100
            """)
        with self.assertRaises(SystemExit):
            harness.read_config(config)

    def test_decide_fw(self):
        sixteen = 16 * harness.GIB - harness.GIB
        self.assertEqual(harness.decide_fw((100_000 + 1) ** 2 * 12, sixteen),
                         "no_memory")
        self.assertEqual(harness.decide_fw((25_000 + 1) ** 2 * 12, sixteen),
                         "run")

    def test_restart_keeps_complete_and_renames_incomplete(self):
        complete = self.write("bf.csv", "info,vertices,3\ndone,bf\n")
        incomplete = self.write("fw.csv", "info,vertices,3\ntime,fw,1,5\n")
        self.assertFalse(harness.prepare(complete))
        self.assertTrue(harness.prepare(incomplete))
        self.assertFalse(incomplete.exists())
        self.assertTrue((self.dir / "fw.csv.incomplete").exists())

    def test_watchdog_kills_a_silent_launch(self):
        fake = self.write("fake.py", """
            import sys, time
            print("info,vertices,3", flush=True)
            time.sleep(30)
            print("done,bf", flush=True)
            """)
        out = self.dir / "run.csv"
        started = __import__("time").monotonic()
        complete = harness.launch([sys.executable, str(fake)], out, 1.0)
        self.assertFalse(complete)
        self.assertLess(__import__("time").monotonic() - started, 15)
        self.assertEqual(out.read_text(), "info,vertices,3\n")

    def merge_fixture(self, edit_check: bool = False) -> list[dict]:
        cfg = {"graph": "g", "runs": "10", "fw_set": "1|2"}
        run_dir = self.dir / "g"
        run_dir.mkdir()
        (run_dir / "bf.csv").write_text(
            "info,vertices,2\ninfo,max_abs_weight,1.00\n"
            + "".join(f"time,reverse,{i},1000000\ntime,bellman_ford_pass,{i},1000000\n"
                      for i in range(1, 11))
            + "path,bellman_ford,1,2,1.00,1,1 2,ok\ndone,bf\n")
        (run_dir / "fw.csv").write_text(
            "info,vertices,2\ntime,floyd_warshall,1,5000000\n"
            "path,floyd_warshall,1,2,1.00,1,1 2,ok\n"
            "path,floyd_warshall,2,1,inf,,,unreachable\ndone,fw\n")
        distance = "9.00" if edit_check else "1.00"
        (run_dir / "bf_check_2.csv").write_text(
            "info,vertices,2\ntime,bellman_ford_pass,1,99000000\n"
            f"path,bellman_ford,1,2,{distance},1,1 2,ok\n"
            "path,bellman_ford,2,2,0.00,0,2,ok\ndone,bf\n")
        (run_dir / "bf_check_1.csv").write_text(
            "info,vertices,2\ntime,bellman_ford_pass,1,99000000\n"
            "path,bellman_ford,1,1,0.00,0,1,ok\n"
            "path,bellman_ford,2,1,inf,,,unreachable\ndone,bf\n")
        (run_dir / "cycle.csv").write_text(
            "info,vertices,2\ntime,negative_cycle_check,1,5\n")  # no done row
        results, _ = harness.merge_graph(cfg, run_dir, "run", 100)
        return results

    def test_merge(self):
        results = self.merge_fixture()
        by = {(r["algorithm"], r["metric"]): r for r in results}
        # bf time = reverse + pass, 10 runs: ok; bf_check rows not mixed in
        self.assertEqual(by[("bellman_ford", "mean_s")]["value"], 2.0)
        self.assertEqual(by[("bellman_ford", "mean_s")]["samples"], 10)
        self.assertEqual(by[("bellman_ford", "mean_s")]["status"], "ok")
        # a single Floyd-Warshall run: fewer_runs, 1 sample
        self.assertEqual(by[("floyd_warshall", "mean_s")]["status"], "fewer_runs")
        self.assertEqual(by[("floyd_warshall", "fw_vs_bf")]["status"], "ok")
        # the incomplete cycle file is ignored; matrix rows are skipped
        self.assertNotIn(("negative_cycle_check", "mean_s"), by)
        self.assertEqual(by[("matrix", "representation")]["status"], "skipped")

    def test_fw_estimate_prefers_measured_runs(self):
        # graph a ran Floyd-Warshall: n = 10 in 2 s -> c = 2e-3 s per n^3;
        # graph b did not (n = 20) and its calibration fit says c = 1
        for name, n in (("a", 10), ("b", 20)):
            (self.dir / name).mkdir()
            (self.dir / name / "fw_decision.txt").write_text(
                ("run" if name == "a" else "no_memory") + " 100\n")
            (self.dir / name / "calibrate.csv").write_text(
                f"info,vertices,{n}\nfit,floyd_warshall_n3,1,3\ndone,calibrate\n")
        (self.dir / "a" / "fw.csv").write_text(
            "info,vertices,10\ntime,floyd_warshall,1,2000000\ndone,fw\n")
        rows = [{"graph": g, "runs": "10", "fw_set": "1"} for g in ("a", "b")]
        harness.merge_all(rows, self.dir, self.dir / "out")
        import csv as csv_module
        with (self.dir / "out" / "results_part3.csv").open() as f:
            estimate = [r for r in csv_module.DictReader(f)
                        if r["graph"] == "b" and r["metric"] == "estimate_s"][0]
        self.assertAlmostEqual(float(estimate["value"]), 2e-3 * 20 ** 3)
        self.assertIn("measured on a", estimate["note"])
        self.assertIn("calibration gave c = 1", estimate["note"])

    def test_merge_mismatch(self):
        by = {(r["algorithm"], r["metric"]): r
              for r in self.merge_fixture(edit_check=True)}
        self.assertEqual(by[("floyd_warshall", "fw_vs_bf")]["status"], "mismatch")


if __name__ == "__main__":
    unittest.main()
