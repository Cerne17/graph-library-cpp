"""Recover the authorized VM with a resource-scoped managed-identity shutdown timer."""
from __future__ import annotations
import datetime as dt
import json
from pathlib import Path
import shlex
import subprocess
import time
from azure_traversals import GROUP, VM, ROOT, ip, run


def setup(deadline=None):
    if deadline is None:
        created=run('az','vm','show','-g',GROUP,'-n',VM,'--query','timeCreated','-o','tsv',capture=True)
        deadline=dt.datetime.fromisoformat(created.replace('Z','+00:00'))+dt.timedelta(hours=6)
    if deadline <= dt.datetime.now(dt.timezone.utc):raise RuntimeError('authorized shutdown deadline has passed')
    # The only IAM grant is on this experiment's VM, never on the subscription/group.
    vm_id=run('az','vm','show','-g',GROUP,'-n',VM,'--query','id','-o','tsv',capture=True)
    identity=json.loads(run('az','vm','identity','assign','-g',GROUP,'-n',VM,'-o','json',capture=True))
    principal=identity['systemAssignedIdentity'] if 'systemAssignedIdentity' in identity else identity['principalId']
    if isinstance(principal,dict):principal=principal['principalId']
    role='40c5ff49-9181-41f8-ae61-143b0e78555e'
    assignment=json.loads(run('az','role','assignment','create','--assignee-object-id',principal,
                             '--assignee-principal-type','ServicePrincipal','--role',role,'--scope',vm_id,'-o','json',capture=True))
    directory=ROOT/'build/azure_traversals';directory.mkdir(parents=True,exist_ok=True)
    record={'vm_id':vm_id,'role_assignment_id':assignment['id'],'deadline_utc':deadline.isoformat()}
    (directory/'guard_config.json').write_text(json.dumps(record,indent=2)+'\n')
    try:
        run('az','vm','start','-g',GROUP,'-n',VM,'-o','none')
        address=ip()
        connection=['ssh','-o','BatchMode=yes','-o','ConnectTimeout=10','-o','StrictHostKeyChecking=accept-new',f'azureuser@{address}']
        for attempt in range(30):
            if subprocess.run(connection+['true'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0:break
            time.sleep(5)
        else:raise RuntimeError('SSH did not become ready')
        run('scp','-q',str(ROOT/'analysis/azure_self_deallocate.py'),str(directory/'guard_config.json'),f'azureuser@{address}:/tmp/')
        calendar=deadline.strftime('%Y-%m-%d %H:%M:%S UTC')
        script=f'''set -eu
sudo install -m 700 /tmp/azure_self_deallocate.py /usr/local/sbin/cos242-self-deallocate.py
sudo install -m 600 /tmp/guard_config.json /etc/cos242-cost-guard.json
sudo tee /etc/systemd/system/cos242-cost-guard.service >/dev/null <<'EOF'
[Unit]
Description=Deallocate the temporary COS242 VM
After=network-online.target
Wants=network-online.target
[Service]
Type=oneshot
ExecStart=/usr/bin/python3 /usr/local/sbin/cos242-self-deallocate.py
Restart=on-failure
RestartSec=60
EOF
sudo tee /etc/systemd/system/cos242-cost-guard.timer >/dev/null <<'EOF'
[Unit]
Description=Cost deadline for the temporary COS242 VM
[Timer]
OnCalendar={calendar}
Persistent=true
Unit=cos242-cost-guard.service
[Install]
WantedBy=timers.target
EOF
sudo systemctl daemon-reload
sudo systemctl enable --now cos242-cost-guard.timer
sudo python3 /usr/local/sbin/cos242-self-deallocate.py --check
sudo systemctl list-timers cos242-cost-guard.timer --no-pager
'''
        # Allow newly created resource-scoped RBAC assignment to propagate.
        for attempt in range(12):
            result=subprocess.run(connection+['bash','-s'],input=script,text=True)
            if result.returncode==0:break
            time.sleep(10)
        else:raise RuntimeError('guard permission verification failed')
        print('Resource-scoped self-deallocation guard verified; deadline',deadline.isoformat())
    except BaseException:
        subprocess.run(['az','vm','deallocate','-g',GROUP,'-n',VM,'-o','none'],check=False)
        raise


if __name__=='__main__':setup()
