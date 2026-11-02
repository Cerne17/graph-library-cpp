"""Prepare/run the 12-graph BFS/DFS experiment on a dedicated Azure VM."""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tarfile

ROOT=Path(__file__).resolve().parent.parent
GROUP='cos242-traversals'
VM='cos242-runner'
REGION='belgiumcentral'
SIZE='Standard_E4as_v4'
REMOTE='cos242-traversals'


def run(*args, capture=False):
    result=subprocess.run(args,check=True,text=True,stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else None


def ip():
    return run('az','vm','show','-d','-g',GROUP,'-n',VM,'--query','publicIps','-o','tsv',capture=True)


def ssh(address, command):
    return run('ssh','-o','BatchMode=yes','-o','StrictHostKeyChecking=accept-new','-o','ConnectTimeout=15',f'azureuser@{address}',command)


def provision():
    # Only run after approval of this exact billable configuration.
    key=Path.home()/'.ssh/id_rsa.pub'
    if not key.is_file():
        key=Path.home()/'.ssh/id_ed25519.pub'
    if not key.is_file():
        raise RuntimeError('An existing SSH public key is required; no credentials are generated.')
    group_exists=run('az','group','exists','-n',GROUP,capture=True)=='true'
    existing=json.loads(run('az','vm','list','-g',GROUP,'-o','json',capture=True)) if group_exists else []
    if existing:
        raise RuntimeError('Dedicated resource group already contains a VM; inspect before provisioning.')
    run('az','group','create','-n',GROUP,'-l',REGION,'-o','none')
    deadline=dt.datetime.now(dt.timezone.utc)+dt.timedelta(hours=6)
    try:
        run('az','vm','create','-g',GROUP,'-n',VM,'-l',REGION,'--size',SIZE,
            '--image','Canonical:ubuntu-24_04-lts:server:latest','--admin-username','azureuser',
            '--ssh-key-values',str(key),'--public-ip-sku','Standard','--os-disk-size-gb','64','-o','none')
        from setup_azure_guard import setup
        setup(deadline=deadline)
    except BaseException:
        # Fail closed if deployment or the shutdown safeguard fails.
        subprocess.run(['az','vm','deallocate','-g',GROUP,'-n',VM,'-o','none'],check=False)
        raise
    print(f'VM ready; automatic deallocation at {deadline.isoformat()} (within six hours).')


def upload(address):
    # Upload current work, not an older remote branch; no reset/overwrite of a prior experiment.
    forecast_path=ROOT/'docs/traversal_forecast.json'
    forecast=json.loads(forecast_path.read_text())
    if forecast['config_sha256'] != hashlib.sha256((ROOT/'config_traversals.csv').read_bytes()).hexdigest():
        raise RuntimeError('Timing configuration changed after the forecast; regenerate the estimate before uploading.')
    files=[]
    for folder in ('include','app','analysis','tests','third_party','src'):
        files.extend(p for p in (ROOT/folder).rglob('*') if p.is_file() and not p.is_symlink() and not any(part.startswith('.') or part in {'__pycache__','venv','node_modules','__MACOSX'} for part in p.relative_to(ROOT).parts))
    files += [ROOT/'Makefile',ROOT/'config_collect.csv',ROOT/'config_traversals.csv']
    files=sorted(set(files))
    # Exact source hashes survive the absence of .git in the uploaded snapshot.
    manifest={'commit':run('git','rev-parse','HEAD',capture=True),
              'forecast_sha256':hashlib.sha256(forecast_path.read_bytes()).hexdigest(),
              'files':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
    directory=ROOT/'build/azure_traversals';directory.mkdir(parents=True,exist_ok=True)
    snapshot=directory/'source_manifest.json';snapshot.write_text(json.dumps(manifest,indent=2)+'\n')
    ssh(address,f"""set -eu
mkdir -p {REMOTE}/data
test ! -e {REMOTE}/traversal.started
if ! command -v rsync >/dev/null; then
  sudo cloud-init status --wait >/dev/null
  sudo apt-get update -qq
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq rsync >/dev/null
fi
""")
    # The macOS openrsync does not reliably honor /./ path trimming with -R.
    # A relative-path tar archive also excludes local virtual environments.
    archive=directory/'sources.tar.gz'
    with tarfile.open(archive,'w:gz') as dst:
        for path in files:
            dst.add(path,arcname=str(path.relative_to(ROOT)),recursive=False)
        dst.add(snapshot,arcname='source_manifest.json',recursive=False)
    run('rsync','-a',str(archive),f'azureuser@{address}:{REMOTE}/sources.tar.gz')
    ssh(address,f'cd {REMOTE} && tar -xzf sources.tar.gz')
    import csv
    with (ROOT/'config_collect.csv').open() as src:
        configs=list(csv.DictReader(src))
    inputs=[]
    for cfg in configs:
        for stem in (cfg['graph'],cfg.get('labels')):
            if not stem:continue
            candidates=[ROOT/'data'/(stem+s) for s in ('.txt','.txt.gz')]
            path=next((p for p in candidates if p.is_file()),None)
            if path is None:raise FileNotFoundError(stem)
            inputs.append(str(path))
    run('rsync','-a','--partial','--progress','-e','ssh -o BatchMode=yes',*inputs,f'azureuser@{address}:{REMOTE}/data/')
    ssh(address,f'''set -eu
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential tmux rsync >/dev/null
cd {REMOTE}
make collector
make test
{{ g++ --version | head -1; lscpu; free -b; cat /etc/os-release; }} > environment_traversals.txt
''')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['plan','provision','upload','start','status','fetch','deallocate'])
    args=parser.parse_args()
    if args.action=='plan':
        import csv
        with (ROOT/'config_traversals.csv').open() as src:
            configs=list(csv.DictReader(src))
        count=2*sum(int(cfg.get('k_'+rep) or 100) for cfg in configs for rep in cfg['representations'].split('|'))
        print(f'VM: {SIZE}, 4 vCPUs, 32 GiB RAM, Ubuntu 24.04, 64-GiB disk, public SSH, existing key.\nRegion: {REGION}; group: {GROUP}. Auto-deallocation within six hours.\nAdaptive plan: {count} traversals; see docs/TRAVERSAL_ESTIMATE.md.\nNo repeated Dijkstra jobs. Raw samples and separate Azure-style summaries retained.')
    elif args.action=='provision':provision()
    elif args.action=='deallocate':run('az','vm','deallocate','-g',GROUP,'-n',VM,'-o','none')
    else:
        address=ip()
        if args.action=='upload':upload(address)
        elif args.action=='start':
            ssh(address,f"cd {REMOTE} && test ! -e traversal.started && touch traversal.started && tmux new-session -d -s traversals 'bash analysis/traversals_job.sh'")
        elif args.action=='status':ssh(address,f'cd {REMOTE}; tail -15 traversal.log; test ! -f traversal.exit || cat traversal.exit')
        elif args.action=='fetch':
            out=ROOT/'data/azure_traversals';out.mkdir(parents=True,exist_ok=True)
            for relative in ('data/azure_traversals/','data/collected_traversals','source_manifest.json','environment_traversals.txt','traversal.log','traversal.exit'):
                run('rsync','-a','--partial','-e','ssh -o BatchMode=yes',f'azureuser@{address}:{REMOTE}/{relative}',str(out)+'/')
            print('After validating local copies: run deallocate to stop VM compute billing. Disk/IP charges remain until resources are deleted.')


if __name__=='__main__':main()
