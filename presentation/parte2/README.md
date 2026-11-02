# Apresentação — COS 242, Trabalho Parte 2

Deck interativo (um único `index.html`, sem bibliotecas externas além das fontes) sobre grafos ponderados, Dijkstra (vetor, heap indexado, entradas obsoletas) e os estudos de caso.

- 14 slides, orçamento de **13 min** (`data-min` em cada slide).
- Dados embutidos: `data/azure/paths_weighted.csv` (caminhos) + pesos de cada aresta extraídos dos arquivos de grafo; tempos e memória de `data/azure/results_weighted.csv`. A soma dos pesos de cada caminho foi conferida contra a distância exportada (30/30).
- `apresentacao_parte2.pdf`: um slide por página. `apresentacao_parte2_expandida.pdf`: cada demo vira várias páginas com estados-chave.

## Atalhos
← → espaço navegar · T cronômetro · R R zerar · N notas · F tela cheia · D tema · ? ajuda · `#n` na URL abre o slide n.

## Deploy (Vercel)
```bash
npm i -g vercel
vercel login
cd presentation/parte2
vercel --prod
vercel domains add grafos.cerne.pro   # CNAME para cname.vercel-dns.com se o DNS não estiver na Vercel
```
Refaça `vercel --prod` depois de qualquer alteração.
