# graph-library-cpp

Biblioteca de grafos não-direcionados e não-ponderados em C++17, sem dependências
externas, construída em torno de uma única interface abstrata com duas
representações intercambiáveis — lista de adjacência e matriz de adjacência — de
modo que o custo da escolha de representação seja algo que se **mede**, e não que
se assume.

Escrita para COS232 (Teoria dos Grafos, UFRJ).

---

## Sobre este documento

O relatório em [`docs/implementacao.tex`](docs/implementacao.tex) descreve o
**projeto** da biblioteca, mas está sujeito a um limite de páginas. Por causa
desse limite, a maior parte dos **dados medidos** ficou de fora dele: o relatório
cita um único par de números de exemplo e as tabelas de complexidade teórica.

**Este README é o apêndice de dados completo do trabalho.** Tudo o que foi
cortado do relatório por limite de espaço está aqui, na íntegra:

| O que o relatório omitiu | Onde está aqui |
| --- | --- |
| Tempos de BFS e DFS dos 6 grafos, em ambas as representações, com média **e** mediana | [Resultado 1 — Tempo](#resultado-1--tempo-de-bfs-e-dfs) |
| Consumo de memória medido, e a projeção que justifica não medir a matriz nos grafos grandes | [Resultado 2 — Memória](#resultado-2--memória) |
| As 18 respostas de paternidade em árvores de BFS/DFS (Q4) | [Resultado 3 — Q4](#q4--pais-nas-árvores-de-busca) |
| As 18 distâncias entre pares (Q5) | [Resultado 3 — Q5](#q5--distâncias-entre-pares) |
| Componentes conexas por grafo (Q6) | [Resultado 3 — Q6](#q6--componentes-conexas) |
| Diâmetros e o porquê de o exato ter sido pulado em 5 dos 6 grafos (Q7) | [Resultado 3 — Q7](#q7--diâmetros) |
| A verificação de que lista e matriz produzem respostas idênticas | [Concordância](#lista-e-matriz-concordam-em-todas-as-respostas) |
| As análises quantitativas sobre os dados acima | seções de análise em cada resultado |

Os números vêm de [`data/results.csv`](data/results.csv) (tempo, memória,
estudo de caso) e de `data/stats_<nome>.csv` (tamanhos e graus).

---

## O que a biblioteca faz

- **Duas representações** atrás de uma interface `Graph`: `AdjacencyList` e `AdjacencyMatrix`.
- **Travessias**: BFS e DFS, ambas devolvendo uma árvore de busca com pais e níveis.
- **Componentes conexas**, calculadas em largura ou em profundidade.
- **Distâncias** entre pares de vértices.
- **Diâmetro**, exato (BFS de todos para todos) e aproximado (*double sweep*),
  com um orçamento de trabalho que recai na aproximação quando o cálculo exato
  fica caro demais.
- **Estatísticas de grau**: mínimo, máximo, médio e mediano.
- Um **driver de benchmark** que mede tempo e memória, e um *harness* em Python
  que o conduz sobre um lote de grafos.

Toda função pública em [`include/graph.hpp`](include/graph.hpp) carrega um
docstring com argumentos, valor de retorno e custo **nas duas representações**.

## Compilando

```bash
make                # app/graphs           build de debug, -g, sem otimização
make release        # app/graphs_release   otimizado, -O2
make benchmark      # app/benchmark        otimizado, -O2
make clean
```

Execute tudo a partir da raiz do repositório — os caminhos de saída são
relativos a ela.

## Executando

`app/graphs` escreve os CSVs de saída de um grafo, ou de um lote:

```bash
./app/graphs_release                        # sem argumentos: lê config.csv
./app/graphs_release data/graph.txt both    # um grafo: both | list | matrix
```

Sem argumentos ele percorre `config.csv`, uma linha `graph,representation` por
grafo, resolvendo `<nome>` para `data/<nome>.txt`. Um grafo que falha é
reportado e pulado, em vez de abortar a execução inteira.

`app/benchmark` produz as medições:

```bash
./app/benchmark data/graph.txt list time     # média e mediana de BFS e DFS, em µs
./app/benchmark data/graph.txt list mem      # segura o grafo para que o RSS seja amostrado
./app/benchmark data/graph.txt list report   # distâncias, componentes, diâmetros
./app/benchmark data/graph.txt list all      # tudo acima, um processo, uma carga
python analysis/harness.py                   # conduz o acima sobre config.csv
python analysis/harness.py retime            # refaz só os tempos, preservando o resto
```

O *harness* precisa de `pandas` e `psutil`.

## Formato de entrada

A primeira linha é a contagem de vértices; cada linha seguinte é uma aresta como
um par de identificadores.

```
5
1 2
2 5
5 3
4 5
1 5
```

Vértices são **indexados a partir de 1**. Todo vetor interno tem tamanho `n+1`
com o índice 0 sem uso, então um laço começando em 0 lê preenchimento.

## Arquivos de saída

Cada execução escreve quatro CSVs em `data/`, nomeados a partir do grafo:

| arquivo | uma linha por | colunas |
| --- | --- | --- |
| `stats_<nome>.csv` | representação | contagens, estatísticas de grau, componentes, diâmetros |
| `bfs_tree_<nome>.csv` | vértice | `node, parent, level` |
| `dfs_tree_<nome>.csv` | vértice | `node, parent, level` |
| `components_<nome>.csv` | vértice | `node, componentId` |

O *harness* agrega suas medições em [`data/results.csv`](data/results.csv), com
o esquema `graph, representation, metric, value_average, value_median`.

---

# Metodologia das medições

**Tempo.** Cada medição é a estatística de **100 execuções** de BFS e 100 de DFS.
Os 100 vértices de origem são sorteados **sem repetição** (`std::mt19937` semeado
por `std::random_device`) e a **mesma amostra** é reusada para BFS e DFS, de modo
que as duas colunas sejam comparáveis entre si. O cronômetro
(`std::chrono::steady_clock`) envolve apenas a chamada do algoritmo — a leitura
do arquivo e a construção do grafo ficam de fora. Quando um grafo tem menos de
100 vértices, a amostra é do tamanho do grafo.

Reportamos **média e mediana** das 100 execuções. A [análise de média vs.
mediana](#por-que-duas-colunas-média-e-mediana) mostra por que as duas são
necessárias: em grafos desconexos a distribuição dos tempos é bimodal, e a média
sozinha esconde isso.

**Memória.** O processo carrega o grafo, imprime `READY` e bloqueia; o *harness*
então amostra o RSS de fora, antes que qualquer algoritmo aloque estruturas
próprias. Do valor bruto subtrai-se um **baseline** — o RSS do mesmo binário
segurando um grafo trivial (`data/baseline.txt`, 1,375 MiB) — para isolar o custo
da estrutura de dados do custo do runtime de C++.

**Nota sobre precisão.** As medições foram feitas em um laptop, sem isolamento
de CPU, em build `-O2`. Os números servem para comparar **ordens de grandeza e
razões entre representações**, que é o objetivo do trabalho; não são benchmarks
de precisão absoluta.

---

# Os grafos medidos

Tamanhos e graus vêm de `data/stats_<nome>.csv`.

| grafo | n | m | grau mín. | grau máx. | grau médio | grau mediano | componentes | maior comp. | menor comp. |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `grafo_1` | 10.000 | 109.977 | 8 | 43 | 22,00 | 22 | 1 | 10.000 | 10.000 |
| `grafo_2` | 49.948 | 1.299.794 | 1 | 112 | 52,05 | 55 | 10 | 25.000 | 48 |
| `grafo_3` | 375.000 | 765.618 | 1 | 15 | 4,08 | 4 | 2 | 250.000 | 125.000 |
| `grafo_4` | 375.000 | 8.187.427 | 9 | 89 | 43,67 | 47 | 2 | 250.000 | 125.000 |
| `grafo_5` | 4.843.750 | 13.168.928 | 1 | 22 | 5,44 | 5 | 5 | 2.500.000 | 156.250 |
| `grafo_6` | 4.843.750 | 46.469.670 | 2 | 56 | 19,19 | 20 | 5 | 2.500.000 | 156.250 |

O conjunto é deliberadamente variado em dois eixos independentes:

- **Densidade**: `grafo_3` tem grau médio 4,08 e `grafo_2` tem 52,05 — mais de
  12× de diferença.
- **Conectividade**: `grafo_1` é conexo; os outros têm de 2 a 10 componentes,
  com a maior sempre em torno de 50–67% dos vértices.

Os pares (`grafo_3`, `grafo_4`) e (`grafo_5`, `grafo_6`) compartilham exatamente
o mesmo `n` e diferem apenas em `m` — o que permite isolar o efeito da densidade
sem mudar o tamanho.

---

# Resultado 1 — Tempo de BFS e DFS

Valores em **microssegundos**, por execução, média e mediana sobre 100 origens
sorteadas. Traço (—) indica medição não realizada; veja
[por que a matriz não foi medida](#por-que-a-matriz-não-foi-medida-nos-grafos-grandes).

| grafo | representação | BFS média | BFS mediana | DFS média | DFS mediana |
| --- | --- | ---: | ---: | ---: | ---: |
| `grafo_1` | lista | 628,31 | 571,50 | 1.027,64 | 999,50 |
| `grafo_1` | matriz | 45.399,60 | 44.307,00 | 45.137,70 | 44.518,50 |
| `grafo_2` | lista | 2.102,43 | 3.122,00 | 4.927,58 | 7.253,00 |
| `grafo_2` | matriz | 352.641,00 | 484.834,00 | 358.590,00 | 490.991,00 |
| `grafo_3` | lista | 15.870,80 | 15.874,00 | 19.304,80 | 21.174,00 |
| `grafo_4` | lista | 51.002,80 | 57.654,00 | 71.063,60 | 79.276,50 |
| `grafo_5` | lista | 294.705,00 | 254.050,00 | 444.015,00 | 403.271,00 |
| `grafo_6` | lista | 422.947,00 | 283.488,00 | 560.673,00 | 388.163,00 |

## Análise: lista vs. matriz

| grafo | BFS matriz/lista | DFS matriz/lista | razão assintótica `n²/(n+m)` | fator constante |
| --- | ---: | ---: | ---: | ---: |
| `grafo_1` | 72,3× | 43,9× | 833× | 11,5 |
| `grafo_2` | 167,7× | 72,8× | 1.848× | 11,0 |

A matriz é uma a duas ordens de grandeza mais lenta, e a desvantagem **cresce com
o número de vértices**: de 72× em `grafo_1` para 168× em `grafo_2`. Isso é o
esperado, porque o custo da matriz é `O(n²)` independentemente de `m`, enquanto o
da lista é `O(n + m)` — a razão entre os dois é `n²/(n+m)`.

Substituindo `m = n·d/2`, onde `d` é o grau médio, essa razão vira
`n / (1 + d/2)`: a penalidade da matriz é **linear em `n`** e apenas inversamente
proporcional à densidade. É por isso que `grafo_2` sofre mais apesar de ser o
grafo *mais denso* do par — seu `n` é 5,0× maior que o de `grafo_1`, enquanto seu
grau médio é só 2,4× maior. Dobrar o tamanho do grafo dói na matriz mais do que
adensá-lo ajuda.

O detalhe mais interessante é que a razão **medida** fica muito abaixo da
assintótica: 72× contra 833× previstos, e 168× contra 1.848×. O fator entre
previsão e medição é praticamente o mesmo nos dois grafos — **11,5 e 11,0**.
Essa constância tem uma explicação concreta: varrer uma linha contígua de bytes
da matriz é uma operação sequencial, amigável ao cache e vetorizável, enquanto
percorrer a lista de vizinhos envolve indireção de ponteiros e saltos de memória.
A análise assintótica conta inspeções de posição e trata as duas como iguais;
na prática, uma inspeção na matriz custa cerca de **11× menos** que uma na lista.

Ou seja: a matriz perde, e perde feio — mas perde cerca de 11× menos feio do que
a notação `O` sugere.

## Por que duas colunas: média e mediana

Em quase todos os grafos a média e a mediana **discordam**, às vezes por quase
metade do valor. Isso não é ruído de medição: é informação real sobre a forma da
distribuição dos tempos.

A coluna de diferença é `|média − mediana| / média`.

| grafo | componentes | grau médio | diferença BFS | diferença DFS |
| --- | ---: | ---: | ---: | ---: |
| `grafo_1` | 1 | 22,00 | 9,0% | 2,7% |
| `grafo_3` | 2 | 4,08 | 0,0% | 9,7% |
| `grafo_5` | 5 | 5,44 | 13,8% | 9,2% |
| `grafo_4` | 2 | 43,67 | 13,0% | 11,6% |
| `grafo_6` | 5 | 19,19 | 33,0% | 30,8% |
| `grafo_2` | 10 | 52,05 | 48,5% | 47,2% |

A causa é que **o custo de uma travessia não depende do tamanho do grafo, e sim
do tamanho da componente que contém a origem**. Em um grafo desconexo, sortear
100 origens produz uma amostra **bimodal**: as origens que caem na componente
gigante custam caro, as que caem nas componentes pequenas terminam quase
instantaneamente. A média mistura os dois grupos em um número que não descreve
nenhum deles; a mediana indica em qual dos grupos caiu a maioria da amostra.
Em `grafo_2`, onde a maior componente tem 25.000 dos 49.948 vértices — quase
exatamente metade — a amostra se divide ao meio e a diferença chega a 48%.

O contraste com `grafo_1` fecha o argumento: sendo **conexo**, toda origem custa
o mesmo, não há bimodalidade, e média e mediana convergem (9,0% e 2,7%, que é a
faixa de ruído das medições).

Fica a exceção aparente: `grafo_3` é desconexo (2 componentes de 250.000 e
125.000) e ainda assim média e mediana coincidem (0,0%). A explicação está no
código: `bfs` aloca e inicializa `visited`, `parent` e `level` com tamanho `n+1`,
e depois ainda **copia** `parent` e `level` para dentro da `SearchTree` de
retorno — cinco vetores de tamanho `n+1` por chamada. Esse custo é `Θ(n)`
**fixo**, pago independentemente de qual componente a origem alcança. Em um grafo
muito esparso como `grafo_3` (grau médio 4,08), esse custo fixo domina a parte
variável `Θ(n_c + m_c)` e achata a bimodalidade. É por isso que, entre os grafos
desconexos, a diferença cresce com a densidade: de 0,0% em `grafo_3` (o mais
esparso) a 48,5% em `grafo_2` (o mais denso).

**Conclusão prática**: reportar só a média esconderia tanto a bimodalidade quanto
o custo fixo de alocação. As duas colunas juntas revelam os dois efeitos.

## Análise: BFS vs. DFS

A DFS é consistentemente mais lenta que a BFS na lista — de 1,2× (`grafo_3`) a
2,3× (`grafo_2`) —, embora as duas sejam `O(n + m)`. A razão é estrutural e está
documentada em `include/graph.hpp`: a DFS iterativa empilha pares
(vértice, pai) e pode acumular **uma entrada por extremidade de aresta**, usando
espaço `O(n + m)`, enquanto a fila da BFS guarda no máximo `O(n)` vértices. Mais
memória movimentada por travessia significa mais pressão de cache.

Na matriz o efeito **desaparece** (`grafo_1`: 45.400 µs contra 45.138 µs, 0,6% de
diferença). Coerente: quando as duas pagam `O(n²)` só para enumerar vizinhos, o
custo da varredura domina tudo e a diferença entre pilha e fila vira ruído.

---

# Resultado 2 — Memória

Valores em **MiB**. `grafo isolado` é o RSS bruto menos o baseline de 1,375 MiB.

| grafo | representação | RSS bruto | baseline | grafo isolado | mínimo teórico | folga |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `grafo_1` | lista | 3,59 | 1,375 | 2,22 | 1,07 | 2,08× |
| `grafo_1` | matriz | 106,23 | 1,375 | 104,86 | 95,39 | 1,10× |
| `grafo_2` | lista | 25,55 | 1,375 | 24,17 | 11,05 | 2,19× |
| `grafo_2` | matriz | 2.723,83 | 1,375 | 2.722,45 | 2.379,32 | 1,14× |
| `grafo_3` | lista | 19,22 | 1,375 | 17,84 | 14,42 | 1,24× |
| `grafo_4` | lista | 130,94 | 1,375 | 129,56 | 71,03 | 1,82× |
| `grafo_5` | lista | 278,28 | 1,375 | 276,91 | 211,33 | 1,31× |
| `grafo_6` | lista | 745,80 | 1,375 | 744,42 | 465,43 | 1,60× |

O mínimo teórico é `(n+1)·24 + 2m·4` bytes para a lista (um `std::vector` de
24 bytes por vértice, mais dois `int` por aresta) e `(n+1)²` bytes para a matriz
(um `char` por par ordenado).

## Análise

**A lista escala com `m`; a matriz, com `n²`.** O par `grafo_3`/`grafo_4` isola
isso perfeitamente: mesmo `n` (375.000), mas `grafo_4` tem 10,7× mais arestas e
consome 7,3× mais memória (129,56 contra 17,84 MiB). A matriz, para esses dois
grafos, consumiria exatamente o mesmo — 131 GiB — porque não enxerga `m`.

**A folga da lista sobre o mínimo teórico chega a 2,2×**, e vem do crescimento
por duplicação do `std::vector`: uma linha que recebe 9 vizinhos aloca capacidade
para 16. A folga é maior justamente nos grafos de grau médio alto
(`grafo_2`: 2,19×, grau 52) e menor nos esparsos (`grafo_3`: 1,24×, grau 4),
o que é consistente com esse mecanismo.

**A matriz fica a 1,10–1,14× do mínimo** — bem mais previsível, já que aloca
`n+1` linhas de tamanho fixo e não tem capacidade sobrando. A folga residual é
arredondamento do alocador em `n+1` alocações separadas.

## Por que a matriz não foi medida nos grafos grandes

`config.csv` pede `both` apenas para `grafo_1` e `grafo_2`; os demais rodam só
em lista. Não é uma omissão de conveniência — é inviabilidade física:

| grafo | n | matriz exigiria | viável? |
| --- | ---: | ---: | --- |
| `grafo_1` | 10.000 | 95 MiB | sim (medido: 104,86 MiB) |
| `grafo_2` | 49.948 | 2,32 GiB | no limite (medido: 2,66 GiB) |
| `grafo_3` | 375.000 | 131 GiB | não |
| `grafo_4` | 375.000 | 131 GiB | não |
| `grafo_5` | 4.843.750 | 21.851 GiB (≈ 21,3 TiB) | não |
| `grafo_6` | 4.843.750 | 21.851 GiB (≈ 21,3 TiB) | não |

Para os dois maiores grafos, a matriz precisaria de mais de **21 terabytes** de
RAM — quatro ordens de grandeza acima de qualquer máquina de mesa. Esse é,
sozinho, o argumento mais forte do trabalho a favor da lista de adjacência: para
grafos grandes e esparsos a matriz não é "mais lenta", é **impossível**.

Essas linhas aparecem em `results.csv` com a métrica `skipped`, valor 1, para que
a ausência fique explícita no dado em vez de virar um silêncio.

---

# Resultado 3 — Estudo de caso (Q4–Q7)

Todos os valores desta seção foram produzidos pelo modo `report` do
`app/benchmark`. Onde as duas representações foram medidas, elas deram resultados
idênticos — veja [Concordância](#lista-e-matriz-concordam-em-todas-as-respostas).

## Q4 — Pais nas árvores de busca

Pai dos vértices 10, 20 e 30 nas árvores de BFS e de DFS enraizadas em 1, 2 e 3.

**Atenção à convenção**: `parent = 0` significa **raiz ou vértice inalcançável**,
sem distinção. Como os alvos (10, 20, 30) nunca são as raízes (1, 2, 3), todo `0`
nas tabelas abaixo significa **inalcançável** — cruze com as
[distâncias de Q5](#q5--distâncias-entre-pares), onde o mesmo caso aparece como `-1`.

### Árvore de BFS

| origem → alvo | `grafo_1` | `grafo_2` | `grafo_3` | `grafo_4` | `grafo_5` | `grafo_6` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 → 10 | 2.042 | 0 | 0 | 243.865 | 1.888.350 | 0 |
| 1 → 20 | 8.382 | 0 | 0 | 370.783 | 0 | 0 |
| 1 → 30 | 2.394 | 0 | 141.597 | 136.244 | 2.502.539 | 0 |
| 2 → 10 | 8.935 | 1.351 | 158.403 | 0 | 0 | 1.677.854 |
| 2 → 20 | 9.071 | 0 | 75.471 | 0 | 0 | 3.607.226 |
| 2 → 30 | 3.555 | 0 | 0 | 0 | 0 | 3.898.629 |
| 3 → 10 | 7.685 | 0 | 158.403 | 0 | 1.888.350 | 0 |
| 3 → 20 | 9.543 | 46.738 | 319.691 | 0 | 0 | 0 |
| 3 → 30 | 5.783 | 12.999 | 0 | 0 | 191.713 | 0 |

### Árvore de DFS

| origem → alvo | `grafo_1` | `grafo_2` | `grafo_3` | `grafo_4` | `grafo_5` | `grafo_6` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 → 10 | 709 | 0 | 0 | 12.269 | 1.888.350 | 0 |
| 1 → 20 | 666 | 0 | 0 | 10.738 | 0 | 0 |
| 1 → 30 | 86 | 0 | 141.597 | 1.531 | 191.713 | 0 |
| 2 → 10 | 709 | 3.946 | 192.218 | 0 | 0 | 381.031 |
| 2 → 20 | 666 | 0 | 141.526 | 0 | 0 | 431.008 |
| 2 → 30 | 86 | 0 | 0 | 0 | 0 | 446.011 |
| 3 → 10 | 709 | 0 | 106.718 | 0 | 1.888.350 | 0 |
| 3 → 20 | 666 | 217 | 141.526 | 0 | 0 | 0 |
| 3 → 30 | 86 | 3.513 | 0 | 0 | 2.502.539 | 0 |

### Análise

**Em `grafo_1`, a DFS dá o mesmo pai independentemente da raiz**: o vértice 10
tem pai 709 partindo de 1, de 2 ou de 3; o 20 tem pai 666 nos três casos; o 30,
pai 86. A BFS não faz nada parecido — o pai de 10 muda de 2.042 para 8.935 e
para 7.685 conforme a raiz.

O contraste expõe a diferença de natureza entre as duas árvores. A árvore de BFS
é uma árvore de caminhos mínimos *a partir da raiz*: mudar a raiz muda todas as
distâncias e, com elas, os pais. A DFS, num grafo conexo e denso como `grafo_1`
(grau médio 22), desce até o fundo antes de retroceder e acaba construindo
essencialmente a **mesma espinha dorsal profunda**, entrando nela em pontos
diferentes conforme a raiz. Vértices distantes dessa espinha acabam pendurados
sempre no mesmo lugar. A ordem de empilhamento fixa (descendente, garantida pelo
`finalize()` que ordena as linhas) é o que torna esse comportamento
reprodutível entre execuções.

**A quantidade de zeros mede a fragmentação.** `grafo_6` tem 12 zeros nas 18
respostas (BFS e DFS somadas): partindo dos vértices 1 e 3 nenhum dos três alvos
é alcançável, e só o vértice 2 compartilha componente com eles. Já `grafo_1`,
conexo, não tem nenhum zero. A tabela de pais,
lida assim, é um retrato indireto de como os rótulos de vértice se distribuem
entre as componentes.

## Q5 — Distâncias entre pares

Número de arestas no menor caminho; `-1` indica pares em componentes diferentes.

| par | `grafo_1` | `grafo_2` | `grafo_3` | `grafo_4` | `grafo_5` | `grafo_6` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| d(10, 20) | 3 | −1 | 9 | 4 | −1 | 5 |
| d(10, 30) | 3 | −1 | −1 | 3 | 9 | 5 |
| d(20, 30) | 4 | 3 | −1 | 4 | −1 | 5 |

### Análise

As distâncias finitas são **pequenas em termos absolutos** — de 3 a 9 arestas em
grafos de até 4,8 milhões de vértices —, o comportamento clássico de *mundo
pequeno*. E são governadas pela densidade, não pelo tamanho: `grafo_3`, o mais
esparso (grau 4,08), produz a maior distância medida (9), enquanto `grafo_4`, com
o mesmo `n` mas 10,7× mais arestas, fecha os mesmos pares em 3 ou 4 saltos.

Os `-1` são consistentes com os zeros de Q4, como deveriam ser: sempre que
`d(a,b) = -1`, os pais correspondentes também são 0.

## Q6 — Componentes conexas

| grafo | nº de componentes | maior | menor | maior / n |
| --- | ---: | ---: | ---: | ---: |
| `grafo_1` | 1 | 10.000 | 10.000 | 100,0% |
| `grafo_2` | 10 | 25.000 | 48 | 50,1% |
| `grafo_3` | 2 | 250.000 | 125.000 | 66,7% |
| `grafo_4` | 2 | 250.000 | 125.000 | 66,7% |
| `grafo_5` | 5 | 2.500.000 | 156.250 | 51,6% |
| `grafo_6` | 5 | 2.500.000 | 156.250 | 51,6% |

### Análise

Todos os grafos desconexos têm uma **componente gigante** que reúne metade ou
mais dos vértices. É essa estrutura que produz a bimodalidade analisada em
[média vs. mediana](#por-que-duas-colunas-média-e-mediana): com a gigante
ocupando ~50% dos vértices, sortear uma origem é quase um cara-ou-coroa entre
uma travessia cara e uma barata.

`grafo_2` é o caso extremo de desbalanceamento: a maior componente tem 25.000
vértices e a menor tem **48** — uma razão de 520×. As componentes pequenas custam
essencialmente nada para percorrer, o que explica a média de BFS ser 33% mais
baixa que a mediana nesse grafo.

Vale notar que os **identificadores** de componente não são estáveis entre
execuções (a ordenação por tamanho usa `std::sort`, que não é estável), mas as
contagens e os tamanhos são determinísticos.

## Q7 — Diâmetros

| grafo | `n·(n+m)` | orçamento (2×10⁹) | diâmetro exato | diâmetro aproximado |
| --- | ---: | ---: | ---: | ---: |
| `grafo_1` | 1,20×10⁹ | 0,6× — **dentro** | 5 | 4 |
| `grafo_2` | 6,74×10¹⁰ | 33,7× acima | pulado | 3 |
| `grafo_3` | 4,28×10¹¹ | 213,9× acima | pulado | 14 |
| `grafo_4` | 3,21×10¹² | 1.605× acima | pulado | 4 |
| `grafo_5` | 8,73×10¹³ | 43.625× acima | pulado | 11 |
| `grafo_6` | 2,49×10¹⁴ | 124.275× acima | pulado | 6 |

### Análise

**O diâmetro exato só foi calculado em `grafo_1`**, e por pouco: 1,20×10⁹ contra
um orçamento de 2×10⁹. Para os demais, `computeStats` devolve `-1` e o grafo é
descrito apenas pela aproximação. `grafo_6` está 124 mil vezes acima do
orçamento — o cálculo exato levaria dias.

O único ponto de comparação disponível mostra que **a aproximação subestima**:
em `grafo_1`, o *double sweep* devolve 4 contra o valor exato de 5. Isso é o
esperado e está documentado — o *double sweep* é um **limite inferior**, exato em
árvores e subestimado em grafos gerais. Nos grafos desconexos há ainda uma
segunda fonte de subestimação: a aproximação varre apenas a maior componente, de
modo que um diâmetro maior escondido numa componente menor passaria despercebido.

Na prática, então, os cinco valores aproximados da tabela devem ser lidos como
**pisos**, não como estimativas centradas. O contraste entre `grafo_3`
(aproximado 14, o mais esparso) e `grafo_4` (aproximado 4, mesmo `n`, 10,7× mais
denso) continua válido como comparação relativa: adicionar arestas encurta
drasticamente o grafo.

O orçamento é medido em `n·(n+m)` — o número de inspeções de aresta de uma
varredura BFS de todos para todos — e não simplesmente em `n`, justamente porque
`grafo_3` e `grafo_4` compartilham `n` e ainda assim diferem 7,5× em custo.

---

# Lista e matriz concordam em todas as respostas

Para os dois grafos medidos nas duas representações, foram comparadas **todas** as
métricas não-relacionadas a desempenho (pais de BFS e DFS, distâncias, contagem e
tamanhos de componentes, diâmetros):

| grafo | métricas comparadas | divergências |
| --- | ---: | ---: |
| `grafo_1` | 26 | **0** |
| `grafo_2` | 25 | **0** |

Esse resultado é a validação central do design. Os algoritmos são funções livres
que recebem `const Graph&` e nunca o tipo concreto; trocar a representação troca
apenas o custo de `neighbors()` e `degree()`, jamais a resposta. A concordância
em 51 de 51 métricas confirma que a abstração não vaza — o que, por sua vez, é o
que dá sentido a comparar tempos: as duas colunas estão medindo exatamente o
mesmo trabalho.

Para reproduzir a comparação:

```bash
python3 - <<'PY'
import pandas as pd
df = pd.read_csv('data/results.csv')
excl = ('bfs_micros','dfs_micros','memory_mb_raw','memory_mb_graph','memory_mb_baseline','skipped')
d = df[~df.metric.isin(excl)]
for g in ['grafo_1','grafo_2']:
    l = d[(d.graph==g)&(d.representation=='list')].set_index('metric').value_average
    m = d[(d.graph==g)&(d.representation=='matrix')].set_index('metric').value_average
    k = l.index.intersection(m.index)
    print(g, len(k), 'divergências:', [x for x in k if l[x] != m[x]])
PY
```

---

# Complexidade

`n` é a contagem de vértices, `m` a de arestas, `k` o número de componentes.

| operação | lista de adjacência | matriz de adjacência |
| --- | --- | --- |
| construção | O(n) | O(n²) |
| `addEdge` | O(1) amortizado | O(1) |
| `degree(u)` | O(1) | **O(n)** |
| `neighbors(u)` | O(deg u) | **O(n)** |
| `finalize` | O(m log n) | O(1) |
| `readGraph` | O(n + m log n) | O(n² + m) |
| `bfs` · `dfs` | O(n + m) | **O(n²)** |
| `getDistance` | O(n + m) | O(n²) |
| componentes | O(n + m + k log k) | O(n²) |
| `getApproximateDiameter` | O(n + m) | O(n²) |
| `getExactDiameter` | O(n·(n+m)) | **O(n³)** |
| memória | O(n + m) inteiros | O(n²) bytes |

A coluna inteira da matriz decorre de uma única linha: `neighbors()` varre uma
linha completa, independentemente de quantos vizinhos o vértice tem. Qualquer
coisa que seja O(n + m) contra a lista vira O(n²) contra a matriz. As medições
da [Análise lista vs. matriz](#análise-lista-vs-matriz) confirmam a direção, com
a ressalva importante de que o fator constante favorece a matriz em cerca de 11×.

---

# Convenções que vale conhecer

- **`-1` significa indefinido**, de forma consistente: o `level` de um vértice
  não alcançado, um `getDistance` entre componentes, um `exactDiameter` pulado.
- **O `level` de uma DFS é a profundidade na árvore de DFS**, não a distância no
  grafo. Para distâncias, use `bfs` ou `getDistance`.
- **`parent` vale 0 tanto para a raiz quanto para vértices inalcançáveis.** Só o
  `level` distingue os dois casos.
- **Nenhuma das representações deduplica arestas.** A lista guarda
  multiplicidade, então uma aresta repetida aumenta os graus e o lema do aperto
  de mãos vale contra `m`. A escrita da matriz é idempotente, então seus graus
  descrevem o grafo simples subjacente enquanto `m` continua contando o arquivo —
  as duas discordam em entradas que repetem arestas ou contêm laços.
- **`getApproximateDiameter` é um limite inferior.** O *double sweep* é exato em
  árvores e subestima em grafos gerais; além disso varre apenas a maior
  componente, então num grafo desconexo uma componente menor pode conter o
  diâmetro real.
- **Identificadores de componente não são estáveis entre execuções.** As
  componentes são ordenadas por tamanho com uma ordenação não estável, então
  componentes de mesmo tamanho podem trocar de posição.

---

# Organização do código

```
include/graph.hpp     toda a interface, com os docstrings
src/graph.cpp         AdjacencyList e AdjacencyMatrix
src/io.cpp            o parser de entrada e todos os escritores de CSV
src/search.cpp        bfs, dfs e os dois diâmetros
src/components.cpp    componentes conexas, em largura e em profundidade
src/distance.cpp      getDistance
src/stats.cpp         computeStats
app/main.cpp          CLI que produz os CSVs de saída
app/benchmark.cpp     modos de tempo, memória e relatório
analysis/harness.py   conduz o benchmark sobre config.csv
docs/implementacao.tex   relatório (limitado em páginas; os dados estão aqui)
docs/diagrama_classes.mmd  diagrama de classes em Mermaid
```

Comece por `include/graph.hpp`, depois `src/search.cpp`. O resto decorre desses
dois.
