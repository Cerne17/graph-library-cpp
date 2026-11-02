"""Forecast the Azure traversals from a short local pilot and saved full statistics."""
from __future__ import annotations
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import statistics
from prepare_traversals import bitmatrix_bytes
from visualization_data import ROOT, checkpoint, read_rows


def planned_count(upper_pair_seconds, budget=300, minimum=10, maximum=100):
    """Use the largest conventional sample size within a per-pair time budget."""
    if not math.isfinite(upper_pair_seconds) or upper_pair_seconds < 0 or not math.isfinite(budget) or budget <= 0:
        raise ValueError('time estimates and budgets must be finite and nonnegative (budget positive)')
    return max((k for k in (10,20,30,50,75,100) if minimum <= k <= maximum and k*upper_pair_seconds <= budget),default=minimum)


def component_work(root, source_ids):
    summary={r['metric']:float(r['value']) for r in read_rows(root/'stats/summary.csv')}
    n=int(summary['vertices']);m=int(summary['edges'])
    components={int(r['component']):int(r['vertices']) for r in read_rows(root/'stats/components.csv')}
    arcs={c:0 for c in components};membership={}
    # For a connected graph, or one nontrivial component plus isolates, no full scan is needed.
    large=[c for c,s in components.items() if s>1]
    if len(large)<=1:
        giant=large[0] if large else next(iter(components))
        arcs[giant]=2*m
        isolated={int(r['root']):int(r['component']) for r in read_rows(root/'stats/components.csv') if int(r['vertices'])==1}
        membership={source:isolated.get(source,giant) for source in source_ids}
    else:
        # Stream rather than retain all per-vertex rows (several million on Part 1).
        import gzip
        with gzip.open(root/'stats/vertices.csv.gz','rt',newline='') as src:
            for row in csv.DictReader(src):
                c=int(row['component']);arcs[c]+=int(row['degree'])
                v=int(row['id'])
                if v in source_ids:membership[v]=c
    expected_r=sum(s*s for s in components.values())/n
    expected_arcs=sum(components[c]*arcs[c] for c in components)/n
    work={v:(n+components[membership[v]]+arcs[membership[v]],components[membership[v]]) for v in source_ids}
    return summary,expected_r,n+expected_r+expected_arcs,work


def calibrate(azure, collected):
    # Historical comparisons use the same machine pair, but different sources and binaries.
    # They constrain a scenario range, not a measured BFS/DFS conversion factor.
    old={(r['graph'],r['algorithm']):r for r in read_rows(collected/'benchmark_summary.csv') if r['status']=='ok' and r['algorithm'].startswith('dijkstra_') and r['representation']=='csr'}
    ratios=[]
    for row in read_rows(azure/'results_weighted.csv'):
        key=(row['graph'],row['metric'].removesuffix('_micros'))
        if row['representation']=='csr' and row['status']=='ok' and key in old and float(old[key]['mean_micros'])>0:
            ratios.append(float(row['value_average'])/float(old[key]['mean_micros']))
    if not ratios:raise ValueError('historical Azure/local calibration unavailable')
    return min(ratios)*.8,max(ratios)*1.5,ratios


def estimate(config, collected, pilot, azure, output, report, budget=300):
    configs=read_rows(config)
    low,high,ratios=calibrate(azure,collected)
    # Matrix coefficients reflect scanning whole 64-bit words, not n*n individual bits.
    matrix_rates={'bfs':[],'dfs':[]}
    prepared={};pilot_overhead=0;loading={};missing_matrices=[];last_finished=None;input_preparation=0
    for cfg in configs:
        graph=cfg['graph'];root=collected/graph;proot=pilot/graph
        source_record=json.loads((proot/'input.json').read_text())
        if source_record['sha256_plaintext'] != json.loads((root/'input.json').read_text())['sha256_plaintext']:
            raise ValueError('pilot and statistics use different graph inputs')
        samples={}
        ids=set();finished=[];run_start=None
        for rep in cfg['representations'].split('|'):
            for alg in ('bfs','dfs'):
                directory=proot/f'timing_{rep}_{alg}'
                job=json.loads((directory/'job.json').read_text())
                finished.append(dt.datetime.fromisoformat(job['finished_at']).timestamp())
                run_start=dt.datetime.fromisoformat(job['run']['started_at']).timestamp()
                if job['status']!='ok':
                    if rep=='csr':raise ValueError(f'{graph}: missing successful CSR pilot')
                    continue
                checkpoint(directory)
                rows=read_rows(directory/'timings.csv')
                if not rows or any(r['status']!='ok' for r in rows):raise ValueError('invalid pilot samples')
                samples[(rep,alg)]=rows;ids.update(int(r['source']) for r in rows)
                duration=(dt.datetime.fromisoformat(job['finished_at'])-dt.datetime.fromisoformat(job['started_at'])).total_seconds()
                load=max(0,duration-sum(float(r['elapsed_micros'])/1e6 for r in rows))
                loading[(graph,rep,alg)]=load
                pilot_overhead += load
        input_preparation+=max(0,(proot/'input.json').stat().st_mtime-(last_finished if last_finished is not None else run_start))
        last_finished=max(finished)
        s,expected_r,expected_work,work=component_work(root,ids)
        prepared[graph]=(s,expected_r,expected_work,work,samples)
        words=(int(s['vertices'])+64)//64
        for alg in ('bfs','dfs'):
            for row in samples.get(('matrix',alg),[]):
                r=int(row['reachable_vertices'])
                if r>0:
                    matrix_work=int(s['vertices']) + r*words + work[int(row['source'])][0]-int(s['vertices'])-r
                    matrix_rates[alg].append(float(row['elapsed_micros'])/1e6/matrix_work)
    if any(not rates for rates in matrix_rates.values()):raise ValueError('small bit-matrix pilots are required')
    decisions=[];all100=[0.,0.];chosen=[0.,0.];count=0
    for cfg in configs:
        graph=cfg['graph'];s,r,expected_work,work,samples=prepared[graph]
        n=int(s['vertices']);pair_counts={}
        for rep in cfg['representations'].split('|'):
            times={};methods={}
            for alg in ('bfs','dfs'):
                if rep=='csr':
                    rates=[float(row['elapsed_micros'])/1e6/work[int(row['source'])][0] for row in samples[(rep,alg)]]
                    central=statistics.median(rates)*expected_work
                    lower=central*low*.8;upper=central*high*1.2
                    methods[alg]='piloto CSR corrigido pelo trabalho esperado das componentes'
                elif (rep,alg) in samples:
                    words=(n+64)//64
                    rates=[float(row['elapsed_micros'])/1e6/(work[int(row['source'])][1]*words+work[int(row['source'])][0]-work[int(row['source'])][1]) for row in samples[(rep,alg)]]
                    central=statistics.median(rates)*(r*words+expected_work-r)
                    lower=central*low*.8;upper=central*high*1.2
                    methods[alg]='piloto matricial corrigido pela componente alcançada'
                else:
                    missing_matrices.append((graph,alg,bitmatrix_bytes(n)))
                    word_visits=r*((n+64)//64)+expected_work-r
                    central=statistics.median(matrix_rates[alg])*word_visits
                    lower=central*low*.75;upper=central*high*1.5
                    methods[alg]='extrapolação de palavras de 64 bits a partir das matrizes pequenas; não medida localmente'
                times[alg]={'local_model_seconds':central,'azure_lower_seconds':lower,'azure_upper_seconds':upper}
            pair_upper=sum(t['azure_upper_seconds'] for t in times.values())
            k=planned_count(pair_upper,budget=budget)
            if rep=='matrix':k=min(k,pair_counts['csr'])
            pair_counts[rep]=k;cfg['k_'+rep]=str(k)
            sums=[sum(t['azure_lower_seconds'] for t in times.values()),pair_upper]
            all100=[a+100*b for a,b in zip(all100,sums)];chosen=[a+k*b for a,b in zip(chosen,sums)];count+=2*k
            decisions.append({'graph':graph,'representation':rep,'vertices':n,'edges':int(s['edges']),'density_simple':s['density_simple'],'expected_reachable_vertices':r,'times':times,'methods':methods,'repetitions_per_algorithm':k,'pair_budget_seconds':budget})
        cfg.setdefault('k_matrix','')
    with config.open('w',newline='') as dst:
        writer=csv.DictWriter(dst,fieldnames=['graph','weighting','representations','labels','source','k_csr','k_matrix']);writer.writeheader();writer.writerows(configs)
    # Includes conservative allowances for loading; network transfer is kept separate.
    extra_loading=sum(loading[(graph,'csr',alg)] for graph,alg,_ in missing_matrices)
    extra_zero_bytes=sum(size for _,_,size in missing_matrices)
    overhead=[(pilot_overhead+extra_loading+input_preparation)*low+extra_zero_bytes/10e9,
              (pilot_overhead+extra_loading+input_preparation)*high+extra_zero_bytes/1e9]
    source_bytes=sum((ROOT/'data'/(cfg['graph']+('.txt.gz' if (ROOT/'data'/(cfg['graph']+'.txt.gz')).is_file() else '.txt'))).stat().st_size for cfg in configs)
    model={'kind':'forecast_not_azure_measurement','calibration_ratios':ratios,'scenario_multiplier':[low,high],
           'assumptions':'CSR: n + vértices alcançados + arcos alcançados; matriz: n + vértices alcançados × ceil((n+1)/64) + arcos alcançados. Faixas são cenários, não intervalos de confiança.',
           'all_100_algorithm_seconds':all100,'chosen_algorithm_seconds':chosen,'loading_allowance_seconds':overhead,'measured_local_input_preparation_seconds':input_preparation,
           'all_100_run_seconds':[a+b for a,b in zip(all100,overhead)],'chosen_run_seconds':[a+b for a,b in zip(chosen,overhead)],
           'upload_bytes':source_bytes,'upload_seconds_at_100mbps':source_bytes*8/100e6,'upload_seconds_at_20mbps':source_bytes*8/20e6,
           'traversal_count':count,'decisions':decisions,
           'config_sha256':hashlib.sha256(config.read_bytes()).hexdigest(),
           'pilot_sha256':{str(p.relative_to(pilot)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(pilot.glob('*/*/timings.csv'))},
           'input_sha256':{cfg['graph']:json.loads((collected/cfg['graph']/'input.json').read_text())['sha256_plaintext'] for cfg in configs}}
    output.write_text(json.dumps(model,ensure_ascii=False,indent=2)+'\n')
    def mins(pair):return '–'.join(f'{v/60:.1f}' for v in pair)
    lines=['# Estimativa de duração da rodada de buscas','',
           '**Previsão antes de criar a VM; não são tempos medidos no Azure.** Piloto local de três origens por busca e representação viável, combinado às estatísticas completas e à rodada histórica. As matrizes grandes não foram alocadas no Mac.','',
           f"Com 100 repetições em todas as combinações: **{mins(model['all_100_run_seconds'])} minutos** para carga e buscas. Plano ajustado: **{mins(model['chosen_run_seconds'])} minutos**, {count} travessias. Esses valores não incluem provisionamento, compilação nem transferência.",'',
           f"Transferência de aproximadamente {source_bytes/1e9:.2f} GB: {model['upload_seconds_at_100mbps']/60:.1f} min a 100 Mb/s ou {model['upload_seconds_at_20mbps']/60:.1f} min a 20 Mb/s, sem contar sobrecarga. Reservar adicionalmente 5–15 min para preparar e testar a VM; é uma margem de planejamento.",'',
           '## Repetições e tempos previstos','',
           '| Grafo | Armazenamento | Repetições por busca | BFS por origem (s) | DFS por origem (s) |','| --- | --- | ---: | ---: | ---: |']
    for d in decisions:
        name='Rede de colaboração' if d['graph']=='rede_colaboracao' else 'Grafo Ponderado '+d['graph'].removeprefix('grafo_W_') if d['graph'].startswith('grafo_W_') else 'Grafo '+d['graph'].removeprefix('grafo_')
        def seconds(alg):return f"{d['times'][alg]['azure_lower_seconds']:.3g}–{d['times'][alg]['azure_upper_seconds']:.3g}"
        lines.append(f"| {name} | {'CSR' if d['representation']=='csr' else 'Matriz com bitmask'} | {d['repetitions_per_algorithm']} | {seconds('bfs')} | {seconds('dfs')} |")
    lines += ['', '## Reproduzir o piloto e o plano','', '```bash', 'python3 analysis/collect.py --config config_traversals.csv --phases timing --algorithms bfs,dfs --ignore-config-counts --k 3 --memory-limit-gb 12 --timeout 600 --out build/traversal_pilot', 'python3 analysis/estimate_traversals.py', '```', '', 'O limite de 12 GiB é o limite conservador usado neste Mac; ele omite as matrizes de aproximadamente 16 GiB. O limite planejado para a VM continua 28 GiB.', '', '## Método e limites','',
              'O custo de CSR considera a inicialização de vetores para todos os vértices e a travessia de vértices e arcos da componente alcançada. Densidade entra pelo número efetivo de arestas, não como fator multiplicado novamente. Para a matriz com bitmask, a varredura percorre palavras de 64 bits por linha: o modelo inclui também inicialização e processamento dos vizinhos encontrados, mas não supõe um teste individual para cada uma das n² células.', '',
              'Componentes são ponderadas pela probabilidade de uma origem uniforme pertencer a cada uma: E[V alcançados] = Σ tamanho²/n. A quantidade esperada de arcos usa os graus de cada componente. Isso importa especialmente nos grafos desconexos da Parte 1 e na rede de colaboração.', '',
              f'O plano usa até {budget/60:g} minutos previstos no cenário superior para BFS+DFS de cada grafo/representação, com 10 a 100 repetições. Matrizes nunca recebem mais repetições que CSR. Esse orçamento não é um limite rígido de execução; o mínimo de dez pode excedê-lo.', '',
              'As diferenças históricas entre Azure e Mac vêm de Dijkstra em dois grafos e não são uma calibração direta das buscas. CPU, caches, largura de banda de RAM, compilador e carga podem alterar a duração real. Por isso, são mostradas faixas amplas; não são intervalos de confiança. A extrapolação matricial recebe margem adicional.', '',
              'BFS e DFS mantêm as mesmas origens por representação. Quando CSR recebe mais repetições, a comparação com matriz usa apenas o prefixo comum de origens; a média CSR completa é publicada separadamente. O número de amostras deve acompanhar toda média/mediana. Dez amostras oferecem evidência exploratória limitada, não a precisão de cem.', '',
              'O plano executável está em `config_traversals.csv`; o detalhe numérico e hashes estão em `docs/traversal_forecast.json`. Os dados brutos do piloto local permanecem em `build/traversal_pilot/` e não entram nos resultados rotulados como Azure.','']
    report.write_text('\n'.join(lines),encoding='utf-8')
    print('All 100:',mins(model['all_100_run_seconds']),'min; adjusted:',mins(model['chosen_run_seconds']),'min;',count,'traversals')
    return model


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name,default in [('config',ROOT/'config_traversals.csv'),('collected',ROOT/'data/collected'),('pilot',ROOT/'build/traversal_pilot'),('azure',ROOT/'data/azure'),('out',ROOT/'docs/traversal_forecast.json'),('report',ROOT/'docs/TRAVERSAL_ESTIMATE.md')]:p.add_argument('--'+name,type=Path,default=default)
    p.add_argument('--pair-budget-seconds',type=float,default=300)
    args=p.parse_args();estimate(args.config,args.collected,args.pilot,args.azure,args.out,args.report,args.pair_budget_seconds)
