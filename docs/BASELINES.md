# Casos artificiais de referência

Resultados esperados calculados manualmente em `tests/data/baselines/manifest.json`. Todos os grafos são não direcionados e a origem é o vértice 1. Vizinhos são visitados em ordem crescente. São casos de correção, não estimativas de desempenho em grafos grandes.

Regenerar: `make baselines`. `-1` indica vértice inalcançável nas buscas; `∞` indica distância ponderada infinita. A profundidade de DFS não é uma distância mínima.

## Vértice isolado

Origem igual ao destino; profundidade zero e nenhuma aresta.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0 | Correto |
| DFS — profundidades | 0 | Correto |

[Entrada do grafo](../tests/data/baselines/singleton.txt)

## Caminho linear

BFS e DFS coincidem; diâmetro de cinco arestas.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 2, 3, 4, 5 | Correto |
| DFS — profundidades | 0, 1, 2, 3, 4, 5 | Correto |

[Entrada do grafo](../tests/data/baselines/path.txt)

## Estrela

Alto grau central; todas as folhas têm distância um.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 1, 1, 1 | Correto |
| DFS — profundidades | 0, 1, 1, 1, 1, 1 | Correto |

[Entrada do grafo](../tests/data/baselines/star.txt)

## Grafo completo

BFS mantém distância um; DFS produz uma árvore profunda, com vizinhos em ordem crescente.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 1, 1 | Correto |
| DFS — profundidades | 0, 1, 2, 3, 4 | Correto |

[Entrada do grafo](../tests/data/baselines/clique.txt)

## Empate de caminhos e componente isolada

Duas rotas mínimas; o vértice 5 é inalcançável. Distância correta independe do desempate entre pais.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 2, -1 | Correto |
| DFS — profundidades | 0, 1, 3, 2, -1 | Correto |

[Entrada do grafo](../tests/data/baselines/diamond.txt)

## Menos arestas não significa menor peso

BFS chega a 2 com uma aresta de peso 10; Dijkstra prefere duas arestas de peso total 2.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, -1 | Correto |
| DFS — profundidades | 0, 1, 2, -1 | Correto |
| Dijkstra — vetor | 0, 2, 1, ∞ | Correto |
| Dijkstra — heap indexado | 0, 2, 1, ∞ | Correto |
| Dijkstra — heap tardio | 0, 2, 1, ∞ | Correto |

[Entrada do grafo](../tests/data/baselines/hops_vs_weight.txt)

## Ciclo de peso zero

O ciclo de peso zero deve terminar sem formar ciclos nos pais; o vértice 5 permanece inalcançável.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 2, -1 | Correto |
| DFS — profundidades | 0, 1, 2, 3, -1 | Correto |
| Dijkstra — vetor | 0, 0, 0, 2, ∞ | Correto |
| Dijkstra — heap indexado | 0, 0, 0, 2, ∞ | Correto |
| Dijkstra — heap tardio | 0, 0, 0, 2, ∞ | Correto |

[Entrada do grafo](../tests/data/baselines/zero_cycle.txt)

## Reduções sucessivas de chave

A estimativa para 2 diminui de 20 para 11 e depois 3; exercita update_key e entradas obsoletas da versão tardia.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 1, -1 | Correto |
| DFS — profundidades | 0, 1, 2, 3, -1 | Correto |
| Dijkstra — vetor | 0, 3, 2, 1, ∞ | Correto |
| Dijkstra — heap indexado | 0, 3, 2, 1, ∞ | Correto |
| Dijkstra — heap tardio | 0, 3, 2, 1, ∞ | Correto |

[Entrada do grafo](../tests/data/baselines/decrease_key.txt)

## Caminhos ponderados empatados

Empates de distância não exigem árvores de pais idênticas nas três versões.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 1, 2 | Correto |
| DFS — profundidades | 0, 1, 3, 2 | Correto |
| Dijkstra — vetor | 0, 1, 1, 2 | Correto |
| Dijkstra — heap indexado | 0, 1, 1, 2 | Correto |
| Dijkstra — heap tardio | 0, 1, 1, 2 | Correto |

[Entrada do grafo](../tests/data/baselines/equal_paths.txt)

## Arestas paralelas e laço

O leitor descarta o laço; CSR preserva as paralelas e Dijkstra encontra a de menor peso.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 2 | Correto |
| DFS — profundidades | 0, 1, 2 | Correto |
| Dijkstra — vetor | 0, 2, 3 | Correto |
| Dijkstra — heap indexado | 0, 2, 3 | Correto |
| Dijkstra — heap tardio | 0, 2, 3 | Correto |

[Entrada do grafo](../tests/data/baselines/parallel_loops.txt)

## Aresta negativa

BFS e DFS ignoram pesos; as três versões de Dijkstra devem rejeitar a entrada.

| Algoritmo | Valores por vértice, a partir de 1 | Resultado |
| --- | --- | --- |
| BFS — distâncias | 0, 1, 2 | Correto |
| DFS — profundidades | 0, 1, 2 | Correto |
| Dijkstra — vetor | Entrada rejeitada: peso negativo | Correto |
| Dijkstra — heap indexado | Entrada rejeitada: peso negativo | Correto |
| Dijkstra — heap tardio | Entrada rejeitada: peso negativo | Correto |

[Entrada do grafo](../tests/data/baselines/negative.txt)
