"""Run artificial cases against hand-computed expectations and write a PT-BR report."""
from __future__ import annotations
import argparse
import csv
import io
import json
import math
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parent.parent


def run_cases(binary, fixtures, output):
    cases=json.loads((fixtures/'manifest.json').read_text())
    output.mkdir(parents=True,exist_ok=True)
    all_rows=[]
    lines=['# Casos artificiais de referência','',
           'Resultados esperados calculados manualmente em `tests/data/baselines/manifest.json`. Todos os grafos são não direcionados e a origem é o vértice 1. Vizinhos são visitados em ordem crescente. São casos de correção, não estimativas de desempenho em grafos grandes.','',
           'Regenerar: `make baselines`. `-1` indica vértice inalcançável nas buscas; `∞` indica distância ponderada infinita. A profundidade de DFS não é uma distância mínima.','']
    for case in cases:
        result=subprocess.run([str(binary),str(fixtures/(case['id']+'.txt')),case['weighting'],str(case['source'])],capture_output=True,text=True,check=True)
        rows=list(csv.DictReader(io.StringIO(result.stdout)))
        expected_algorithms=['bfs','dfs']+(['dijkstra_array','dijkstra_heap','dijkstra_lazy'] if case['weighting']=='weighted' else [])
        if {r['algorithm'] for r in rows} != set(expected_algorithms):raise ValueError('missing baseline algorithm')
        lines += ['## '+case['title'],'',case['note'],'','| Algoritmo | Valores por vértice, a partir de 1 | Resultado |','| --- | --- | --- |']
        for algorithm in expected_algorithms:
            selected=[r for r in rows if r['algorithm']==algorithm]
            negative=algorithm.startswith('dijkstra') and case['negative']
            if negative:
                if len(selected)!=1 or selected[0]['status']!='negative_weight':raise ValueError('negative weights accepted')
                values='Entrada rejeitada: peso negativo'
            else:
                expected=case['bfs'] if algorithm=='bfs' else case['dfs'] if algorithm=='dfs' else case['dist']
                if [int(r['vertex']) for r in selected]!=list(range(1,case['vertices']+1)):raise ValueError('incomplete vertex results')
                for row,value in zip(selected,expected):
                    actual=float(row['value'])
                    valid=(actual==math.inf) if value is None else math.isclose(actual,value,rel_tol=1e-12,abs_tol=1e-12)
                    if row['status']!='ok' or not valid:raise ValueError(f"{case['id']}/{algorithm}/{row['vertex']}: expected {value}, got {actual}")
                values=', '.join('∞' if v is None else str(v) for v in expected)
            title={'bfs':'BFS — distâncias','dfs':'DFS — profundidades','dijkstra_array':'Dijkstra — vetor','dijkstra_heap':'Dijkstra — heap indexado','dijkstra_lazy':'Dijkstra — heap tardio'}[algorithm]
            lines.append(f'| {title} | {values} | Correto |')
        all_rows += [{'case':case['id'],**r} for r in rows]
        lines += ['',f"[Entrada do grafo](../tests/data/baselines/{case['id']}.txt)",'']
    with (output/'results.csv').open('w',newline='') as dst:
        writer=csv.DictWriter(dst,fieldnames=['case','algorithm','vertex','value','status']);writer.writeheader();writer.writerows(all_rows)
    report=ROOT/'docs/BASELINES.md';report.write_text('\n'.join(lines),encoding='utf-8')
    print(f'{len(cases)} artificial cases passed; {len(all_rows)} vertex/algorithm results')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/baseline_cases')
    parser.add_argument('--fixtures',type=Path,default=ROOT/'tests/data/baselines')
    parser.add_argument('--out',type=Path,default=ROOT/'docs/baselines')
    args=parser.parse_args();run_cases(args.binary,args.fixtures,args.out)
