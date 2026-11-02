"""Check saved search reachability against independently collected component sizes."""
from __future__ import annotations
import argparse
import csv
import gzip
import hashlib
import json
from pathlib import Path
from visualization_data import checkpoint, read_rows


def validate(collected: Path, statistics: Path, config: Path):
    result = {'samples_verified': 0, 'graphs': {}}
    for cfg in read_rows(config):
        name = cfg['graph']
        stats = statistics / name / 'stats'
        record = checkpoint(stats)
        summary = {r['metric']: int(float(r['value'])) for r in read_rows(stats / 'summary.csv')
                   if r['metric'] in {'vertices', 'component_count'}}
        source = json.loads((collected / name / 'input.json').read_text())
        if record['settings']['graph_sha256'] != source['sha256_plaintext']:
            raise ValueError(f'{name}: statistics and timings use different inputs')
        samples = [r for rep in cfg['representations'].split('|') for alg in ('bfs', 'dfs')
                   for r in read_rows(collected / name / f'timing_{rep}_{alg}' / 'timings.csv')]
        wanted = {int(r['source']) for r in samples}
        if summary['component_count'] == 1:
            expected = dict.fromkeys(wanted, summary['vertices'])
        else:
            sizes = {int(r['component']): int(r['vertices']) for r in read_rows(stats / 'components.csv')}
            path = stats / 'vertices.csv.gz'
            with path.open('rb') as src:
                if hashlib.file_digest(src, 'sha256').hexdigest() != record['artifacts']['vertices.csv.gz']:
                    raise ValueError(f'{name}: changed per-vertex component data')
            expected = {}
            with gzip.open(path, 'rt', newline='') as src:
                rows = csv.reader(src)
                header = next(rows)
                id_col, component_col = header.index('id'), header.index('component')
                for row in rows:
                    vertex = int(row[id_col])
                    if vertex in wanted:
                        expected[vertex] = sizes[int(row[component_col])]
                        if len(expected) == len(wanted):
                            break
        if expected.keys() != wanted:
            raise ValueError(f'{name}: source missing from component data')
        for sample in samples:
            if int(sample['reachable_vertices']) != expected[int(sample['source'])]:
                raise ValueError(f'{name}: search reachable count differs from component size')
        result['samples_verified'] += len(samples)
        result['graphs'][name] = {'distinct_sources': len(wanted), 'samples_verified': len(samples)}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--collected', type=Path, default=Path('data/azure_traversals/collected_traversals'))
    parser.add_argument('--statistics', type=Path, default=Path('data/collected'))
    parser.add_argument('--config', type=Path, default=Path('config_traversals.csv'))
    parser.add_argument('--out', type=Path, default=Path('data/azure_traversals/reachability_validation.json'))
    args = parser.parse_args()
    result = validate(args.collected, args.statistics, args.config)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print('Reachability checked against component sizes:', result['samples_verified'], 'samples')
