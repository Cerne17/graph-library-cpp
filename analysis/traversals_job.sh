#!/usr/bin/env bash
# EXIT records failures as well as successful completion.
set -euo pipefail
trap 'echo "$?" > traversal.exit' EXIT
bash analysis/run_traversals.sh > traversal.log 2>&1
