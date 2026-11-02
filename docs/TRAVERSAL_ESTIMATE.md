# Estimativa de duração da rodada de buscas

**Previsão antes de criar a VM; não são tempos medidos no Azure.** Piloto local de três origens por busca e representação viável, combinado às estatísticas completas e à rodada histórica. As matrizes grandes não foram alocadas no Mac.

Com 100 repetições em todas as combinações: **68.7–308.3 minutos** para carga e buscas. Plano ajustado: **15.7–61.2 minutos**, 2150 travessias. Esses valores não incluem provisionamento, compilação nem transferência.

Transferência de aproximadamente 2.54 GB: 3.4 min a 100 Mb/s ou 16.9 min a 20 Mb/s, sem contar sobrecarga. Reservar adicionalmente 5–15 min para preparar e testar a VM; é uma margem de planejamento.

## Repetições e tempos previstos

| Grafo | Armazenamento | Repetições por busca | BFS por origem (s) | DFS por origem (s) |
| --- | --- | ---: | ---: | ---: |
| Grafo 1 | CSR | 100 | 0.00052–0.00224 | 0.00105–0.00454 |
| Grafo 1 | Matriz com bitmask | 100 | 0.00411–0.0177 | 0.00466–0.0201 |
| Grafo 2 | CSR | 100 | 0.0019–0.00819 | 0.00616–0.0266 |
| Grafo 2 | Matriz com bitmask | 100 | 0.0189–0.0813 | 0.0215–0.0927 |
| Grafo 3 | CSR | 100 | 0.00611–0.0263 | 0.0115–0.0495 |
| Grafo 3 | Matriz com bitmask | 10 | 2.06–11.9 | 2.2–12.7 |
| Grafo 4 | CSR | 100 | 0.0481–0.207 | 0.0803–0.346 |
| Grafo 4 | Matriz com bitmask | 10 | 2.08–11.9 | 2.22–12.8 |
| Grafo 5 | CSR | 100 | 0.212–0.915 | 0.427–1.84 |
| Grafo 6 | CSR | 50 | 0.425–1.83 | 0.576–2.48 |
| Grafo Ponderado 3 | CSR | 100 | 0.0253–0.109 | 0.0519–0.224 |
| Grafo Ponderado 4 | CSR | 75 | 0.319–1.38 | 0.393–1.69 |
| Grafo Ponderado 5 | CSR | 10 | 1.34–5.77 | 2.61–11.2 |
| Grafo Ponderado 6 | CSR | 10 | 1.91–8.23 | 3.18–13.7 |
| Grafo Ponderado 7 | CSR | 10 | 4.53–19.5 | 12.5–53.8 |
| Rede de colaboração | CSR | 100 | 0.0355–0.153 | 0.0646–0.279 |

## Reproduzir o piloto e o plano

```bash
python3 analysis/collect.py --config config_traversals.csv --phases timing --algorithms bfs,dfs --ignore-config-counts --k 3 --memory-limit-gb 12 --timeout 600 --out build/traversal_pilot
python3 analysis/estimate_traversals.py
```

O limite de 12 GiB é o limite conservador usado neste Mac; ele omite as matrizes de aproximadamente 16 GiB. O limite planejado para a VM continua 28 GiB.

## Método e limites

O custo de CSR considera a inicialização de vetores para todos os vértices e a travessia de vértices e arcos da componente alcançada. Densidade entra pelo número efetivo de arestas, não como fator multiplicado novamente. Para a matriz com bitmask, a varredura percorre palavras de 64 bits por linha: o modelo inclui também inicialização e processamento dos vizinhos encontrados, mas não supõe um teste individual para cada uma das n² células.

Componentes são ponderadas pela probabilidade de uma origem uniforme pertencer a cada uma: E[V alcançados] = Σ tamanho²/n. A quantidade esperada de arcos usa os graus de cada componente. Isso importa especialmente nos grafos desconexos da Parte 1 e na rede de colaboração.

O plano usa até 5 minutos previstos no cenário superior para BFS+DFS de cada grafo/representação, com 10 a 100 repetições. Matrizes nunca recebem mais repetições que CSR. Esse orçamento não é um limite rígido de execução; o mínimo de dez pode excedê-lo.

As diferenças históricas entre Azure e Mac vêm de Dijkstra em dois grafos e não são uma calibração direta das buscas. CPU, caches, largura de banda de RAM, compilador e carga podem alterar a duração real. Por isso, são mostradas faixas amplas; não são intervalos de confiança. A extrapolação matricial recebe margem adicional.

BFS e DFS mantêm as mesmas origens por representação. Quando CSR recebe mais repetições, a comparação com matriz usa apenas o prefixo comum de origens; a média CSR completa é publicada separadamente. O número de amostras deve acompanhar toda média/mediana. Dez amostras oferecem evidência exploratória limitada, não a precisão de cem.

O plano executável está em `config_traversals.csv`; o detalhe numérico e hashes estão em `docs/traversal_forecast.json`. Os dados brutos do piloto local permanecem em `build/traversal_pilot/` e não entram nos resultados rotulados como Azure.
