"""Combines the two Part 3 Azure runs into the final dataset.

First run (data/azure_part3/, config_part3.csv, three harness processes at
once): Floyd-Warshall on W_1 and W_2, calibration, the Bellman-Ford variants,
all-pairs samples. Follow-up run (data/azure_part3_followup/,
config_part3_followup.csv, one process): 10 runs of Bellman-Ford and of the
negative-cycle check on every graph, the full all-pairs Bellman-Ford on W_2,
and the distances among the set by Bellman-Ford on W_3 to W_5.

The final run directory starts as a copy of the first run's; every launch file
of the follow-up replaces the first run's file of the same name, except that
the variant rows of the first run's bf.csv (bellman_ford_pass_<variant>,
rounds_<variant>, relaxations_<variant>) are kept in the new bf.csv, since the
follow-up did not rerun the variants. The merge then uses config_part3.csv
with runs = 10 everywhere, the number every timed step aimed for.

Usage (from the repository root):
    python3 analysis/combine_part3.py
writes data/azure_part3_final/ (part3_runs/, config_part3_final.csv,
results_part3.csv, paths_part3.csv).
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIRST = ROOT / "data" / "azure_part3" / "part3_runs"
FOLLOWUP = ROOT / "data" / "azure_part3_followup" / "part3_runs_followup"
FINAL = ROOT / "data" / "azure_part3_final"

VARIANT_PREFIXES = ("time,bellman_ford_pass_", "info,rounds_",
                    "info,relaxations_")


def combined_bf(first: Path, followup: Path) -> str:
    """The follow-up bf.csv with the first run's variant rows before done."""
    lines = followup.read_text().splitlines()
    assert lines[-1].startswith("done,"), f"{followup} is incomplete"
    variants = [line for line in first.read_text().splitlines()
                if line.startswith(VARIANT_PREFIXES)] if first.exists() else []
    return "\n".join(lines[:-1] + variants + [lines[-1]]) + "\n"


def main() -> None:
    if FINAL.exists():
        shutil.rmtree(FINAL)
    runs = FINAL / "part3_runs"
    shutil.copytree(FIRST, runs)
    for graph_dir in sorted(FOLLOWUP.iterdir()):
        target = runs / graph_dir.name
        for launch in sorted(graph_dir.glob("*.csv")):
            if launch.name == "bf.csv":
                text = combined_bf(target / "bf.csv", launch)
                (target / "bf.csv").write_text(text)
            else:
                shutil.copy2(launch, target / launch.name)

    config = (ROOT / "config_part3.csv").read_text().splitlines()
    header = config[0].split(",")
    runs_col = header.index("runs")
    rows = [config[0]]
    for line in config[1:]:
        fields = line.split(",")
        fields[runs_col] = "10"
        rows.append(",".join(fields))
    final_config = FINAL / "config_part3_final.csv"
    final_config.write_text("\n".join(rows) + "\n")

    subprocess.run([sys.executable, str(ROOT / "analysis" / "harness_part3.py"),
                    "--merge-only", "--config", str(final_config),
                    "--runs-dir", str(runs), "--out", str(FINAL)], check=True)


if __name__ == "__main__":
    main()
