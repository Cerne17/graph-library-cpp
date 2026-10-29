# Guia de entrada — COS242, Parte 2

Bem-vindo ao projeto. Este guia explica o que já existe, como montar o ambiente,
como trabalhamos e o que falta fazer. Leia até o fim antes de abrir o primeiro
PR: a base de código mudou bastante desde a Parte 1.

---

## 1. O projeto em uma página

**Parte 1 (entregue):** biblioteca de grafos não-direcionados e não-ponderados,
com lista e matriz de adjacência, BFS, DFS, componentes conexas, distâncias e
diâmetro, além de um benchmark e um *harness* em Python. O `README.md` descreve
essa versão e todos os resultados medidos.

**Parte 2 (em andamento):** o enunciado pede:

1. **Grafos com pesos** reais nas arestas. O arquivo de entrada ganha uma
   terceira coluna (`u v peso`).
2. **Distância e caminho mínimo** de um vértice para todos os outros, com a
   árvore de caminhos. Só com pesos não-negativos: com algum peso negativo, a
   biblioteca deve **avisar que não implementa** esse caso.
3. **Dijkstra de duas formas:** com um vetor de estimativas de distância, e com
   um **heap cujas chaves possam ser modificadas de forma eficiente**.
4. **Estudos de caso:**
   - para cada grafo com pesos: distância e caminho mínimo do vértice 10 até
     20, 30, 40, 50 e 60; tempo médio das duas versões de Dijkstra sobre k = 100
     vértices de origem aleatórios;
   - na rede de colaboração entre pesquisadores: distância e caminho mínimo de
     *Edsger W. Dijkstra* até *Alan M. Turing*, *J. B. Kruskal*,
     *Jon M. Kleinberg*, *Éva Tardos* e *Daniel R. Figueiredo*, usando
     exatamente essa grafia.
5. **Entregáveis:** relatório de no máximo 5 páginas (com a URL do código) e
   apresentação de no máximo 8 minutos.

---

## 2. Duas bases de código convivendo

O repositório tem **duas** bibliotecas no momento. Saber qual é qual evita
muita confusão:

| | Legado (Parte 1) | Nova (Parte 2) |
| --- | --- | --- |
| Onde | `include/graph.hpp`, `src/`, `app/`, `analysis/` | `include/graph/` |
| Estilo | classes com `virtual`, `camelCase` | templates, *concepts*, `snake_case` |
| Compilação | `.hpp` + `.cpp` | *header-only* |
| Testes | não tem | `tests/`, com doctest |
| Status | funciona, entregue; **não mexer** sem combinar | onde todo trabalho novo acontece |

A ideia é que a nova biblioteca substitua a antiga. Enquanto isso não acontece,
as duas compilam lado a lado.

### Por que reescrever?

Os três pilares do projeto são **arquitetura fundamentada em teoria**,
**desempenho** e **experiência de desenvolvimento**. A versão nova:

- troca polimorfismo em tempo de execução por **políticas em tempo de
  compilação** (direção, peso e armazenamento viram parâmetros de template);
- usa **concepts** do C++20 para que usar um algoritmo errado, como Dijkstra num
  grafo sem pesos, seja um **erro de compilação legível**, e não um bug;
- usa **CSR** (*Compressed Sparse Row*) em vez de `vector<vector<int>>`: dois
  vetores contíguos, sem cópia ao iterar vizinhos.

---

## 3. Mapa da biblioteca nova

```
include/graph/
├── types.hpp              vertex_id (uint32), edge_index (uint64), edge, weighted_edge<W>
├── policies.hpp           tags undirected/directed, unweighted/weighted<W>,
│                          e os concepts direction_policy / weight_policy
├── concepts.hpp           graph_like e weighted_graph: o que um algoritmo pode exigir
├── heap.hpp               indexed_heap<K, Compare>, com update_key em O(log n);
│                          indexed_min_heap<K> é o atalho para o min-heap
├── graph.hpp              csr_graph<D, W> e matrix_graph<D, W>: os nomes de uso
├── storage/               (namespace gl)
│   ├── csr.hpp            csr_storage<D, W>: armazenamento CSR, com ou sem pesos
│   ├── matrix.hpp         matrix_storage<D, W>: bitset, ou (n + 1)² pesos
│   ├── weight_column.hpp  coluna de pesos que só existe se o grafo for ponderado
│   └── input_edge.hpp     input_edge_t<W>: escolhe edge ou weighted_edge
├── io/                    (namespace gl::io)
│   ├── edge_list.hpp      read_edge_list<W>(path): lê arquivos de 2 ou 3 colunas
│   ├── load_graph.hpp     load_graph<G>(path): lê e monta o grafo
│   └── label_map.hpp      label_map e read_label_map: nome ↔ índice
└── algorithms/            (namespace gl::algorithms)
    ├── search.hpp         bfs, dfs → search_tree {source, parent, level}
    ├── components.hpp     connected_components (só não-direcionados)
    ├── distance.hpp       distance, exact_diameter, approximate_diameter
    ├── stats.hpp          compute_degree_stats → degree_stats
    └── dijkstra.hpp       em andamento (grupo)
```

### Os conceitos centrais, em uma frase cada

- **`graph_like<G>`**: G informa `vertex_count()` e percorre vizinhos com
  `for_each_neighbor(u, f)`. BFS, DFS e componentes exigem isto.
- **`weighted_graph<G>`**: além disso, G é ponderado e oferece
  `for_each_edge(u, f)`, que chama `f(destino, peso)`. Dijkstra exige isto.
- **`csr_storage<D, W>`**: construído só por `from_edges(n, arestas)`. Vizinhos
  saem ordenados. Grafos sem pesos não pagam nenhum byte pelos pesos.
- **`csr_graph<D, W>` / `matrix_graph<D, W>`** (`graph.hpp`): os nomes de uso
  das duas representações.
- **Leitura de arquivo:** `gl::io::load_graph<G>("arquivo.txt")` lê a lista de
  arestas e monta o grafo. `gl::io::read_label_map(...)` lê os nomes dos
  vértices (`name_of(v)`, `id_of(nome)`).

### Exemplo de uso de ponta a ponta

```cpp
#include "graph/graph.hpp"
#include "graph/io/load_graph.hpp"

using G = gl::csr_graph<gl::undirected, gl::weighted<>>;

const G g = gl::io::load_graph<G>("data/<grafo>.txt");

g.for_each_edge(10, [](gl::vertex_id v, double w) {
  // cada vizinho de 10 e o peso da aresta
});
```

---

## 4. Montando o ambiente

### Requisitos

- **Compilador com C++23**: Apple clang recente (Xcode / Command Line Tools
  atuais) ou GCC 13+. O código usa `std::views::zip`.
- `make`.
- **Python 3** com `pandas` e `psutil`, só se for rodar o *harness* de medição.

### Passos

```bash
git clone https://github.com/Cerne17/graph-library-cpp.git library
cd library
git switch feature/generalization   # a Parte 2 vive nesta branch
make test                           # compila e roda todos os testes
```

`make test` deve terminar com `Status: SUCCESS!`. Se não terminar, pare e avise
antes de começar qualquer tarefa.

### Particularidades por sistema

- **macOS:** o AddressSanitizer do Apple clang trava na inicialização nesta
  versão do sistema. Por isso o `make test` usa só UBSan mais o *hardening* da
  libc++, que verifica índices de `std::vector`.
- **Linux:** a definição `_LIBCPP_HARDENING_MODE` não tem efeito com a libstdc++
  do GCC. O equivalente é `-D_GLIBCXX_ASSERTIONS`, e o ASan funciona
  normalmente. Se for desenvolver no Linux, rode também com
  `-fsanitize=address,undefined -D_GLIBCXX_ASSERTIONS`.
- **Grafos grandes** (`data/grafo_*.txt`, de 14 MB a 700 MB) **não** estão no
  repositório. Baixe do site da disciplina ou peça ao Miguel.

### Editor

O `compile_flags.txt` na raiz configura o clangd (`-Iinclude`, `-Ithird_party`,
`-std=c++23`). Abra o editor **na raiz do repositório** para os includes
resolverem.

---

## 5. Como trabalhamos

### Branches e PRs

- Crie sua branch a partir de `feature/generalization`, com um nome que diga o
  que é: `feature/label-map`, `feature/matrix-storage`.
- Abra o PR **para `feature/generalization`**, nunca direto para `main`.
- Um PR = uma tarefa. PRs pequenos são revisados mais rápido.
- Antes de pedir revisão: `make test` verde, sem *warnings* novos.

### Commits

Seguimos o padrão `tipo(escopo): descrição`, como no histórico:

```
feat(storage): weighted CSR with weights sorted alongside targets
refactor(heap): generalize to indexed_heap with a comparator
test(storage): for_each_edge, negative weights, zero-cost unweighted layout
fix: untrack test binary, correct Liar test, namespace types
```

### Testes são obrigatórios

- Todo header novo ganha um `tests/test_<nome>.cpp` que o inclui. **Um header
  que nenhum `.cpp` inclui nunca é compilado**, então erros nele passam
  despercebidos.
- Fatos de tempo de compilação: `static_assert` no escopo do arquivo, um fato
  por linha.
- Comportamento: `TEST_CASE` com `CHECK` / `REQUIRE`.
- Todo *concept* precisa de um teste **negativo**: um tipo que deve ser
  rejeitado por exatamente um motivo.
- **Veja o teste falhar antes de vê-lo passar.** Um teste que nunca falhou não
  provou nada.
- Para conferir valores calculados em ponto flutuante, use
  `doctest::Approx`. Igualdade exata só vale para valores copiados, nunca
  calculados.

### Convenções

Tudo está em [`docs/CONVENTIONS.md`](CONVENTIONS.md). O essencial:

- estilo da biblioteca padrão: `snake_case` para tipos, funções e *concepts*;
- `namespace gl` para tipos, *storage* e heap; `gl::algorithms` para os
  algoritmos; `gl::io` para os leitores. Feche com o nome completo
  (`} // namespace gl::io`);
- vértices são **1-indexados**, como nos arquivos de entrada;
- **erro do programador** (pré-condição violada) → `assert`;
  **erro de entrada** (arquivo ruim, peso negativo no Dijkstra) → exceção;
- *include what you use*: se o arquivo usa algo, inclui o header que o declara;
- classes: seção `private` primeiro, depois `public`.

---

## 6. Armadilhas que já custaram tempo

1. **Corpo de função membro de template só é compilado quando chamado.** Um
   bug num método que nenhum teste chama passa no `make test`. "Verde" só prova
   o que foi exercitado.
2. **Assinatura com tipo dependente quebra a instanciação.** Um membro com
   `requires W::is_weighted` cuja **assinatura** menciona
   `typename W::weight_type` quebra a versão sem pesos. Use retorno `auto` e
   construa o tipo no corpo.
3. **Ler número negativo direto num tipo sem sinal** pode dar a volta para um
   valor enorme. Leia num `long long`, valide, depois converta.
4. **Vetores paralelos precisam andar juntos.** Ordenar só `targets_` sem
   reordenar os pesos quebra a correspondência sem travar nada.
5. **`std::ranges::sort` não é estável.** Arestas paralelas com o mesmo destino
   podem sair em qualquer ordem: compare-as sem depender da ordem.
6. **`std::priority_queue` usa a convenção oposta à do nosso heap:** lá,
   `std::less` dá um *max*-heap; aqui, `std::less` dá um *min*-heap.

---

## 7. O que falta e como dividir

**Divisão atual:**

- **Grupo:** algoritmos da Parte 2 — tarefas E a H abaixo. Os arquivos já
  existem como esqueleto (declaração + contrato nos comentários) e os testes
  como casos marcados `doctest::skip()`: falta escrever os corpos.
- **Miguel:** fez a matriz de adjacência (tarefa C) e a migração da Parte 1
  (tarefa D). Não mexa em `storage/matrix.hpp` nem nos algoritmos migrados sem
  combinar.
- **Qualquer mudança em `concepts.hpp`** (por exemplo, exigir
  `has_negative_weight()` em `weighted_graph`) afeta todo mundo: combine antes.

Convenções já decididas para Dijkstra:

- resultado em `shortest_paths<W>`, com `dist` e `parent` 1-indexados;
- vértice inalcançável: `dist = +∞` e `parent = 0`;
- peso negativo: lança `std::invalid_argument`, verificado em O(1) via
  `has_negative_weight()`.

**Estado das tarefas.** A, C e D estão feitas; B está quase pronta; E a H estão
abertas para o grupo (veja abaixo):

### A. Mapeamento nome ↔ índice da rede de colaboração — feita

Em `io/label_map.hpp` (`gl::io::label_map`, `gl::io::read_label_map`), com
testes em `tests/test_label_map.cpp`. O plano original:


- Uma classe pequena e separada do grafo, por exemplo `gl::label_map`:
  `std::vector<std::string>` (índice → nome) e
  `std::unordered_map<std::string, vertex_id>` (nome → índice).
- Leitura do arquivo de nomes da rede (confira o formato no site da
  disciplina: nomes podem conter vírgulas, pontos e acentos).
- Nome desconhecido → exceção. Comparação **exata**, byte a byte, sem
  normalizar acentos (*Éva Tardos* tem que funcionar).
- **Pronto quando:** testes com um arquivo de nomes pequeno em `tests/data/`,
  incluindo nome com acento e nome inexistente.

### B. Testes do leitor de arquivos e comentários `#` — quase pronta

Já existem: comentários `#`, um teste por caso de erro (com a mensagem
conferida), o teste da mensagem com número de linha e o de ponta a ponta
(`tests/test_load_graph.cpp`). Os casos de erro usam arquivos temporários
escritos pelo próprio teste, não *fixtures*. **Falta:** a *fixture* com o
exemplo do enunciado (peso `-9.5`). Confirme com quem pegou a tarefa.

O plano original:

- `io/edge_list.hpp` funcionava, mas tinha só um teste.
- Escrever os testes de arquivos bons (2 e 3 colunas), um teste de ponta a
  ponta (ler → montar CSR → conferir arestas), e um teste por caso de erro: arquivo
  inexistente, arquivo vazio, n inválido, campo faltando, lixo no fim da linha,
  vértice fora do intervalo, vértice negativo, laço.
- Aceitar linhas de comentário que começam com `#` (após espaços opcionais).
- Usar o exemplo do enunciado (com o peso `-9.5`) como *fixture*.
- **Pronto quando:** cada caso de erro tem *fixture* e teste, e um teste
  confere a mensagem com o número da linha.

### C. Matriz de adjacência — feita

- `storage/matrix.hpp`, com a mesma interface pública do `csr_storage`.
- Sem pesos: **bitset plano** (`std::vector<std::uint64_t>`), com varredura de
  vizinhos palavra a palavra (`std::countr_zero`, `std::popcount`).
- Com pesos: `std::vector<W>` plano com as (n + 1)² células, `+∞` como "sem
  aresta"; arestas paralelas ficam com o menor peso. (O plano original guardava
  só o triângulo superior quando não-direcionado; ficou a matriz inteira.)
- **Pronto quando:** satisfaz `graph_like` / `weighted_graph`, e um teste
  diferencial mostra que CSR e matriz devolvem os mesmos vizinhos e pesos em
  grafos aleatórios.

### D. Migração da Parte 1 — feita

- BFS, DFS, componentes e diâmetro reescritos como templates sobre
  `graph_like`, em `include/graph/algorithms/`.
- **Pronto quando:** reproduzem os números da Parte 1 (`data/results.csv`; por
  exemplo, no grafo_1, o pai do vértice 10 na BFS a partir do 1 é 2042).
- Feita em `algorithms/search.hpp`, `components.hpp`, `distance.hpp` e
  `stats.hpp`. `tests/test_algorithms_vs_legacy.cpp` compara com o código da
  Parte 1 em grafos aleatórios, e os 18 pais da Q4 conferem nos grafos 1 a 4.
- **Atenção:** os grafos do enunciado têm laços (por exemplo, `grafo_1` linha
  5126), e `read_edge_list` / `from_edges` os rejeitam. Para conferir a Q4, os
  laços foram descartados (não mudam pais de BFS/DFS). Decidir o que fazer com
  eles antes de rodar os grafos grandes.

### E. Peças comuns de Dijkstra — `algorithms/shortest_paths.hpp`

Faça esta primeiro: as três versões usam estas peças.

- `shortest_paths<W>{source, dist, parent}` já está pronto.
- `detail::check_dijkstra_input(g, source)` → nada. `assert` se `source` fora de
  1..n; lança `std::invalid_argument` se `g.has_negative_weight()`.
- `detail::init_shortest_paths(g, source)` → `shortest_paths` com `dist = +∞`,
  `parent = 0`, `dist[source] = 0`, vetores com n + 1 posições.
- **Decisão antes de começar:** `check_dijkstra_input` chama
  `has_negative_weight()`, que o conceito `weighted_graph` ainda não exige.
  Proposta: acrescentar ao conceito (afeta `concepts.hpp`, combine).

### F. Três versões de Dijkstra

Uma por arquivo, para que cada pessoa trabalhe sem conflito. Todas começam
com `check_dijkstra_input` + `init_shortest_paths`.

1. `algorithms/dijkstra_array.hpp` — `dijkstra_array(g, source)`. Vetor de
   visitados; a cada rodada, varre todos os não visitados atrás do menor
   `dist`; para quando o menor é +∞. O(n² + m).
2. `algorithms/dijkstra_heap.hpp` — `dijkstra_heap(g, source)`. Usa
   `gl::indexed_min_heap<W>` (`push`, `top`, `pop`, `contains`, `update_key`).
   Cada vértice entra no heap uma única vez. O((n + m) log n).
3. `algorithms/dijkstra_lazy.hpp` — `dijkstra_lazy(g, source)`. Usa
   `std::priority_queue` com `std::greater`; uma relaxação empurra uma entrada
   nova; ao tirar uma entrada com `d > dist[u]`, ignora (está velha).
   O((n + m) log m). Serve de linha de base para o relatório.

`algorithms/dijkstra.hpp` só inclui as três.

### G. Caminho mínimo — `algorithms/path.hpp`

- `reconstruct_path(tree, target)` → `std::vector<vertex_id>` com o caminho da
  origem até `target`, origem primeiro. `{source}` se `target == source`;
  vazio se inalcançável (`parent[target] == 0` e `target != source`).
- Funciona com `search_tree` (BFS/DFS) e `shortest_paths` (Dijkstra), via o
  conceito `parent_tree` que já está no arquivo.
- Dica: siga `parent` de `target` até a origem e inverta no fim
  (`std::ranges::reverse`).

### H. Testes

Os casos já existem com `doctest::skip()`. Tire o `skip` quando o caso estiver
escrito:

- `tests/test_dijkstra.cpp` — comportamento comum às três versões (vértice
  único, grafo pequeno conferido à mão, inalcançáveis, peso zero, peso negativo
  lança, CSR = matriz, `float`). O topo do arquivo mostra como rodar cada caso
  para as três versões com `TEST_CASE_TEMPLATE`.
- `tests/test_dijkstra_differential.cpp` — grafos aleatórios com semente fixa:
  as três versões concordam em `dist`; `parent` forma uma árvore de caminhos
  mínimos; com todos os pesos 1, `dist` = nível da BFS.
- `tests/test_path.cpp` — `reconstruct_path` em árvores montadas à mão.

**Pronto quando:** nenhum caso pulado, `make test` passando no Linux (com
AddressSanitizer) e no Mac.

---

## 8. Usando agente de código

O repositório tem um `AGENTS.md` na raiz com as instruções para agentes de
código (Claude Code, Codex, Cursor e similares). O `CLAUDE.md` apenas importa o
`AGENTS.md`. Mantenha os dois atualizados se mudar alguma convenção.

Revise tudo o que o agente produzir antes do PR: os testes têm que fazer sentido
para você, não só passar.

---

## 9. Para ler primeiro

1. Este guia.
2. `docs/CONVENTIONS.md`.
3. `include/graph/concepts.hpp` e `include/graph/storage/csr.hpp`.
4. `tests/test_csr.cpp`: os testes mostram como tudo é usado.

Dúvidas: fale com o Miguel antes de decidir algo de arquitetura.
