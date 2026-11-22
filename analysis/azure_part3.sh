#!/usr/bin/env bash
# Runs the Part 3 case studies on an Azure VM, from your Mac. Adapted from
# azure_part2.sh: Part 3 branch, harness, inputs and results.
#
# Needs: Azure CLI (`brew install azure-cli`, then `az login`), ssh, rsync.
# Run from the repository root, one step at a time, after the owner approved
# `git push origin feature/part3` (setup clones the branch from GitHub):
#
#   analysis/azure_part3.sh check       # quota and which VM sizes this region offers
#   analysis/azure_part3.sh create      # VM (first size that works)
#   analysis/azure_part3.sh setup       # compilers, clone, build, make test, make test-part3
#   analysis/azure_part3.sh upload      # data/part3/ inputs (resumable)
#   analysis/azure_part3.sh calibrate   # VM-time estimate of the full run (owner approves it)
#   analysis/azure_part3.sh start [G]   # harness in tmux (all graphs, or only G)
#   analysis/azure_part3.sh status      # last log lines (ETA of Floyd-Warshall), finished?
#   analysis/azure_part3.sh fetch       # results into data/azure_part3/
#   analysis/azure_part3.sh delete      # deletes everything; billing stops
#
# Settings (environment variables):
#   AZ_LOCATION  region your subscription allows (default: eastus)
#   AZ_GROUP     resource group, deleted as a whole at the end (default: cos242-part3)
#   AZ_SIZES     VM sizes to try, first that succeeds wins. Default: 16 GB,
#                2 vCPUs (7.5 GB of Floyd-Warshall matrices plus the graph).
#   BRANCH       branch the VM builds (default: feature/part3)
#
# The auto-shutdown is a daily schedule: `start` re-arms it to now + the
# calibrated estimate + 3 h and refuses when that is more than 22 h ahead; then
# run the graphs one at a time with `start <graph>`, re-arming before each.
# Billing runs while the VM exists, even when idle: `delete` after `fetch`.

set -euo pipefail

LOCATION="${AZ_LOCATION:-eastus}"
GROUP="${AZ_GROUP:-cos242-part3}"
VM="cos242-runner"
USER_NAME="azureuser"
BRANCH="${BRANCH:-feature/part3}"
REPO_URL="https://github.com/Cerne17/graph-library-cpp.git"
SIZES="${AZ_SIZES:-Standard_E2as_v5 Standard_E2s_v5}"
REMOTE_DIR="library"
INPUTS="data/part3/grafo_W_*.txt.gz"
MAX_AHEAD_HOURS=22

die() { echo "error: $*" >&2; exit 1; }

ip() {
  az vm show -d -g "$GROUP" -n "$VM" --query publicIps -o tsv 2>/dev/null ||
    die "no VM $VM in $GROUP: run create first"
}

remote() {
  ssh -o StrictHostKeyChecking=accept-new "$USER_NAME@$(ip)" "$@"
}

cmd_check() {
  echo "== vCPU quota in $LOCATION (current / limit)"
  az vm list-usage -l "$LOCATION" \
    --query "[?contains(name.value, 'cores') || contains(name.value, 'Family')].{name:localName, used:currentValue, limit:limit}" \
    -o table | awk 'NR <= 2 || $NF > 0'
  echo
  echo "== candidate sizes in $LOCATION (empty restriction = available)"
  for size in $SIZES; do
    az vm list-skus -l "$LOCATION" --size "$size" --resource-type virtualMachines \
      --query "[?name=='$size'].{size:name, restriction:restrictions[0].reasonCode}" \
      -o tsv | sed 's/^/  /'
  done
}

cmd_create() {
  az group create -n "$GROUP" -l "$LOCATION" -o none
  local created=""
  for size in $SIZES; do
    echo "trying $size ..."
    if az vm create -g "$GROUP" -n "$VM" --size "$size" \
         --image Canonical:ubuntu-24_04-lts:server:latest \
         --admin-username "$USER_NAME" --generate-ssh-keys \
         --public-ip-sku Standard --os-disk-size-gb 64 -o none; then
      created="$size"
      break
    fi
  done
  [ -n "$created" ] || die "no size worked; try another AZ_LOCATION (see check)"
  # safety net until start re-arms it: daily, ~23 h away
  local at
  at="$(python3 -c 'import datetime as d; print((d.datetime.now(d.timezone.utc) - d.timedelta(hours=1)).strftime("%H%M"))')"
  az vm auto-shutdown -g "$GROUP" -n "$VM" --time "$at" -o none
  echo "created $created at $(ip); auto-shutdown daily at ${at} UTC"
}

cmd_setup() {
  remote BRANCH="$BRANCH" REPO_URL="$REPO_URL" DIR="$REMOTE_DIR" bash -s <<'EOF'
set -euo pipefail
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  build-essential git tmux rsync python3-pandas python3-psutil >/dev/null
if [ -d "$DIR/.git" ]; then
  git -C "$DIR" fetch -q origin "$BRANCH" && git -C "$DIR" checkout -q "$BRANCH" &&
    git -C "$DIR" reset -q --hard "origin/$BRANCH"
else
  git clone -q --branch "$BRANCH" "$REPO_URL" "$DIR"
fi
cd "$DIR"
make -s test | tail -3
make -s benchmark-part3
make -s test-part3 2>&1 | tail -3
{ echo "commit: $(git rev-parse --short HEAD)"; g++ --version | head -1
  nproc; free -g | head -2; lscpu | grep -E 'Model name|L3'
  curl -s -H Metadata:true 'http://169.254.169.254/metadata/instance/compute/vmSize?api-version=2021-02-01&format=text'; echo
} > environment.txt
cat environment.txt
EOF
}

cmd_upload() {
  remote "mkdir -p $REMOTE_DIR/data/part3"
  # shellcheck disable=SC2086  # INPUTS is a glob on purpose
  rsync -avP --partial -e "ssh -o StrictHostKeyChecking=accept-new" \
    $INPUTS "$USER_NAME@$(ip):$REMOTE_DIR/data/part3/"
}

cmd_calibrate() {
  remote "cd $REMOTE_DIR && python3 analysis/harness_part3.py --calibrate-only 2>&1 | tee -a run.log | tail -3"
}

# Re-arms the daily auto-shutdown to now + estimate + 3 h (UTC), or refuses.
rearm_shutdown() {
  local hours at
  hours="$(remote "cat $REMOTE_DIR/data/part3_runs/vm_estimate.txt" 2>/dev/null)" ||
    die "no vm_estimate.txt: run calibrate first"
  at="$(python3 - "$hours" "$MAX_AHEAD_HOURS" <<'PY'
import datetime as d, sys
ahead = float(sys.argv[1]) + 3
if ahead > float(sys.argv[2]):
    sys.exit(f"estimate + 3 h = {ahead:.1f} h is more than {sys.argv[2]} h ahead: "
             "run one graph at a time with `start <graph>`")
print((d.datetime.now(d.timezone.utc) + d.timedelta(hours=ahead)).strftime("%H%M"))
PY
)" || exit 1
  az vm auto-shutdown -g "$GROUP" -n "$VM" --time "$at" -o none
  echo "auto-shutdown re-armed to ${at} UTC (estimate ${hours} h + 3 h)"
}

cmd_start() {
  local only=""
  if [ -n "${1:-}" ]; then
    only="--only $1"
  fi
  rearm_shutdown
  remote "cd $REMOTE_DIR && rm -f run.done && tmux new -d -s part3 \
    'python3 analysis/harness_part3.py $only 2>&1 | tee -a run.log; touch run.done'"
  echo "started; follow with: analysis/azure_part3.sh status"
}

cmd_status() {
  remote "cd $REMOTE_DIR && tail -n 15 run.log 2>/dev/null; \
    echo '== last progress row:'; \
    grep -h '^progress' data/part3_runs/*/fw.csv 2>/dev/null | tail -n 1; \
    if [ -e run.done ]; then echo '== finished'; else echo '== running'; fi; \
    free -g | sed -n 2p"
}

cmd_fetch() {
  mkdir -p data/azure_part3
  rsync -avP -e "ssh -o StrictHostKeyChecking=accept-new" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/results_part3.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/paths_part3.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/run.log" \
    "$USER_NAME@$(ip):$REMOTE_DIR/environment.txt" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/part3_runs" data/azure_part3/
}

cmd_delete() {
  az group delete -n "$GROUP" --yes
  echo "deleted $GROUP; nothing left billing"
}

case "${1:-}" in
  check | create | setup | upload | calibrate | status | fetch | delete) "cmd_$1" ;;
  start) cmd_start "${2:-}" ;;
  *) sed -n '2,29p' "$0"; exit 1 ;;
esac
