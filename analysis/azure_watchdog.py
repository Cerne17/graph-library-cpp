"""Live status of the Part 2 run on the Azure VM.

Every refresh it pulls the partial results into data/azure/ (the harness
appends one whole graph at a time, so the files are always consistent), prints
a summary and checks for each graph as it finishes, and keeps an animated
progress bar. Progress is estimated: finished graphs count fully, the current
one by elapsed time against its estimate (capped at 95% until it finishes).

Needs alive-progress (see docs/AZURE.md). From the repository root:
    python analysis/azure_watchdog.py [refresh_seconds]
Ctrl-C quits; the run on the VM is unaffected.
"""
import csv
import io
import os
import subprocess
import sys
import time
from pathlib import Path

from alive_progress import alive_bar

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "data" / "azure"
GROUP = os.environ.get("AZ_GROUP", "cos242-part2")
REFRESH = int(sys.argv[1]) if len(sys.argv) > 1 else 30
# Minutes per graph, measured on Standard_E4as_v4 (run of 2026-10-30). Only
# drives the bar and the ETA; adjust for another VM size or config.
EST_MIN = {"grafo_W_3": 48, "grafo_W_4": 10, "grafo_W_5": 55,
           "grafo_W_6": 80, "grafo_W_7": 255, "rede_colaboracao": 5}
TOTAL = sum(EST_MIN.values())
ARRAY_GRAPHS = {"grafo_W_3"}

REMOTE = r'''
cd library
echo "done=$([ -e run.done ] && echo 1 || echo 0)"
echo "now=$(date +%s)"
echo "mtime=$(stat -c %Y run.log 2>/dev/null || echo 0)"
echo "start=$(stat -c %W run.log 2>/dev/null || echo 0)"
echo "mem=$(free -g | awk 'NR==2 {print $3"/"$2" GB"}')"
pid=$(pgrep -f '[b]uild/benchmark_weighted' | head -1)
echo "proc=$([ -n "$pid" ] && ps -o etime=,pcpu=,rss= -p "$pid" |
  awk '{printf "%s elapsed, %s%% CPU, %d MB", $1, $2, $3/1024}')"
echo "---log"
cat run.log 2>/dev/null
'''


def sh(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(args, capture_output=True, text=True)


def remote_state(ip: str) -> dict | None:
    r = sh("ssh", "-o", "ConnectTimeout=10", "-o", "BatchMode=yes",
           f"azureuser@{ip}", REMOTE)
    if r.returncode != 0:
        return None
    head, _, log = r.stdout.partition("---log\n")
    state = dict(line.split("=", 1) for line in head.splitlines() if "=" in line)
    state["log"] = log
    return state


def pull(ip: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name in ("data/results_weighted.csv", "data/paths_weighted.csv",
                 "run.log", "environment.txt"):
        sh("rsync", "-q", "-e", "ssh -o BatchMode=yes",
           f"azureuser@{ip}:library/{name}", str(OUT) + "/")


def read_csv(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return list(csv.DictReader(io.StringIO(path.read_text())))


def fmt_micros(v: str) -> str:
    if not v:
        return "-"
    us = float(v)
    return f"{us / 1e6:.2f} s" if us >= 1e6 else f"{us / 1e3:.1f} ms"


def report(graph: str, results: list[dict], paths: list[dict]) -> list[str]:
    """Summary lines and checks for one finished graph."""
    rows = [r for r in results if r["graph"] == graph]
    get = {(r["representation"], r["metric"]): r for r in rows}
    lines, problems = [f"== {graph} finished"], []

    for rep in ("csr", "matrix"):
        mem = get.get((rep, "memory_mb_graph"))
        if (rep, "memory_mb_estimate") in get and mem is None:
            est = float(get[(rep, "memory_mb_estimate")]["value_median"])
            status = get[(rep, "memory_mb_estimate")]["status"]
            lines.append(f"   {rep:6}  {status} (needs ~{est:,.0f} MB)")
            if status not in ("ok", "no_memory"):
                problems.append(f"{rep}: status {status}")
            continue
        if mem is None:
            continue
        times = "  ".join(
            f"{v.removeprefix('dijkstra_')} "
            f"{fmt_micros(get.get((rep, v + '_micros'), {}).get('value_median', ''))}"
            for v in ("dijkstra_heap", "dijkstra_lazy", "dijkstra_array"))
        lines.append(f"   {rep:6}  {float(mem['value_median']):,.1f} MB   {times}")

    for (rep, metric), r in get.items():
        if r["status"] in ("ok", "no_memory"):
            continue
        if metric == "dijkstra_array_micros" and r["status"] == "skipped" \
                and graph not in ARRAY_GRAPHS:
            continue
        problems.append(f"{rep} {metric}: status {r['status']}")
    for v in ("memory_mb_graph", "dijkstra_heap_micros", "dijkstra_lazy_micros"):
        if get.get(("csr", v), {}).get("status") != "ok":
            problems.append(f"csr {v} missing or not ok")
    if graph in ARRAY_GRAPHS and get.get(("csr", "dijkstra_array_micros"), {}).get("status") != "ok":
        problems.append("csr dijkstra_array not ok")

    gp = [p for p in paths if p["graph"] == graph]
    for rep in sorted({p["representation"] for p in gp}):
        mine = [p for p in gp if p["representation"] == rep]
        statuses = ", ".join(sorted({p["status"] for p in mine}))
        lines.append(f"   paths {rep}: {len(mine)} targets ({statuses})")
        if len(mine) != 5:
            problems.append(f"paths {rep}: {len(mine)} targets, expected 5")
        for p in mine:
            label = p["target_name"] or p["target"]
            dist = f"{float(p['distance']):.4g}" if p["distance"] else "-"
            lines.append(f"      -> {label}: {p['status']}, dist {dist}, {p['hops'] or '-'} hops")
    if not gp:
        problems.append("no path rows")

    lines.append("   checks: OK" if not problems else
                 "   checks: PROBLEMS\n" + "\n".join(f"     ! {p}" for p in problems))
    return lines


def progress(state: dict) -> tuple[float, str, list[str]]:
    log = state["log"]
    finished = [l.split()[1] for l in log.splitlines() if l.startswith("processing ")]
    wrote = sum(1 for l in log.splitlines() if l.startswith("  wrote "))
    done_graphs, current = finished[:wrote], (finished[wrote] if len(finished) > wrote else None)
    now, mtime = int(state["now"]), int(state["mtime"])
    minutes = sum(EST_MIN.get(g, 0) for g in done_graphs)
    text = ""
    if current and state["done"] == "0":
        frac = min(0.95, (now - mtime) / 60 / EST_MIN.get(current, 30))
        minutes += EST_MIN.get(current, 30) * frac
        left = TOTAL - minutes
        text = (f"{current} ({len(done_graphs) + 1}/{len(EST_MIN)}) | "
                f"{state.get('proc') or 'loading'} | mem {state['mem']} | "
                f"ETA ~{int(left // 60)}h{int(left % 60):02d}m")
    return (1.0 if state["done"] == "1" else minutes / TOTAL), text, done_graphs


def main() -> None:
    print("looking up VM IP ...")
    ip = sh("az", "vm", "show", "-d", "-g", GROUP, "-n", "cos242-runner",
            "--query", "publicIps", "-o", "tsv").stdout.strip()
    if not ip:
        sys.exit(f"no VM in {GROUP}")
    print(f"VM {ip}; partial results go to {OUT}\n")

    reported: set[str] = set()
    with alive_bar(manual=True, title="Part 2", bar="smooth",
                   spinner="dots_waves", dual_line=True, stats=False,
                   elapsed="{elapsed}", monitor="{percent:.0%}") as bar:
        while True:
            state = remote_state(ip)
            if state is None:
                bar.text("ssh failed, retrying ...")
                time.sleep(REFRESH)
                continue
            pull(ip)
            frac, text, done_graphs = progress(state)
            results = read_csv(OUT / "results_weighted.csv")
            paths = read_csv(OUT / "paths_weighted.csv")
            for g in done_graphs:
                if g not in reported:
                    print("\n".join(report(g, results, paths)) + "\n")
                    reported.add(g)
            if "FAILED" in state["log"] or "Traceback" in state["log"]:
                print("!! FAILED or Traceback in run.log:")
                print("\n".join(state["log"].splitlines()[-10:]))
            bar(frac)
            bar.text(text or "waiting ...")
            if state["done"] == "1":
                bar.text("finished")
                break
            time.sleep(REFRESH)
    print(f"\n== finished. Results in {OUT}. Billing continues until "
          "analysis/azure_part2.sh delete.\a")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nwatchdog stopped (the run on the VM continues)")
