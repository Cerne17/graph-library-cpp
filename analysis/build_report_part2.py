"""Generate Portuguese LaTeX reports from saved results; never run graph benchmarks."""
from __future__ import annotations
import csv
import hashlib
import json
import re
from pathlib import Path
from visualization_data import ROOT, checkpoint, number, read_rows


def esc(value):
    return ''.join({'\\':r'\textbackslash{}','&':r'\&','%':r'\%','$':r'\$','#':r'\#','_':r'\_','{':r'\{','}':r'\}'}.get(c,c) for c in str(value))


def num(value, digits=6):
    return r'\num{'+format(float(value), f'.{digits}g')+'}'


def title(name):
    return 'Rede de colaboração' if name=='rede_colaboracao' else ('Grafo Ponderado '+name.removeprefix('grafo_W_') if name.startswith('grafo_W_') else 'Grafo '+name.removeprefix('grafo_'))


def table(headers, rows, columns=None, caption=None, size=r'\small'):
    cols=columns or ('l'+'r'*(len(headers)-1))
    out=[r'\par\smallskip\noindent\begin{minipage}{\textwidth}\centering',r'\begingroup'+size,r'\setlength{\tabcolsep}{4pt}',r'\begin{tabular}{'+cols+'}',r'\toprule',' & '.join(headers)+r' \\',r'\midrule']
    out += [' & '.join(row)+r' \\' for row in rows]
    out += [r'\bottomrule',r'\end{tabular}',r'\endgroup']
    if caption:out += [r'\captionof{table}{'+caption+'}']
    out += [r'\end{minipage}\par\smallskip']
    return '\n'.join(out)+'\n'


PREAMBLE=r"""\documentclass[a4paper,11pt]{article}
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage[brazilian]{babel}
\usepackage{lmodern,microtype}
\usepackage[margin=2.15cm]{geometry}
\usepackage{amsmath,amssymb,booktabs,array,caption,float,graphicx,xcolor}
\usepackage{siunitx,setspace,enumitem,tikz}
\usetikzlibrary{arrows.meta,positioning}
\usepackage[colorlinks=true,linkcolor=blue,urlcolor=blue]{hyperref}
\sisetup{output-decimal-marker={,},group-separator={\,},group-minimum-digits=4}
\setstretch{1.07}
\setlength{\parskip}{3pt}
\setlength{\emergencystretch}{2em}
\captionsetup{font=small,labelfont=bf,skip=4pt}
\setlist{nosep,leftmargin=*}
\newcommand{\cod}[1]{\texttt{\detokenize{#1}}}
\newcommand{\rota}[1]{{\fontsize{8.5}{10.2}\selectfont #1}}
\graphicspath{{figures/}{docs/figures/}}
\title{Trabalho 2 - Teoria dos Grafos}
\author{João Costa e Miguel Cerne}
\date{Outubro de 2026}
"""

ARCHITECTURE=r"""
\begin{center}
\begin{tikzpicture}[>=Stealth,box/.style={draw=black!55,rounded corners,align=center,font=\scriptsize,minimum height=8mm,text width=4.4cm},node distance=7mm and 12mm]
\node[box] (pol) {Políticas: direção e ponderação};
\node[box,right=of pol] (io) {Leitura: arestas e mapa de nomes};
\node[box,below=of pol] (storage) {Armazenamentos: CSR e matriz};
\node[box,below=of io] (concept) {Contratos: \texttt{graph\_like}, \texttt{weighted\_graph}};
\node[box,below=of storage] (alg) {Funções livres: buscas e Dijkstra};
\node[box,below=of concept] (heap) {Heap indexado e resultados \texttt{dist}/\texttt{parent}};
\draw[->] (pol)--(storage);
\draw[->] (io)--node[above,sloped,font=\tiny]{constrói}(storage);
\draw[->,dashed] (storage)--node[above,font=\tiny]{satisfaz}(concept);
\draw[->,dashed] (alg)--node[above,sloped,font=\tiny]{requer}(concept);
\draw[->] (alg)--node[above,font=\tiny]{usa / produz}(heap);
\end{tikzpicture}
\captionof{figure}{Arquitetura da biblioteca nova. Setas tracejadas representam contratos verificados em compilação, não herança virtual. O diagrama Mermaid detalhado está em \protect\path{docs/diagrama_classes_atual.mmd}.}
\end{center}
"""


def path_table(rows, name, collaboration=False):
    content=[]
    combined=isinstance(name,list)
    names=name if combined else [name]
    for r in rows:
        if r['graph'] not in names:continue
        if r['status']=='unreachable':
            route='Sem caminho';distance=r'$+\infty$';hops='--'
        else:
            vertices=(r['path_names'].split(' | ') if collaboration else r['path'].split())
            route=r'\rota{'+r'\,$\to$\,\allowbreak '.join(esc(v) for v in vertices)+'}'
            distance=num(r['distance'], 9 if collaboration else 8);hops=r['hops']
        content.append(([r['graph'].rsplit('_',1)[-1]] if combined else [])+[esc(r['target_name']) if collaboration else r['target'],distance,hops,route])
    columns=(r'>{\raggedright\arraybackslash}p{2.6cm} r r >{\raggedright\arraybackslash}p{10.2cm}' if collaboration else r'r r r >{\raggedright\arraybackslash}p{12.5cm}')
    if combined:columns=r'r r r r >{\raggedright\arraybackslash}p{11.7cm}'
    headers=(['Grafo'] if combined else [])+['Destino',r'$d$','Arestas','Caminho mínimo completo']
    caption=('Grafos Ponderados 3 a 6. Origem: vértice 10.' if combined else title(name)+('. Origem: Edsger W. Dijkstra.' if collaboration else '. Origem: vértice 10.'))
    return table(headers,content,columns,caption=caption,size=r'\fontsize{8.7}{10.4}\selectfont')


def main():
    docs=ROOT/'docs';configs=read_rows(ROOT/'config_collect.csv')
    summaries={}
    for cfg in configs:
        directory=ROOT/'data/collected'/cfg['graph'];job=checkpoint(directory/'stats')
        source=json.loads((directory/'input.json').read_text())
        if job['settings']['graph_sha256']!=source['sha256_plaintext']:raise ValueError('statistics input mismatch')
        summaries[cfg['graph']]={r['metric']:number(r['value']) for r in read_rows(directory/'stats/summary.csv')}
    weighted=[cfg['graph'] for cfg in configs if cfg['weighting']=='weighted']
    historical=read_rows(ROOT/'data/azure/results_weighted.csv')
    metrics={(r['graph'],r['representation'],r['metric']):r for r in historical}
    paths=read_rows(ROOT/'data/azure/paths_weighted.csv')
    if len(paths)!=30 or any(r['representation']!='csr' for r in paths):raise ValueError('expected 30 CSR case studies')
    for name in weighted:
        selected=[r for r in paths if r['graph']==name]
        if len(selected)!=5:raise ValueError('missing case-study target')
        if name!='rede_colaboracao':
            if {int(r['target']) for r in selected}!={20,30,40,50,60} or any(r['source']!='10' for r in selected):raise ValueError('wrong numeric case-study targets')
        else:
            expected_names={'Alan M. Turing','J. B. Kruskal','Jon M. Kleinberg','Éva Tardos','Daniel R. Figueiredo'}
            if {r['target_name'] for r in selected}!=expected_names or any(r['source_name']!='Edsger W. Dijkstra' for r in selected):raise ValueError('wrong collaboration names')
        checkpoint(ROOT/'data/collected'/name/'cases')
        saved={(r['source'],r['target']):r for r in read_rows(ROOT/'data/collected'/name/'cases/cases.csv') if r['algorithm']=='dijkstra_heap'}
        for r in selected:
            other=saved[(r['source'],r['target'])]
            if r['status']!=other['status'] or (r['status']=='ok' and abs(float(r['distance'])-float(other['distance']))>1e-9):raise ValueError('Azure/current cases disagree')
            if r['status']=='ok':
                seq=r['path'].split()
                if seq[0]!=r['source'] or seq[-1]!=r['target'] or len(seq)!=int(r['hops'])+1:raise ValueError('invalid path endpoints or length')
        for alg in ('dijkstra_heap','dijkstra_lazy'):
            record=metrics[(name,'csr',alg+'_micros')]
            if record['status']!='ok' or int(record['samples'])!=100:raise ValueError('missing successful Dijkstra timing samples')
    traversal=read_rows(docs/'results/traversals/traversal_summary.csv')
    if len(traversal)!=32 or sum(int(r['samples']) for r in traversal)!=2150:raise ValueError('traversal summary incomplete')
    by_search={(r['graph'],r['representation'],r['algorithm']):r for r in traversal}
    paired={(r['graph'],r['representation'],r['metric']):r for r in read_rows(docs/'results/traversals/results_traversals_paired.csv')}
    rss={(r['graph'],r['representation'],r['metric']):r for r in read_rows(docs/'results/traversals/results_traversals.csv')}
    def perf(name,alg):
        r=metrics[(name,'csr',alg+'_micros')]
        return (num(float(r['value_average'])/1e6)+' / '+num(float(r['value_median'])/1e6)) if r['status']=='ok' else r'\textit{Não medido}'
    structure=[]
    for name in weighted:
        s=summaries[name];matrix=8*(s['vertices']+1)**2/1024**3
        structure.append([title(name),num(s['vertices'],12),num(s['edges'],12),num(100*s['density_simple'],3),num(s['component_count'],12),num(metrics[(name,'csr','memory_mb_graph')]['value_average'],5),num(matrix,3)])
    timings=[[title(name),perf(name,'dijkstra_array'),perf(name,'dijkstra_heap'),perf(name,'dijkstra_lazy')] for name in weighted]
    maintex=PREAMBLE+r"""\begin{document}
\maketitle
\vspace{-1.2em}
\section{Introdução}
Este relatório estende a biblioteca da Parte 1 para grafos não direcionados ponderados, com pesos reais, distâncias e árvores de caminhos mínimos. O conjunto experimental compreende os Grafos Ponderados 3 a 7 e a rede de colaboração. Preservam-se as análises de tamanho, memória e componentes do relatório anterior, agora com as representações e os protocolos atuais. Código-fonte e estudos de caso: \url{https://github.com/Cerne17/graph-library-cpp}.
\section{Implementação}
A versão da Parte 1, baseada em \cod{Graph} e métodos virtuais, permanece como legado. A biblioteca nova é \emph{header-only}, em C++23, com \emph{templates}, políticas e conceitos. Os algoritmos continuam como funções livres: não foram incorporados como métodos de \cod{Graph}. Os aliases \cod{csr_graph<D,W>} e \cod{matrix_graph<D,W>} selecionam armazenamento, direção e ponderação em compilação.

CSR guarda deslocamentos de 64 bits, destinos de 32 bits e pesos \cod{double} em vetores paralelos; ordenar vizinhos preserva a associação destino--peso. Pesos não ocupam espaço nas instanciações não ponderadas. A matriz ponderada usa $(N+1)^2$ células de 8 bytes e $+\infty$ para ausência; a não ponderada usa palavras de 64 bits com bitmask. CSR preserva arestas paralelas; a matriz conserva o menor peso. O leitor aceita \cod{u v peso}, valida IDs, descarta laços e os contabiliza.
"""+r"""
O diagrama atualizado da arquitetura está em \path{docs/diagrama_classes_atual.mmd}; uma visão resumida consta no material complementar.

As três versões produzem \cod{shortest_paths}, com \cod{source}, \cod{dist} e \cod{parent}. Distância infinita e pai zero indicam inalcançabilidade; a reconstrução segue os pais e inverte a sequência. Dijkstra rejeita pesos negativos no grafo armazenado, informando caso não implementado; aceita peso zero. O heap indexado mantém um mapa vértice--posição atualizado em cada troca e oferece \cod{update_key} em $O(\log N)$.
"""+table(['Versão','Seleção / atualização','Tempo no pior caso em CSR'],[
['Vetor','Varredura de estimativas',r'$O(N^2+M)$'],['Heap indexado','Uma entrada por vértice',r'$O((N+M)\log N)$'],['Heap com entradas obsoletas','Insere; descarta versões antigas',r'$O(N+(N+M)\log(M+1))$']],columns='lll',size=r'\footnotesize')+r"""
A terceira versão, baseada em \cod{std::priority_queue}, é apenas uma comparação adicional: não substitui o heap com atualização de chave exigido no enunciado. O arquivo Mermaid também distingue os executáveis e os fluxos de coleta do legado.
\newpage
\section{Análise dos resultados}
\subsection{Estrutura, memória e protocolo}
Sejam $N$ os vértices, $M$ as arestas válidas, contando paralelas, e $M_u$ os pares distintos de vértices. A densidade simples é $\delta=2M_u/[N(N-1)]$. Na Tabela~\ref{tab:estrutura}, o RSS de CSR é medido; a coluna de matriz é apenas o tamanho teórico das células, sem temporários nem vetores das buscas.
"""+table(['Grafo',r'$N$',r'$M$',r'$\delta$ (\%)','Comp.','CSR (MiB)','Matriz (GiB)'],structure,columns='lrrrrrr',caption=r'Estrutura e memória dos grafos ponderados.\label{tab:estrutura}',size=r'\fontsize{8.5}{10.5}\selectfont')+r"""
Todos são esparsos. Para CSR ponderado, a carga útil aproxima-se de $8(N+2)+24M$ bytes; para a matriz ponderada, de $8(N+1)^2$. Mesmo o menor caso requer cerca de 74,5 GiB apenas em células, inviabilizando todas as matrizes ponderadas na VM de 32 GiB. Esse limite não se aplica à matriz não ponderada com bitmask.

A rodada de Dijkstra ocorreu em 30/10/2026, no Azure, em VM \cod{Standard_E4as_v4}, região \cod{belgiumcentral}, 4 vCPUs, 32 GiB, AMD EPYC 7763 e g++ 13.3. Usaram-se $k=100$ origens distintas, sorteadas com semente 2025, e a mesma sequência entre versões. Sua ordem de execução alterna a cada origem. O intervalo medido inclui a chamada e suas alocações, mas exclui leitura, descompactação, validação e exportação. Os campos históricos \cod{memory_mb_*} estão, de fato, em MiB ($2^{20}$ bytes). RSS da representação não é pico de memória de Dijkstra.
\subsection{Tempos de Dijkstra}
"""+table(['Grafo','Vetor','Heap indexado','Entradas obsoletas'],timings,columns='lrrr',caption='Média / mediana em segundos; 100 origens por versão medida.',size=r'\fontsize{9}{11}\selectfont')+r"""
\textbf{Lacuna perante o enunciado:} a versão por vetor só foi medida no Grafo Ponderado 3; nas demais entradas, sua configuração foi \cod{array=no}. Portanto, a comparação obrigatória vetor--heap está incompleta nos Grafos Ponderados 4 a 7; os tempos da rede são uma análise adicional. ``Não medido'' não significa zero, falha nem estimativa. A versão com entradas obsoletas não preenche essa lacuna.

No Grafo Ponderado 3, o vetor levou cerca de 224 vezes o tempo médio do heap indexado. No Grafo Ponderado 7, a versão com entradas obsoletas foi cerca de 2,71 vezes mais rápida que o indexado. Logo, o limite assintótico não determina sozinho o desempenho: ocupação da fila, alocações e localidade são hipóteses para investigação, não causas isoladas já demonstradas.
\newpage
\subsection{Distâncias e caminhos mínimos: origem 10}
As tabelas apresentam um caminho mínimo completo por destino, calculado com o heap indexado. Distância é soma dos pesos; a coluna ``Arestas'' é o comprimento desse caminho em número de arestas, não uma distância de BFS. Empates podem produzir outros caminhos igualmente válidos. Os resultados atuais foram conferidos com a rodada histórica do Azure.
"""
    maintex+=path_table(paths,weighted[:4])
    maintex+=r"""\newpage
\subsection{Maior grafo ponderado e rede de colaboração}
O número de arestas de um caminho não determina seu peso: por exemplo, no Grafo Ponderado 7, o destino 40 exige 46 arestas no caminho exibido. A rede usa exatamente as grafias pedidas no enunciado; o identificador de Edsger W. Dijkstra é 2722. As distâncias da rede abaixo são arredondadas a nove algarismos significativos; os CSVs preservam os valores exportados.
"""+path_table(paths,'grafo_W_7')+path_table(paths,'rede_colaboracao',True)+r"""
Alan M. Turing (ID 11365) é um vértice existente, porém está em outra componente da origem: a distância é infinita e não há caminho. Isso difere de nome desconhecido. Os pesos da rede são inversamente relacionados à coautoria; minimizar sua soma não equivale a minimizar o número de intermediários, nem permite inferir sozinho uma relação social direta.
\newpage
\subsection{Continuidade com as análises da Parte 1}
Uma segunda rodada, em 31/10/2026, mediu somente BFS/DFS na biblioteca nova: CSR nos 12 grafos e matriz não ponderada nos Grafos 1 a 4. Foram 2.150 travessias, em 17 min 27 s de carga e execução, com a mesma semente 2025 e origens distintas. As contagens foram ajustadas ao custo; não devem ser apresentadas como 100 em todos os casos. As matrizes dos Grafos 3 e 4, agora com bitmask, ocupam aproximadamente 16,37 GiB cada; as dos Grafos 5 e 6 exigiriam cerca de 2.731 GiB cada e foram excluídas.
"""
    bridge=[]
    for i in range(1,7):
        name=f'grafo_{i}';b=by_search[(name,'csr','bfs')];d=by_search[(name,'csr','dfs')]
        if i<=4:
            k=paired[(name,'matrix','bfs_micros')]['samples']
            ratios=[num(float(paired[(name,'matrix',a+'_micros')]['value_average'])/float(paired[(name,'csr',a+'_micros')]['value_average']),4)+r'$\times$' for a in ('bfs','dfs')]
        else:k='--';ratios=['--','--']
        bridge.append([str(i),b['samples'],num(float(b['mean_micros'])/1e6),num(float(d['mean_micros'])/1e6),k,*ratios])
    maintex+=table(['Grafo',r'$k$ CSR','BFS CSR (s)','DFS CSR (s)',r'$k$ par','Matriz/CSR BFS','Matriz/CSR DFS'],bridge,columns='rrrrrrr',caption='Médias CSR completas; razões restritas ao prefixo comum de origens.',size=r'\fontsize{9}{11}\selectfont')+r"""
Em CSR, a busca inicializa vetores de tamanho $N$ antes de percorrer a componente alcançada: o custo é $O(N+N_c+M_c)$, não apenas $O(N_c+M_c)$. Na matriz com bitmask, o termo de varredura é $O(N_c\lceil(N+1)/64\rceil+M_c)$, além da inicialização. A DFS iterativa pode empilhar um vértice várias vezes antes de marcá-lo; a BFS o marca ao enfileirar. Isso é compatível com BFS mais rápida nesta rodada, mas não prova uma ordenação universal entre os algoritmos.

Os grafos desconexos explicam parte relevante da variação. Nas dez origens matriciais, o Grafo 4 alcançou em média 150.000 vértices, contra 212.500 no Grafo 3; ser mais rápido, apesar de mais denso, não é evidência isolada de erro. Comparamos CSR e matriz somente nas mesmas origens. As 2.150 contagens de alcançados conferem com os tamanhos exatos das componentes. Essa rodada não deve ser misturada diretamente com os tempos históricos de Dijkstra.
\section{Validação e conclusão}
Os 178 casos C++ passaram; a execução Linux utilizou sanitizadores. Os testes diferenciais comparam distâncias das versões de Dijkstra e, com pesos unitários, níveis de BFS. Onze grafos artificiais, com resultados calculados à mão, cobrem pesos zero e negativos, inalcançabilidade, empates, reduções de chave, paralelas e diferenças entre profundidade e distância. Total: 160 resultados por vértice/algoritmo conferidos.

CSR permitiu analisar os maiores grafos; a bitmask ampliou os casos matriciais viáveis da Parte 1. As tabelas desta versão contêm todos os 30 pares solicitados para o conjunto analisado, inclusive os caminhos completos, e os tempos disponíveis. \textbf{Permanece pendente a medição por vetor nos Grafos Ponderados 4 a 7}, necessária para completar integralmente o estudo de tempos do enunciado. O material complementar reúne graus, componentes, pesos, figuras, dispersão dos tempos, casos artificiais e rastreabilidade, sem ocupar o limite deste relatório.
\end{document}
"""
    maintex=re.sub(r'\\cod\{([^{}]*/[^{}]*)\}', lambda m: r'\path{'+m[1]+'}', maintex)
    (docs/'relatorio_parte2.tex').write_text(maintex)

    supp=PREAMBLE.replace('a4paper,11pt','a4paper,12pt').replace(r'\setstretch{1.07}',r'\setstretch{1.15}').replace('Trabalho 2 - Teoria dos Grafos','Trabalho 2 - Material complementar')+r"""\begin{document}
\maketitle
\begin{abstract}
Material de apoio às análises das Partes 1 e 2. Mantém separados resultados exatos, limites inferiores, projeções de memória e medições de duas rodadas distintas no Azure. O relatório de cinco páginas está em \cod{docs/relatorio_parte2.tex}; este complemento não é parte desse limite e não resolve a ausência de tempos de Dijkstra por vetor nos maiores grafos.
\end{abstract}
\setcounter{tocdepth}{1}\tableofcontents
\clearpage
\section{Escopo, arquivos e critérios de cobertura}
O conjunto ponderado é o já medido: Grafos Ponderados 3, 4, 5, 6 e 7, mais a rede de colaboração. Os Grafos Ponderados 1 e 2 de um acervo de 2025/2 não foram incorporados a esta rodada. O enunciado consultado está identificado como COS242, 2025/2; a data deste material e dos experimentos é outubro de 2026. As exigências do documento são tratadas como critérios do relatório, não como autorização para novas execuções de custo elevado.
"""+table(['Exigência da Parte 2','Situação neste material'],[
['Representação não direcionada com pesos reais','Implementada em CSR e matriz'],['Dijkstra por vetor e por heap atualizável','Ambos implementados e testados'],['Aviso para pesos negativos','Rejeição nas rotinas de Dijkstra'],['Distâncias e caminhos 10 até 20/30/40/50/60','25 pares completos no relatório'],['Cinco destinos da rede, com grafia exata','5 pares completos no relatório'],['Tempos médios das duas versões por grafo','Vetor medido no 3; pendente nos 4 a 7'],['URL do código e limite de cinco páginas','No relatório principal']],columns=r'>{\raggedright\arraybackslash}p{8.3cm}>{\raggedright\arraybackslash}p{7cm}',size=r'\small')+r"""
Não se substituem tempos ausentes por números dos colegas, extrapolações quadráticas ou tempos da versão com entradas obsoletas. A fonte pública do código é \url{https://github.com/Cerne17/graph-library-cpp}. Os dados brutos da nova rodada estão em \cod{data/azure_traversals/}; tabelas compactas ficam em \cod{docs/results/traversals/}.
\section{Arquitetura e decisões de implementação}
"""+ARCHITECTURE+r"""
\subsection{Contratos, políticas e armazenamento}
A base legada permanece separada em \cod{include/graph.hpp}, \cod{src/} e nos aplicativos sem sufixo ponderado. A biblioteca nova ocupa \cod{include/graph/}. \cod{graph_like} exige contagem de vértices e visita de vizinhos; \cod{weighted_graph} acrescenta tipo de peso, visita de arestas e detecção de pesos negativos. São restrições estáticas sobre tipos, não classes abstratas. Os aliases de grafo não acrescentam uma camada de objetos sobre os armazenamentos.

Em CSR, o intervalo de vizinhos de $u$ é delimitado por \cod{offsets[u]} e \cod{offsets[u+1]}. As duas orientações de cada aresta são armazenadas nos grafos não direcionados. Destinos e pesos são ordenados juntos. No caso não ponderado, a coluna de pesos é vazia, selecionada em compilação. Arestas paralelas permanecem em CSR; a matriz booleana deduplica adjacências e a matriz ponderada guarda o menor peso de cada par.

A bitmask utiliza $(N+1)\lceil(N+1)/64\rceil$ palavras, cada uma de 8 bytes, com uma linha e coluna zero não usadas. Não se aplica essa mesma economia às células da matriz ponderada: cada célula guarda um número real. Os IDs são de 32 bits, os índices de arestas são de 64 bits e os vértices vão de 1 a $N$.
\subsection{Leitura de entradas compactadas}
Os \emph{harnesses} Python aceitam arquivos \cod{.txt.gz}, validam e descompactam em arquivo temporário, depois fornecem o texto ao programa C++. Isso não é leitura direta de gzip pela biblioteca C++ nem execução de algoritmos sobre conteúdo comprimido. A leitura, a construção e a descompactação ficam fora do intervalo interno de tempo dos algoritmos. O diretório temporário é removido ao terminar. Hashes do texto descompactado permitem verificar que diferentes etapas usaram o mesmo grafo.

O leitor de arestas valida os IDs e campos, aceita comentários e registra os laços descartados. Assim, sua contagem de arestas pode diferir da Parte 1 entregue, que contava laços. A interpretação de pesos negativos refere-se ao grafo após a política de leitura; descartar um laço negativo não equivale a resolver caminhos mínimos no arquivo original. O coletor adicional detecta esse caso; as entradas reais analisadas não contêm pesos negativos.
\subsection{Heap indexado, vetor e entradas obsoletas}
O heap mantém \cod{heap_}, \cod{key_} e \cod{pos_}. Cada troca atualiza as posições dos dois vértices. Inserção, remoção e atualização de chave custam $O(\log N)$; consulta de presença custa $O(1)$. O indexado mantém no máximo uma entrada ativa por vértice. Os vetores de chave e posição são dimensionados para todos os vértices, mesmo se a componente alcançada for pequena.

A versão por vetor varre as estimativas dos não finalizados a cada seleção e para quando não resta distância finita. Com componente alcançada de $N_c$ vértices e $M_c$ arestas, o custo é $O(N+NN_c+M_c)$ em CSR. A implementação com \cod{std::priority_queue} insere uma nova entrada a cada melhoria e descarta uma entrada quando sua distância ficou obsoleta; pode usar espaço proporcional às relaxações bem-sucedidas. O heap próprio oferece atualização de chave e cumpre a exigência específica do enunciado; a terceira versão não deve ser confundida com ele.
\subsection{Caminhos, empates e pesos zero}
A distância inicial é infinita, exceto na origem; o vetor de pais começa em zero. Uma melhoria estrita atualiza distância e pai. Isso evita substituir pais apenas por empate e trata ciclos de peso zero. Inalcançável tem distância infinita e caminho vazio; origem igual ao destino tem caminho unitário. O mapa de nomes usa as grafias exatas e, quando há repetição de nome, resolve para o primeiro ID; esse critério deve ser lembrado ao interpretar nomes ambíguos.
\clearpage
\section{Estrutura e memória dos grafos ponderados}
"""+table(['Grafo',r'$N$',r'$M$',r'$\delta$ (\%)','Comp.','CSR (MiB)','Matriz (GiB)'],structure,columns='lrrrrrr',size=r'\fontsize{8.4}{10.5}\selectfont')+r"""
A densidade considera os pares distintos; a contagem $M$ preserva as paralelas válidas. O RSS de CSR desta tabela pertence à rodada histórica de Dijkstra; a estimativa matricial representa apenas as células. A diferença entre valor estimado e medido não deve ser atribuída apenas a estruturas matemáticas: o RSS inclui efeitos do alocador, páginas residentes e a medição de uma linha de base.
"""
    components=[];degrees=[];weights=[]
    for name in weighted:
        s=summaries[name]
        components.append([title(name),num(s['component_count'],12),num(s['component_largest'],12),num(s['component_smallest'],12),num(s['isolated_vertices'],12)])
        degrees.append([title(name),num(s['degree_min']),num(s['degree_median']),num(s['degree_mean']),num(s['degree_max'])])
        weights.append([title(name),num(s['weight_min']),num(s['weight_mean']),num(s['weight_max']),num(s['zero_weight_edges'],12),num(s['parallel_edges'],12)])
    supp+=table(['Grafo','Componentes','Maior','Menor','Isolados'],components,caption='Estrutura exata das componentes; conectividade desconsidera os pesos.',size=r'\fontsize{9}{11}\selectfont')
    supp+=table(['Grafo','Grau mínimo','Mediana','Média','Máximo'],degrees,caption='Graus em CSR, incluindo a multiplicidade das arestas paralelas.',size=r'\fontsize{9}{11}\selectfont')
    supp+=table(['Grafo','Peso mínimo','Média','Máximo','Pesos zero','Paralelas'],weights,caption='Propriedades das arestas ponderadas válidas.',size=r'\fontsize{8.8}{11}\selectfont')
    supp+=r"""
O Grafo Ponderado 6 possui três isolados; ``quase conexo'' não significa que todas as origens alcancem todos os vértices. Na rede de colaboração, as muitas componentes pequenas e os isolados contribuem para médias menores que as medianas quando parte das origens sorteadas não alcança a componente gigante. Não há uma regra geral segundo a qual assimetria sempre ``distorce negativamente'' a média: média e mediana resumem aspectos diferentes da distribuição.
\clearpage
\section{Dijkstra: resultados medidos e limites}
"""+table(['Grafo','Vetor','Heap indexado','Entradas obsoletas'],timings,columns='lrrr',caption='Média / mediana, em segundos, com 100 origens por algoritmo medido.',size=r'\fontsize{8.6}{11}\selectfont')+r"""
A rodada histórica usa o commit registrado em \cod{data/azure/environment.txt} (\cod{b414456}), e as distâncias das versões são comparadas fora do intervalo de tempo. Ela conserva médias e medianas agregadas, mas não preserva uma planilha completa de cada origem e tempo individual como a nova coleta. Não é possível reconstruir dela um desvio-padrão confiável; tal campo não foi inventado.

A versão por vetor no Grafo Ponderado 3 fornece uma comparação concreta entre seleção linear e heap. Nos demais grafos, a versão existe e passa nos testes, mas não foi medida. O crescimento quadrático explica a decisão prática de não executá-la; não transforma uma extrapolação em uma medição, nem satisfaz integralmente a tabela exigida pelo enunciado. Seria necessária uma rodada específica para completar essa lacuna.

O fato de a versão com entradas obsoletas superar a indexada nos maiores casos merece investigação por contadores de operações e memória. A implementação indexada acessa posições e chaves por vértice, enquanto a outra armazena pares na fila; localidade e volume de atualizações são hipóteses plausíveis. As tabelas demonstram o resultado observado, não o mecanismo causal.
"""
    def figure(path,caption,width=r'\textwidth'):
        return r'\begin{figure}[H]\centering\includegraphics[width='+width+']{'+path+r'}\caption{'+caption+r'}\end{figure}'+'\n'
    supp+=figure('parte2/azure_timings.png','Dijkstra no Azure: médias e medianas. Dados ausentes da versão por vetor não equivalem a tempo zero.')
    supp+=figure('parte2/azure_memory.png','Memória residente de CSR na rodada histórica. Não é pico de memória das buscas.')
    supp+=r"""\clearpage
\section{Busca e comparação com a Parte 1}
\subsection{Protocolo da nova rodada}
A nova rodada de 31/10/2026 foi executada sequencialmente em uma VM da mesma categoria, em \cod{belgiumcentral}, com 4 vCPUs e 32 GiB. Os fontes incluíam alterações locais, preservadas em um snapshot de 99 arquivos com hashes: o commit isolado não representa todo o código medido. São 32 execuções e 2.150 travessias, com semente 2025 e origens distintas. A duração total foi 17 min 27 s; a soma dos intervalos internos das buscas foi 11 min 44 s. Nenhum benchmark de Dijkstra foi repetido nessa rodada.

A quantidade de origens considera número de vértices, arestas, tamanhos das componentes e armazenamento. Para CSR, o trabalho esperado pesa componentes pela probabilidade de uma origem uniforme pertencer a cada uma: $E[N_c]=\sum_i n_i^2/N$. Para a bitmask, considera-se também a varredura de palavras por linha. O ajuste do plano foi feito antes da VM; não selecionou resultados rápidos depois de medi-los. Dez origens fornecem evidência exploratória menos precisa que cem.

BFS e DFS usam a mesma sequência por grafo. Quando CSR possui mais amostras, sua comparação com matriz restringe-se ao prefixo comum. A média CSR completa é publicada separadamente. As 2.150 contagens de alcançados foram conferidas contra os tamanhos exatos das componentes. Hashes verificaram identidade dos dados, do executável e dos artefatos. A VM, disco, IP, rede e permissão temporária foram removidos após o download validado.
\subsection{Tempos completos em CSR}
"""
    searchrows=[]
    for cfg in configs:
        name=cfg['graph'];b=by_search[(name,'csr','bfs')];d=by_search[(name,'csr','dfs')]
        searchrows.append([title(name),b['samples'],num(float(b['mean_micros'])/1e6),num(float(b['median_micros'])/1e6),num(float(d['mean_micros'])/1e6),num(float(d['median_micros'])/1e6)])
    supp+=table(['Grafo',r'$k$','BFS média','BFS mediana','DFS média','DFS mediana'],searchrows,caption='Tempos em segundos; pesos são ignorados por BFS e DFS.',size=r'\fontsize{8.7}{10.7}\selectfont')
    spread=[]
    for cfg in configs:
        name=cfg['graph']
        for alg in ('bfs','dfs'):
            r=by_search[(name,'csr',alg)]
            spread.append([title(name),alg.upper(),num(float(r['min_micros'])/1e6),num(float(r['max_micros'])/1e6),num(float(r['sample_stddev_micros'])/1e6),num(r['min_reachable'],12),num(r['max_reachable'],12)])
    supp+=r'\clearpage\subsection{Dispersão e alcançabilidade em CSR}'+'\n'+table(['Grafo','Busca','Mínimo (s)','Máximo (s)','Desvio (s)','Alc. mín.','Alc. máx.'],spread,size=r'\fontsize{8.3}{10.5}\selectfont')+r"""
O desvio-padrão é amostral. Os valores misturam variação da execução e diferença real de trabalho entre componentes; não são medidas de instabilidade da VM por si só. Na rede de colaboração há origens que alcançam somente a si próprias. Para comparar implementações, é essencial controlar a origem e os vértices efetivamente alcançados.
"""
    supp+=r'\clearpage\subsection{Matrizes com bitmask e comparações pareadas}'+'\n'
    matrixrows=[]
    for i in range(1,5):
        name=f'grafo_{i}'
        for alg in ('bfs','dfs'):
            r=by_search[(name,'matrix',alg)];cp=paired[(name,'csr',alg+'_micros')]
            matrixrows.append([title(name),alg.upper(),r['samples'],num(float(cp['value_average'])/1e6),num(float(r['mean_micros'])/1e6),num(float(r['median_micros'])/1e6),num(float(r['sample_stddev_micros'])/1e6)])
    supp+=table(['Grafo','Busca',r'$k$','CSR pareado','Matriz média','Mediana','Desvio'],matrixrows,caption='Segundos; CSR usa exatamente as origens da matriz.',size=r'\fontsize{8.8}{11}\selectfont')
    memory=[]
    for cfg in configs:
        name=cfg['graph'];s=summaries[name];r=rss[(name,'csr','memory_mb_graph')]
        mat=rss.get((name,'matrix','memory_mb_graph'))
        estimate=((s['vertices']+1)*((int(s['vertices'])+64)//64)*8/1024**3) if cfg['weighting']=='unweighted' else None
        memory.append([title(name),num(r['value_average']),num(mat['value_average']) if mat else '--',num(estimate,6) if estimate is not None else '--'])
    supp+=table(['Grafo','CSR medido (MiB)','Matriz medida (MiB)','Bitmask teórica (GiB)'],memory,caption='RSS antes das buscas, subtraída a linha de base; média das cargas BFS/DFS. Não é pico.',size=r'\fontsize{8.3}{10.8}\selectfont')
    supp+=r"""
Na Parte 1 entregue, a matriz em bytes limitava os casos viáveis aos grafos pequenos. A matriz atual com bitmask permite executar os Grafos 3 e 4 com cerca de 16,37 GiB. Matrizes ponderadas continuam precisando de células reais. Também é necessário reservar memória para a lista temporária de arestas e vetores dos algoritmos, não apenas para a representação final; o orçamento do processo foi limitado a 28 GiB.

O Grafo 4 matricial foi mais rápido que o Grafo 3 mesmo contendo mais arestas. As dez origens alcançaram em média 150.000 e 212.500 vértices, respectivamente. A quantidade de linhas varridas ajuda a explicar o resultado, sem assumir que densidade foi o único fator. A média CSR completa do Grafo 4 usa 100 origens e não pode substituir a média de suas dez origens pareadas.
"""
    supp+=figure('azure_traversals.png','BFS e DFS em CSR na nova rodada: médias e medianas, com contagens ajustadas ao custo.')
    supp+=figure('azure_traversals_matrix.png','Comparação pareada entre CSR e matriz com bitmask nos quatro grafos elegíveis.')
    supp+=figure('azure_traversals_memory.png','Memória das representações antes das buscas, medida em uma rodada distinta da de Dijkstra.')
    supp+=r"""\clearpage
\section{Compatibilidade e revisão das interpretações da Parte 1}
O relatório anterior é a referência de organização e de perguntas analíticas, não uma hipótese de que as representações permaneceram iguais. Três ajustes são necessários: (i) os laços agora são descartados e contados; (ii) o custo de inicializar vetores globais aparece mesmo em uma componente pequena; (iii) a bitmask muda constantes, memória e a forma de listar vizinhos.
"""
    oldcounts=[109977,1299794,765618,8187427,13168928,46469670]
    legacyrows=[]
    for i,old in enumerate(oldcounts,1):
        s=summaries[f'grafo_{i}'];legacyrows.append([str(i),num(old,12),num(s['edges'],12),num(s['skipped_self_loops'],12),num(s['parallel_edges'],12)])
    supp+=table(['Grafo','Arestas na Parte 1','Arestas válidas atuais','Laços descartados','Paralelas'],legacyrows,caption='Reconciliação das contagens; os dados não foram silenciosamente substituídos.',size=r'\fontsize{9}{11}\selectfont')
    unweighted=[]
    for i in range(1,7):
        s=summaries[f'grafo_{i}'];unweighted.append([str(i),num(s['vertices'],12),num(s['component_count'],12),num(s['component_largest'],12),num(s['component_smallest'],12),num(s['degree_mean']),num(s['degree_median'])])
    supp+=table(['Grafo',r'$N$','Comp.','Maior','Menor','Grau médio','Mediana'],unweighted,size=r'\fontsize{9}{11}\selectfont')+r"""
A exploração em CSR custa $O(N+N_c+M_c)$, pois os vetores são dimensionados para todo o grafo. A DFS marca ao remover da pilha e pode manter cópias pendentes de um mesmo vértice; a BFS marca ao inserir na fila. A profundidade da DFS não é distância mínima. Na bitmask, listar vizinhos varre palavras de 64 bits e enumera os bits presentes, em vez de testar cada célula individualmente.

Dupla varredura fornece um limite inferior na maior componente, em arestas e ignorando pesos. O limite 4 no Grafo 1 é compatível com o diâmetro 5 informado anteriormente. No Grafo 5, os valores 11 (anterior) e 12 (atual) são aproximações: origens e escopo podem alterar o limite obtido. Maior densidade global não garante menor diâmetro em grafos distintos.
"""
    diameters=[]
    for cfg in configs:
        name=cfg['graph'];d=read_rows(ROOT/'data/collected'/name/'stats/diameter.csv')
        exact=next((r for r in d if r['method']=='exact' and r['status']=='ok'),None)
        lower=next((r for r in d if r['method']=='double_sweep' and r['status']=='lower_bound'),None)
        diameters.append([title(name),num(exact['value']) if exact else '--',num(lower['value']) if lower else '--'])
    supp+=table(['Grafo','Exato, máximo nas componentes','Limite inferior, maior componente'],diameters,size=r'\fontsize{8.8}{11}\selectfont')
    supp+=r'\clearpage\section{Distribuições e visualizações das redes}'+'\n'+r"""
As frequências de graus e tamanhos de componentes usam todos os dados exportados e são exibidas apenas por pontos, sem ligá-los. Histograma de pesos é uma agregação por intervalos de uma variável numérica real; não é a frequência exata de cada peso distinto. A função acumulada complementar do grau indica a fração de vértices com grau pelo menos igual ao valor do eixo horizontal.
"""
    for figure_index,(path,caption) in enumerate([
        ('parte2/degree_frequencies.png','Frequências exatas de grau nos grafos ponderados; cada marcador corresponde a um grau observado.'),
        ('parte2/degree_tails.png','Distribuições acumuladas complementares dos graus; dados discretos sem linhas de ligação.'),
        ('parte2/weight_distributions.png','Distribuições dos pesos em intervalos; a escala dos pesos muda entre grafos.'),
        ('parte2/component_sizes.png','Tamanhos e multiplicidades das componentes, representados por pontos.'),
        ('parte1/degree_frequencies.png','Frequências exatas de grau dos seis grafos não ponderados.'),
        ('parte1/search_depths.png','Comparação entre níveis de BFS e profundidades de DFS nos estudos de caso da Parte 1.')]):
        supp+=(r'\clearpage'+'\n' if figure_index else '')+figure(path,caption)
    supp+=r"""\clearpage
\subsection{Amostras de rede: significado e limites}
Cada figura mostra uma amostra distribuída e outra de vizinhança, limitadas a 1.000 vértices e 4.000 arestas exportadas. Os detalhes de seleção estão no CSV de metadados de cada amostra; o desenho usa semente 2025 e 45 iterações. As posições são ilustrativas e não codificam distância geográfica, distância ponderada ou importância causal. O peso da aresta não determina o comprimento visual.

A escala do vértice representa seu grau no grafo original, não apenas na amostra. Um vértice sem ligação desenhada pode ter vizinhos fora da seleção ou arestas omitidas pelo limite; somente a marcação de isolado indica grau zero no grafo original. As amostras não estimam número de componentes nem diâmetro do grafo inteiro. Os valores globais vêm das tabelas e distribuições exatas.
"""
    for name in weighted:
        supp+=figure('network_'+name+'.png','Estrutura amostrada de '+title(name)+'. Os dois painéis não representam o grafo inteiro.')
    supp+=r"""\clearpage
\section{Casos artificiais de referência}
Os valores esperados foram calculados manualmente e registrados em \cod{tests/data/baselines/manifest.json}. São casos de correção, não estimativas de desempenho para milhões de vértices. A origem é 1; vizinhos são visitados em ordem crescente. Os 11 casos geraram 160 resultados por vértice/algoritmo conferidos. O valor $-1$ indica inalcançabilidade nos níveis das buscas; $+\infty$ indica distância ponderada infinita. Para regenerar: \cod{make baselines}.
"""
    baselines=json.loads((ROOT/'tests/data/baselines/manifest.json').read_text())
    for case in baselines:
        supp+=r'\par\noindent\begin{minipage}{\textwidth}\subsection{'+esc(case['title'])+'}\n'+esc(case['note'])+'\n'
        def vector(values):return ', '.join(r'$+\infty$' if v is None else num(v) for v in values)
        rows=[['BFS: distâncias',vector(case['bfs'])],['DFS: profundidades',vector(case['dfs'])]]
        if case['dist'] is not None:rows.append(['Dijkstra: três versões',vector(case['dist'])])
        elif case['negative']:rows.append(['Dijkstra: três versões','Rejeição esperada por peso negativo'])
        supp+=table(['Algoritmo','Valores por vértice, a partir de 1'],rows,columns=r'l>{\raggedright\arraybackslash}p{10.4cm}',size=r'\small')+r'\end{minipage}\par\medskip'+'\n'
    supp+=r"""\section{Rastreabilidade e reprodução}
O gerador \cod{analysis/build_report_part2.py} lê apenas resultados salvos, verifica os 30 pares e os resumos e produz os dois fontes LaTeX. Não executa Dijkstra, BFS, DFS nem provisiona recursos. Os arquivos \cod{data/azure/results_weighted.csv} e \cod{paths_weighted.csv} são da rodada histórica. Os resumos da nova rodada estão em \cod{docs/results/traversals/}, e os brutos, fontes e executável Linux, em \cod{data/azure_traversals/}. Este último diretório e os grandes dados de entrada não são versionados.

\cod{docs/figures/provenance.json} contém hashes das figuras, dados e scripts de visualização. \cod{docs/results/traversals/execution_record.json} registra execução e limpeza. As estatísticas estruturais foram calculadas separadamente das medições de tempo no Azure; não são medições de desempenho local apresentadas como nuvem. O ambiente do relatório deve ser analisado por rodada, pois código, caches e alocador podem mudar mesmo numa VM da mesma categoria.

Os fontes LaTeX usam PNGs da galeria no complemento; o relatório principal é independente de imagens externas. A compilação com Tectonic pode ser feita a partir da raiz do repositório. O diagrama Mermaid detalha a arquitetura atual; o diagrama anterior foi preservado em seu arquivo original. A biblioteca não foi alterada para produzir este material.
\end{document}
"""
    supp=re.sub(r'\\cod\{([^{}]*/[^{}]*)\}', lambda m: r'\path{'+m[1]+'}', supp)
    (docs/'material_complementar_parte2.tex').write_text(supp)
    manifest={'scope':weighted,'required_case_pairs':len(paths),'traversals':2150,'known_gap':'array timings only measured for grafo_W_3; no inferred measurements','sources':{}}
    for path in [ROOT/'data/azure/results_weighted.csv',ROOT/'data/azure/paths_weighted.csv',ROOT/'data/azure/environment.txt',docs/'results/traversals/traversal_summary.csv',docs/'results/traversals/results_traversals_paired.csv',docs/'figures/provenance.json',ROOT/'tests/data/baselines/manifest.json',Path(__file__)]:
        manifest['sources'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    (docs/'report_part2_provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Generated relatorio_parte2.tex and material_complementar_parte2.tex; 30 case pairs checked.')


if __name__=='__main__':main()
