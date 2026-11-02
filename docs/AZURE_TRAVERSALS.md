# Nova rodada de BFS e DFS no Azure

Plano executável: `python3 analysis/azure_traversals.py plan`.

- VM temporária `Standard_E4as_v4`: 4 vCPUs, 32 GiB de RAM, Ubuntu 24.04,
  disco de 64 GiB, região `belgiumcentral`, grupo exclusivo `cos242-traversals`.
- Acesso por SSH com IP público e uma chave pública já existente. O script
  não gera credenciais.
- Desalocação automática programada para até seis horas após a criação, por
  um temporizador persistente do systemd e identidade gerenciada com permissão
  de desligamento restrita à própria VM.
  Se a configuração desse desligamento falhar, o script tenta desalocar a VM
  imediatamente e retorna erro.
- Representação CSR nos 12 grafos: seis não ponderados, cinco ponderados e a rede de
  colaboração. Nos grafos não ponderados 1 a 4, executar também a matriz com bitmask;
  as matrizes dos grafos 5 e 6 excedem 28 GiB e são excluídas. BFS e DFS ignoram pesos e percorrem a topologia não direcionada.
- Origens aleatórias **distintas**, semente 2025. As quantidades variam por grafo
  e representação conforme `k_csr` e `k_matrix` em `config_traversals.csv`; consulte
  [a estimativa de duração](TRAVERSAL_ESTIMATE.md). BFS e DFS usam as mesmas origens.
  Quando os números de repetições diferem entre representações, a comparação
  usa o prefixo comum. Não repetir Dijkstra.

## Estado atual

Rodada concluída em **31/10/2026**, das 19h35min38s às 19h53min05s
(Brasília): **32 execuções, 2.150 travessias, todas válidas**. Duração de
**17 min 27 s**, incluindo preparação dos dados e cargas do coletor, excluindo
provisionamento, envio e compilação. A soma dos tempos internos das buscas é
**11 min 44 s**. A previsão de carga e buscas era de 15,7–61,2 minutos.

Os alcançados de todas as amostras correspondem aos tamanhos exatos das
componentes. Foram conferidos o snapshot de 99 fontes, o hash do executável em
todas as execuções, os hashes dos artefatos e as origens pareadas. Os 178 casos
de teste C++ passaram na VM com sanitizadores.

- [Resultados no formato da rodada anterior](results/traversals/results_traversals.csv).
- [Mínimo, máximo, desvio-padrão, média, mediana e amostras](results/traversals/traversal_summary.csv).
- [Comparação com origens pareadas](results/traversals/results_traversals_paired.csv).
- [Conferência dos alcançados](results/traversals/reachability_validation.json).
- [Galeria atualizada](VISUALIZATIONS.md).

Os dados brutos, fontes arquivados, executável Linux e metadados completos estão
em `data/azure_traversals/`, preservados localmente. Esses arquivos volumosos
não são versionados. As tabelas compactas acima ficam junto à documentação.

**Recursos Azure:** VM desalocada e permissão temporária removida após o download
validado. O grupo exclusivo foi excluído e sua ausência foi confirmada; VM,
disco, IP e recursos de rede foram removidos. Consulte o
[registro da execução e da limpeza](results/traversals/execution_record.json).
O agendamento nativo não é suportado nessa região;
foi usado um temporizador persistente com identidade gerenciada e permissão de
desalocação restrita à própria VM, verificado antes de iniciar o experimento.

## Pontos de atenção nos resultados

A matriz foi mais lenta que CSR em todos os pares medidos. A tabela usa médias
restritas às mesmas origens e à mesma quantidade de amostras:

| Grafo | Amostras por busca e representação | Matriz / CSR em BFS | Matriz / CSR em DFS |
| --- | ---: | ---: | ---: |
| Grafo 1 | 100 | 6,87× | 4,59× |
| Grafo 2 | 100 | 12,97× | 6,55× |
| Grafo 3 | 10 | 119,21× | 101,57× |
| Grafo 4 | 10 | 31,87× | 28,36× |

A alta variação em grafos desconexos acompanha a diferença entre as componentes
alcançadas; a conferência de todos os alcançados passou. Por exemplo, no Grafo 2,
as origens alcançam entre 781 e 25.000 vértices, e na rede de colaboração, entre
1 e 576.309. Isso deve acompanhar qualquer comparação de médias.

Na matriz, o Grafo 4 levou menos tempo que o Grafo 3 apesar de ter mais arestas.
As dez origens do Grafo 4 alcançaram em média 150.000 vértices, contra 212.500 no
Grafo 3. A redução das linhas visitadas ajuda a explicar a diferença: não se
deve atribuir esse resultado apenas à densidade. Mesmo com os mesmos IDs de
origem, grafos diferentes podem ter componentes diferentes para esses IDs.

Dez amostras nos casos mais caros servem à exploração; não oferecem a mesma
precisão de cem. Estes resultados não mostram divergência de correção nas
travessias verificadas. Não são uma prova de ausência de outros defeitos.

## Seleção das matrizes

`python3 analysis/prepare_traversals.py` gera `config_traversals.csv` e
`docs/traversal_memory_plan.json` a partir dos dados já verificados. Esse comando
reinicia os números em 100; em seguida, execute `python3 analysis/estimate_traversals.py`
para aplicar novamente o plano ajustado ao custo com os dados do piloto local. A fórmula
é `(n + 1) × ceil((n + 1) / 64) × 8` bytes, exatamente como a implementação,
incluindo o preenchimento de cada linha. O limite foi interpretado como 28 GiB;
a seleção seria a mesma usando 28 GB decimais.

Os grafos 1 e 2 exigem aproximadamente 0,012 e 0,291 GiB de matriz; os grafos
3 e 4, 16,373 GiB cada. Os grafos 5 e 6 exigiriam cerca de 2.731 GiB cada.
O orçamento total de construção e execução é limitado a 28 GiB, incluindo
arestas temporárias e estruturas de busca, para preservar folga na VM de 32 GiB.
O coletor mede RSS antes de iniciar as buscas.

## Executar após aprovação

Na raiz do repositório:

```bash
python3 analysis/azure_traversals.py provision
python3 analysis/azure_traversals.py upload
python3 analysis/azure_traversals.py start
python3 analysis/azure_traversals.py status
python3 analysis/azure_traversals.py fetch
```

O envio usa a cópia atual dos fontes, incluindo alterações ainda não commitadas,
com hashes registrados em `source_manifest.json`. O diretório remoto é exclusivo;
não há `git reset` nem reaproveitamento silencioso da rodada antiga. O envio não
pode sobrescrever um experimento já iniciado. O processo roda em `tmux` e grava
`traversal.exit`: `0` significa sucesso, qualquer outro valor indica falha.

O tempo inclui a chamada da busca e suas alocações, excluindo leitura,
descompactação, contagem posterior de alcançados e exportação. Como na rodada
anterior, a ordem das buscas e a carga da máquina podem influenciar os tempos.
Os dados brutos ficam em `data/collected_traversals/` na VM e, após o download,
em `data/azure_traversals/collected_traversals/` localmente.

## Estatísticas e validação

`results_traversals.csv` tem o mesmo formato da rodada anterior:
`graph,representation,metric,value_average,value_median,samples,status`.
Tempos são salvos em microssegundos; as figuras os convertem para segundos.
`traversal_summary.csv` acrescenta mínimo, máximo, desvio-padrão amostral,
alcançados, semente, host e hash do executável.

O exportador exige todas as amostras previstas para cada representação, origens distintas e válidas, mesmo conjunto
ordenado de origens e mesmos alcançados em BFS/DFS. Entre CSR e matriz, verifica
o prefixo comum e exporta também `results_traversals_paired.csv`, para comparação
com a mesma quantidade e as mesmas origens. As médias CSR completas ficam separadas. Também verifica hashes dos
artefatos e identidade dos dados. Não publica uma tabela parcial como completa. A validação adicional em
`validate_traversal_reachability.py` compara cada número de alcançados com o
tamanho exato da componente da origem, usando as estatísticas já coletadas.
O RSS da representação é medido antes da busca, com subtração de um processo
mínimo; não é o pico de memória do algoritmo. O resumo conserva medições de RSS
por busca e uma média/mediana das duas cargas para comparação da representação.

Depois de `fetch`, valide novamente e regenere a galeria:

```bash
python3 analysis/summarize_traversals.py \
  --collected data/azure_traversals/collected_traversals \
  --out data/azure_traversals --config config_traversals.csv
python3 analysis/validate_traversal_reachability.py
make figures
```

A galeria mantém os novos tempos de BFS/DFS separados dos tempos históricos de
Dijkstra. Para encerrar a cobrança de computação:

```bash
python3 analysis/azure_traversals.py deallocate
```

Disco e IP podem continuar cobrando após desalocação. Depois de conferir os
arquivos baixados, a remoção do grupo exclusivo encerra esses recursos também:

```bash
python3 - <<'PY'
import json, subprocess
from pathlib import Path
record = json.loads(Path("build/azure_traversals/guard_config.json").read_text())
subprocess.run(["az", "role", "assignment", "delete", "--ids", record["role_assignment_id"]], check=True)
PY
az group delete -n cos242-traversals --yes
```
