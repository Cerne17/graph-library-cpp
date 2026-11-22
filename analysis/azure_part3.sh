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
#   analysis/azure_part3.sh smoke       # smoke config on the VM, compared with the local run
#   analysis/azure_part3.sh calibrate   # VM-time estimate of the full run (owner approves it)
#   analysis/azure_part3.sh start [G]   # harness in tmux (all graphs, or only G)
#   analysis/azure_part3.sh start-parallel "G1" "G2 G3" ...
#                                       # one harness process per group, at once
#   analysis/azure_part3.sh status      # last log lines (ETA of Floyd-Warshall), finished?
#   analysis/azure_part3.sh fetch       # results into data/azure_part3/
#   analysis/azure_part3.sh guard [H]   # the VM deallocates itself in H hours (default 20)
#   analysis/azure_part3.sh delete      # deletes everything; billing stops
#
# Settings (environment variables):
#   AZ_LOCATION  region your subscription allows (default: belgiumcentral; the
#                student subscription allows only a few regions)
#   AZ_GROUP     resource group, deleted as a whole at the end (default: cos242-part3)
#   AZ_SIZES     VM sizes to try, first that succeeds wins. Default: 32 GB,
#                4 vCPUs: two Floyd-Warshall runs (7.5 GB each) at once, plus
#                the large graphs in a third process.
#   AZ_HOURS     hours to re-arm the auto-shutdown for in start-parallel
#                (default 8; 3 h of margin are added)
#   BRANCH       branch the VM builds (default: feature/part3)
#
# The auto-shutdown is a daily schedule: `start` re-arms it to now + the
# calibrated estimate + 3 h and refuses when that is more than 22 h ahead; then
# run the graphs one at a time with `start <graph>`, re-arming before each.
# Billing runs while the VM exists, even when idle: `delete` after `fetch`.

set -euo pipefail

LOCATION="${AZ_LOCATION:-belgiumcentral}"
GROUP="${AZ_GROUP:-cos242-part3}"
VM="cos242-runner"
USER_NAME="azureuser"
BRANCH="${BRANCH:-feature/part3}"
REPO_URL="https://github.com/Cerne17/graph-library-cpp.git"
SIZES="${AZ_SIZES:-Standard_E4as_v4 Standard_E4_v4}"
REMOTE_DIR="library"
INPUTS="data/part3/grafo_W_*.txt.gz"
MAX_AHEAD_HOURS=22

die() { echo "error: $*" >&2; exit 1; }

# az vm show, retried: right after a resource group is recreated, Azure can
# answer ResourceNotFound for a while on some requests.
vm_show() {
  local tries out
  for tries in 1 2 3 4 5 6; do
    if out="$(az vm show -g "$GROUP" -n "$VM" "$@" 2>/dev/null)"; then
      echo "$out"
      return 0
    fi
    sleep 5
  done
  return 1
}

IP_CACHE=""
ip() {
  if [ -z "$IP_CACHE" ]; then
    IP_CACHE="$(vm_show -d --query publicIps -o tsv)" ||
      die "no VM $VM in $GROUP: run create first"
  fi
  echo "$IP_CACHE"
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
  echo "created $created at $(ip)"
  # safety net until start re-arms it
  cmd_guard 20
}

# The cost guard, as in Part 2 (azure_self_deallocate.py): the VM gets a
# managed identity allowed to deallocate only itself (Virtual Machine
# Contributor on the VM's own resource ID), and a systemd timer on the VM
# deallocates it at the deadline. Azure's own auto-shutdown schedule is not
# offered in every region (not in belgiumcentral). Re-running it moves the
# deadline to now + $1 hours.
cmd_guard() {
  local hours="${1:-20}" vm_id principal deadline
  vm_id="$(vm_show --query id -o tsv)" || die "no VM $VM in $GROUP"
  principal="$(az vm identity assign -g "$GROUP" -n "$VM" --query systemAssignedIdentity -o tsv)"
  az role assignment create --assignee-object-id "$principal" \
    --assignee-principal-type ServicePrincipal \
    --role 40c5ff49-9181-41f8-ae61-143b0e78555e --scope "$vm_id" -o none 2>/dev/null ||
    true # already assigned on a re-run
  deadline="$(python3 -c "import datetime as d, sys; print((d.datetime.now(d.timezone.utc) + d.timedelta(hours=float(sys.argv[1]))).strftime('%Y-%m-%d %H:%M:%S UTC'))" "$hours")"
  scp -q -o StrictHostKeyChecking=accept-new analysis/azure_self_deallocate.py \
    "$USER_NAME@$(ip):/tmp/cos242-self-deallocate.py"
  # quoted: ssh joins its arguments into one remote command line
  remote "VM_ID='$vm_id' DEADLINE='$deadline' bash -s" <<'GUARD'
set -eu
sudo install -m 700 /tmp/cos242-self-deallocate.py /usr/local/sbin/cos242-self-deallocate.py
printf '{"vm_id": "%s"}\n' "$VM_ID" | sudo tee /etc/cos242-cost-guard.json >/dev/null
sudo chmod 600 /etc/cos242-cost-guard.json
sudo tee /etc/systemd/system/cos242-cost-guard.service >/dev/null <<'UNIT'
[Unit]
Description=Deallocate the temporary COS242 VM
After=network-online.target
Wants=network-online.target
[Service]
Type=oneshot
ExecStart=/usr/bin/python3 /usr/local/sbin/cos242-self-deallocate.py
Restart=on-failure
RestartSec=60
UNIT
sudo tee /etc/systemd/system/cos242-cost-guard.timer >/dev/null <<UNIT
[Unit]
Description=Cost deadline for the temporary COS242 VM
[Timer]
OnCalendar=$DEADLINE
Persistent=true
Unit=cos242-cost-guard.service
[Install]
WantedBy=timers.target
UNIT
sudo systemctl daemon-reload
sudo systemctl enable cos242-cost-guard.timer >/dev/null 2>&1
sudo systemctl restart cos242-cost-guard.timer
GUARD
  # the role takes a moment to propagate; the check retries for ~2 minutes
  local tries=0
  until remote "sudo python3 /usr/local/sbin/cos242-self-deallocate.py --check" 2>/dev/null; do
    tries=$((tries + 1))
    [ "$tries" -lt 12 ] || die "cost guard: deallocation permission not effective yet; rerun guard"
    sleep 10
  done
  echo "cost guard: the VM deallocates itself at $deadline"
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
# The estimate is $1 when given, else vm_estimate.txt from calibrate.
rearm_shutdown() {
  local hours at
  if [ -n "${1:-}" ]; then
    hours="$1"
  else
    hours="$(remote "cat $REMOTE_DIR/data/part3_runs/vm_estimate.txt" 2>/dev/null)" ||
      die "no vm_estimate.txt: run calibrate first"
  fi
  at="$(python3 - "$hours" "$MAX_AHEAD_HOURS" <<'PY'
import datetime as d, sys
ahead = float(sys.argv[1]) + 3
if ahead > float(sys.argv[2]):
    sys.exit(f"estimate + 3 h = {ahead:.1f} h is more than {sys.argv[2]} h ahead: "
             "run one graph at a time with `start <graph>`")
print((d.datetime.now(d.timezone.utc) + d.timedelta(hours=ahead)).strftime("%H%M"))
PY
)" || exit 1
  cmd_guard "$(python3 -c "import sys; print(float(sys.argv[1]) + 3)" "$hours")"
  echo "(estimate ${hours} h + 3 h)"
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

cmd_start_parallel() {
  [ "$#" -gt 0 ] || die "start-parallel needs at least one group of graphs"
  rearm_shutdown "${AZ_HOURS:-8}"
  local jobs="" i=0 group only
  for group in "$@"; do
    i=$((i + 1))
    only=""
    for g in $group; do only="$only --only $g"; done
    jobs="$jobs python3 analysis/harness_part3.py$only > run_$i.log 2>&1 &"
  done
  remote "cd $REMOTE_DIR && rm -f run.done && tmux new -d -s part3 \
    '$jobs wait; python3 analysis/harness_part3.py --merge-only >> run.log 2>&1; touch run.done'"
  echo "started $i harness processes; follow with: analysis/azure_part3.sh status"
}

cmd_smoke() {
  remote "cd $REMOTE_DIR && rm -rf data/part3_runs_smoke data/azure_smoke && \
    python3 analysis/harness_part3.py --config config_part3_smoke.csv \
      --data tests/data/part3_smoke --runs-dir data/part3_runs_smoke \
      --out data/azure_smoke 2>&1 | tail -2"
  mkdir -p data/azure_part3_smoke
  rsync -a -e "ssh -o StrictHostKeyChecking=accept-new" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/azure_smoke/" data/azure_part3_smoke/
  [ -f data/local_part3_smoke/results_part3.csv ] ||
    python3 analysis/harness_part3.py --config config_part3_smoke.csv \
      --data tests/data/part3_smoke --runs-dir data/part3_runs_smoke \
      --out data/local_part3_smoke
  python3 analysis/compare_part3.py data/local_part3_smoke data/azure_part3_smoke
}

cmd_status() {
  remote "cd $REMOTE_DIR && for f in run.log run_*.log; do [ -f \$f ] && \
    { echo \"== \$f\"; tail -n 6 \$f; }; done; \
    for f in data/part3_runs/*/fw.csv; do [ -f \$f ] && \
      { echo \"== \$f\"; grep '^progress' \$f | tail -n 1; }; done; \
    if [ -e run.done ]; then echo '== finished'; else echo '== running'; fi; \
    free -g | sed -n 2p"
}

cmd_fetch() {
  mkdir -p data/azure_part3
  rsync -avP -e "ssh -o StrictHostKeyChecking=accept-new" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/results_part3.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/paths_part3.csv" \
    "$USER_NAME@$(ip):$REMOTE_DIR/run*.log" \
    "$USER_NAME@$(ip):$REMOTE_DIR/environment.txt" \
    "$USER_NAME@$(ip):$REMOTE_DIR/data/part3_runs" data/azure_part3/
}

cmd_delete() {
  az group delete -n "$GROUP" --yes
  echo "deleted $GROUP; nothing left billing"
}

case "${1:-}" in
  check | create | setup | upload | smoke | calibrate | status | fetch | delete) "cmd_$1" ;;
  guard) cmd_guard "${2:-20}" ;;
  start) cmd_start "${2:-}" ;;
  start-parallel) shift; cmd_start_parallel "$@" ;;
  *) sed -n '2,37p' "$0"; exit 1 ;;
esac
