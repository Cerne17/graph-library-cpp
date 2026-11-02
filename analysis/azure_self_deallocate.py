"""VM-local cost guard: use its managed identity to deallocate only itself."""
from __future__ import annotations
import argparse
import fnmatch
import json
from pathlib import Path
import urllib.parse
import urllib.request


def invoke(vm_id, check=False):
    if not vm_id.startswith('/subscriptions/') or '/providers/Microsoft.Compute/virtualMachines/' not in vm_id:
        raise ValueError('invalid VM resource ID')
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    query=urllib.parse.urlencode({'api-version':'2018-02-01','resource':'https://management.azure.com/'})
    token_request=urllib.request.Request('http://169.254.169.254/metadata/identity/oauth2/token?'+query,headers={'Metadata':'true'})
    with opener.open(token_request,timeout=15) as response:
        token=json.load(response)['access_token']
    headers={'Authorization':'Bearer '+token}
    if check:
        url='https://management.azure.com'+vm_id+'/providers/Microsoft.Authorization/permissions?api-version=2022-04-01'
        with opener.open(urllib.request.Request(url,headers=headers),timeout=30) as response:
            permissions=json.load(response)['value']
        action='microsoft.compute/virtualmachines/deallocate/action'
        allowed=any(any(fnmatch.fnmatchcase(action,a.lower()) for a in p.get('actions',[])) and not any(fnmatch.fnmatchcase(action,a.lower()) for a in p.get('notActions',[])) for p in permissions)
        if not allowed:raise RuntimeError('self-deallocation permission not effective')
        print('Managed identity and self-deallocation permission verified; no token logged.')
    else:
        url='https://management.azure.com'+vm_id+'/deallocate?api-version=2024-07-01'
        with opener.open(urllib.request.Request(url,data=b'',headers=headers,method='POST'),timeout=30) as response:
            print('Self-deallocation accepted:',response.status)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',type=Path,default=Path('/etc/cos242-cost-guard.json'))
    p.add_argument('--check',action='store_true')
    args=p.parse_args();invoke(json.loads(args.config.read_text())['vm_id'],args.check)
