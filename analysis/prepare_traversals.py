"""Select CSR and eligible Part 1 bit matrices using actual row padding."""
import argparse
import csv
import json
from pathlib import Path
from visualization_data import ROOT, read_rows


def bitmatrix_bytes(n):
    # Same allocation as matrix_cells: n+1 rows, ceil((n+1)/64) words per row.
    return (n+1) * ((n+64)//64) * 8


def prepare(config, collected, output, plan, limit_gib=28):
    configs=read_rows(config);decisions=[]
    for cfg in configs:
        info=json.loads((collected/cfg['graph']/'input.json').read_text())
        n=info['vertices']
        matrix=bitmatrix_bytes(n) if cfg['weighting']=='unweighted' else None
        eligible=matrix is not None and matrix < limit_gib*1024**3
        cfg['representations']='csr|matrix' if eligible else 'csr'
        cfg['k_csr']='100'
        cfg['k_matrix']='100' if eligible else ''
        decisions.append({'graph':cfg['graph'],'vertices':n,'bitmatrix_bytes':matrix,
                          'bitmatrix_gib':matrix/1024**3 if matrix is not None else None,
                          'matrix_selected':eligible,'reason':'below_limit' if eligible else 'weighted_csr_only' if matrix is None else 'exceeds_limit'})
    with output.open('w',newline='') as dst:
        writer=csv.DictWriter(dst,fieldnames=['graph','weighting','representations','labels','source','k_csr','k_matrix']);writer.writeheader();writer.writerows(configs)
    plan.write_text(json.dumps({'matrix_limit_gib':limit_gib,'total_memory_budget_gib':28,'reserve_target_gib':'2–4 on the 32-GiB VM','jobs':decisions,'traversal_count_at_100':sum(400 if d['matrix_selected'] else 200 for d in decisions)},indent=2,ensure_ascii=False)+'\n')
    print(f"Selected {sum(d['matrix_selected'] for d in decisions)} bit matrices; {sum(400 if d['matrix_selected'] else 200 for d in decisions)} traversals")


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',type=Path,default=ROOT/'config_collect.csv')
    p.add_argument('--collected',type=Path,default=ROOT/'data/collected')
    p.add_argument('--out',type=Path,default=ROOT/'config_traversals.csv')
    p.add_argument('--plan',type=Path,default=ROOT/'docs/traversal_memory_plan.json')
    args=p.parse_args();prepare(args.config,args.collected,args.out,args.plan)
