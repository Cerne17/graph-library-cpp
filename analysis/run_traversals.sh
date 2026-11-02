#!/usr/bin/env bash
# Run inside the uploaded Azure snapshot. Raw sources/times remain in checkpoints.
set -euo pipefail
python3 analysis/collect.py --phases timing --algorithms bfs,dfs \
  --config config_traversals.csv --k 100 --seed 2025 --memory-limit-gb 28 \
  --timeout 14400 --out data/collected_traversals --tmp /tmp
python3 analysis/summarize_traversals.py
