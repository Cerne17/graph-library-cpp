# Material da Parte 2 em LaTeX

A organização segue o relatório de referência: introdução, decisões de implementação, tabelas e análise contextualizada por tamanho, memória e componentes. O conjunto é o já medido: Grafos Ponderados 3 a 7 e rede de colaboração.

- [Relatório principal em LaTeX](relatorio_parte2.tex): cinco páginas, 25 pares numéricos e cinco pares da rede, com caminhos completos, distâncias e tabelas de tempo.
- [Material complementar em LaTeX](material_complementar_parte2.tex): 24 páginas com figuras, estatísticas, dispersão das travessias, interpretação da Parte 1 e 11 casos artificiais.
- [Arquitetura atual, com Mermaid](ARQUITETURA.md); o diagrama antigo foi preservado.
- [PDF principal](../output/pdf/relatorio_parte2.pdf) e [PDF complementar](../output/pdf/material_complementar_parte2.pdf).
- [Pacote LaTeX portátil](../output/pdf/latex_parte2.zip), com os dois fontes, figuras utilizadas e diagrama.

## Lacuna dos dados exigidos

O enunciado pede tempos médios de Dijkstra por vetor e por heap para os grafos ponderados. A versão por vetor foi medida apenas no Grafo Ponderado 3; faltam medições nos Grafos Ponderados 4 a 7. Os arquivos deixam essa pendência explícita. A versão com entradas obsoletas é uma comparação adicional e não substitui o heap atualizável nem os tempos ausentes por vetor. Os tempos de Dijkstra na rede são adicionais ao estudo de caminhos exigido para ela.

Assim, o relatório atende ao limite de páginas e expõe os resultados disponíveis, mas não deve ser apresentado como cobertura experimental integral enquanto essa lacuna permanecer. Nenhuma estimativa ou resultado de colegas foi apresentado como uma medição própria.

## Gerar os fontes e compilar

Da raiz do repositório, com os resultados e estatísticas locais presentes:

```bash
python3 analysis/build_report_part2.py
mkdir -p output/pdf
tectonic --outdir output/pdf docs/relatorio_parte2.tex
tectonic --outdir output/pdf docs/material_complementar_parte2.tex
```

O gerador usa apenas a biblioteca padrão do Python e valida os 30 pares solicitados, suas grafias e os resumos. Ele não executa algoritmos de grafos, não repete benchmarks nem provisiona recursos. O relatório principal não depende de figuras externas; o complemento usa PNGs existentes em `docs/figures/`. Os mesmos fontes podem ser compilados em um projeto LaTeX convencional. No pacote portátil, selecione qual `.tex` será o documento principal; as imagens ficam em `figures/`.

Ao alterar conteúdo manualmente, evite executar novamente o gerador, pois ele reescreve os dois fontes. Para alterações reprodutíveis, edite as seções correspondentes em `analysis/build_report_part2.py`. `docs/report_part2_provenance.json` registra hashes dos arquivos usados para gerar as tabelas.

## Distinções preservadas

As medições históricas de Dijkstra e a rodada de 2.150 travessias são identificadas separadamente. RSS é memória residente da representação, não pico do algoritmo. Projeções matriciais são estimativas; dupla varredura fornece limite inferior do diâmetro em arestas. As contagens atuais excluem laços, diferentemente do relatório entregue da Parte 1. Os custos de inicialização globais e a implementação da bitmask são considerados na interpretação atual.
