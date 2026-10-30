#!/usr/bin/env bash
# Runs the Part 2 case studies on an Azure VM, from your Mac.
#
# Needs: Azure CLI (`brew install azure-cli`, then `az login`), ssh, rsync.
# Run from the repository root, one step at a time:
#
#   analysis/azure_part2.sh check     # quota and which VM sizes this region offers
#   analysis/azure_part2.sh create    # VM (largest size that works) + safety shutdown
#   analysis/azure_part2.sh setup     # compilers, clone, build, make test
#   analysis/azure_part2.sh upload    # data/ inputs (resumable)
#   analysis/azure_part2.sh start     # harness in tmux; safe to close the laptop
#   analysis/azure_part2.sh status    # last lines of the log, and whether it ended
#   analysis/azure_part2.sh fetch     # results into data/azure/
#   analysis/azure_part2.sh delete    # deletes everything; billing stops
#
# Settings (environment variables):
#   AZ_LOCATION  region your subscription allows (default: eastus)
#   AZ_GROUP     resource group, deleted as a whole at the end (default: cos242-part2)
#   AZ_SIZES     VM sizes to try, first that succeeds wins. Default: 128 GB
#                with 4 active vCPUs (fits grafo_W_3's 75 GB matrix), then 32 GB.
#   BRANCH       branch the VM builds (default: feature/harness-part2)
#
# Billing runs while the VM exists, even when idle: `delete` as soon as `fetch`
# is done. `create` also sets an auto-shutdown (deallocate) about 23 h ahead
# as a safety net.

set -euo pipefail

LOCATION="${AZ_LOCATION:-eastus}"
GROUP="${AZ_GROUP:-cos242-part2}"
VM="cos242-runner"
USER_NAME="azureuser"
BRANCH="${BRANCH:-feature/harness-part2}"
REPO_URL="https://github.com/Cerne17/graph-library-cpp.git"
SIZES="${AZ_SIZES:-Standard_E16-4as_v5 Standard_E16-4s_v5 Standard_E4as_v5 Standard_E4s_v5 Standard_D4as_v5}"
REMOTE_DIR="library"
INPUTS="data/baseline.txt data/grafo_W_*.txt.gz data/rede_colaboracao.txt data/rede_colaboracao_vertices.txt"

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

  # Daily auto-shutdown at "now minus one hour" (UTC): the next one is ~23 h away.
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
make -s benchmark-weighted
make -s test | tail -3
{ echo "commit: $(git rev-parse --short HEAD)"; g++ --version | head -1
  nproc; free -g | head -2; lscpu | grep -E 'Model name|L3'
  curl -s -H Metadata:true 'http://169.254.169.254/metadata/instance/compute/vmSize?api-version=2021-02-01&format=text'; echo
} > environment.txt
cat environment.txt
EOF
}

cmd_upload() {
  # shellcheck disable=SC2086  # INPUTS is a list of globs on purpose
  rsync -avP --partial -e "ssh -o StrictHostKeyChecking=accept-new" \
    $INPUTS "$USER_NAME@$(ip):$REMOTE_DIR/data/"
}

cmd_start() {
  remote "cd $REMOTE_DIR && rm -f run.done && tmux new -d -s part2 \
    'python3 analysis/harness_weighted.py 2>&1 | tee -a run.log; touch run.done'"
  echo "started; follow with: analysis/azure_part2.sh status"
}

cmd_status() {
  remote "cd $REMOTE_DIR && tail -n 15 run.log 2>/dev/null; \
    if [ -e run.done ]; then echo '== finished'; else echo '== running'; fi; \
    free -g | sed -n 2p"
}

cmd_fetch() {
  mkdir -p data/azure
  rsync -avP -e "ssh -o StrictHostKeyChecking=accept-new" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/results_weighted.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/paths_weighted.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/run.log" \
    "$USER_NAME@$(ip):$REMOTE_DIR/environment.txt" data/azure/
}

cmd_delete() {
  az group delete -n "$GROUP" --yes
  echo "deleted $GROUP; nothing left billing"
}

case "${1:-}" in
  check | create | setup | upload | start | status | fetch | delete) "cmd_$1" ;;
  *) sed -n '2,25p' "$0"; exit 1 ;;
esac
