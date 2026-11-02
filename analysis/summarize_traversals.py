"""Validate BFS/DFS repetitions and export Azure-style timing statistics."""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics
from visualization_data import checkpoint, number, read_rows


def summarize(collected: Path, config: Path, output: Path, expected=None):
    rows, details, paired = [], [], []
    experiment_identity = None
    for cfg in read_rows(config):
        name = cfg['graph']
        root = collected / name
        input_record = json.loads((root / 'input.json').read_text())
        sources = None
        reached = None
        raw = {}
        for representation in cfg['representations'].split('|'):
            count = int(cfg.get('k_'+representation) or 100) if expected is None else expected
            rss = []
            for algorithm in ('bfs', 'dfs'):
                directory = root / f'timing_{representation}_{algorithm}'
                job = checkpoint(directory)
                settings = job['settings']
                identity = (settings['binary_sha256'], job['run']['hostname'], settings['seed'])
                if experiment_identity is not None and identity != experiment_identity:
                    raise ValueError('mixed binaries, hosts or random seeds in experiment')
                experiment_identity = identity
                if settings['graph_sha256'] != input_record['sha256_plaintext']:
                    raise ValueError(f'{name}: timing input differs')
                if settings['k'] != count or settings['representation'] != representation or settings['algorithm'] != algorithm:
                    raise ValueError(f'{name}: unexpected timing settings')
                samples = read_rows(directory / 'timings.csv')
                if len(samples) != count or any(r['status'] != 'ok' or r['algorithm'] != algorithm for r in samples):
                    raise ValueError(f'{name}/{algorithm}: requires {count} successful samples')
                ids = [int(r['source']) for r in samples]
                if len(set(ids)) != count or any(not 1 <= v <= input_record['vertices'] for v in ids):
                    raise ValueError(f'{name}: invalid/duplicate random sources')
                if any(int(r['seed']) != settings['seed'] for r in samples):
                    raise ValueError(f'{name}: inconsistent random seed')
                reach = [int(r['reachable_vertices']) for r in samples]
                if sources is not None and (sources[:min(len(sources),len(ids))] != ids[:min(len(sources),len(ids))] or reached[:min(len(sources),len(ids))] != reach[:min(len(sources),len(ids))]):
                    raise ValueError(f'{name}: BFS/DFS sources or reachable counts differ')
                if sources is None or len(ids) > len(sources):
                    sources, reached = ids, reach
                times = [number(r['elapsed_micros']) for r in samples]
                if min(times) < 0:
                    raise ValueError('negative elapsed time')
                raw[(representation,algorithm)] = times
                mean, median = statistics.mean(times), statistics.median(times)
                rows.append([name, representation, algorithm+'_micros', mean, median, count, 'ok'])
                memory = job['graph_rss_minus_baseline_bytes'] / 1024**2
                rss.append(memory)
                rows.append([name, representation, algorithm+'_memory_mb_graph', memory, memory, 1, 'ok'])
                details.append([name, representation, algorithm, count, settings['seed'], mean, median,
                                min(times), max(times), statistics.stdev(times) if len(times)>1 else 0,
                                min(reach), max(reach), job['run']['hostname'], settings['binary_sha256']])
            rows += [[name, representation, 'vertices', input_record['vertices'], input_record['vertices'], 1, 'ok'],
                     [name, representation, 'memory_mb_graph', statistics.mean(rss), statistics.median(rss), 2, 'ok']]
        if 'matrix' in cfg['representations'].split('|'):
            for algorithm in ('bfs','dfs'):
                matched = min(len(raw[('csr',algorithm)]),len(raw[('matrix',algorithm)]))
                for representation in ('csr','matrix'):
                    times = raw[(representation,algorithm)][:matched]
                    paired.append([name,representation,algorithm+'_micros',statistics.mean(times),statistics.median(times),matched,'ok'])
    output.mkdir(parents=True, exist_ok=True)
    for name, header, content in [
        ('results_traversals_paired.csv', ['graph','representation','metric','value_average','value_median','samples','status'], paired),
        ('results_traversals.csv', ['graph','representation','metric','value_average','value_median','samples','status'], rows),
        ('traversal_summary.csv', ['graph','representation','algorithm','samples','seed','mean_micros','median_micros','min_micros','max_micros','sample_stddev_micros','min_reachable','max_reachable','host','binary_sha256'], details)]:
        path = output / name
        temporary = path.with_suffix('.csv.tmp')
        with temporary.open('w',newline='') as dst:
            writer=csv.writer(dst);writer.writerow(header);writer.writerows(content)
        temporary.replace(path)
    print(f'Validated {len(details)} jobs, {sum(int(row[3]) for row in details)} traversals')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--collected',type=Path,default=Path('data/collected_traversals'))
    parser.add_argument('--config',type=Path,default=Path('config_traversals.csv'))
    parser.add_argument('--out',type=Path,default=Path('data/azure_traversals'))
    parser.add_argument('--expected',type=int,default=None,help='Override planned counts; default: k_csr/k_matrix in config, falling back to 100')
    args=parser.parse_args()
    summarize(args.collected,args.config,args.out,args.expected)
