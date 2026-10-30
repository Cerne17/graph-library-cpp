# Rodando os estudos de caso da Parte 2 numa VM da Azure

Guia para reproduzir a rodada de `analysis/harness_weighted.py` numa VM da
Azure (conta *Azure for Students*), a partir de um Mac. Tudo passa por
`analysis/azure_part2.sh`; o acompanhamento ao vivo, por
`analysis/azure_watchdog.py`. As armadilhas da seção 3 custaram horas na
primeira vez: leia antes de criar qualquer coisa.

---

## 1. Visão geral

```
check → create → setup → upload → start → status (+ watchdog) → fetch → delete
```

| passo | o que faz | custa? |
|---|---|---|
| `check` | cota de vCPUs e tamanhos de VM disponíveis na região | não |
| `create` | grupo de recursos + VM + desligamento automático | **sim, a partir daqui** |
| `setup` | instala compiladores, clona o branch, `make benchmark-weighted`, `make test`, grava `environment.txt` | sim |
| `upload` | envia as entradas de `data/` (~2 GB, retomável) | sim |
| `start` | roda o harness dentro de um `tmux` na VM (pode fechar o notebook) | sim |
| `status` | últimas linhas do log e se terminou | sim |
| `fetch` | traz resultados para `data/azure/` | sim |
| `delete` | apaga o grupo inteiro: **só aqui a cobrança para** | — |

A VM clona o branch do GitHub (`BRANCH`, padrão `feature/harness-part2`): o
código que roda é o que está no remoto, não o da sua máquina. Faça push antes
do `setup`.

## 2. Pré-requisitos

```bash
brew install azure-cli
az login                       # escolha a assinatura "Azure for Students"
az account show --query name   # confirme
```

Também: `ssh` e `rsync` (já vêm no macOS), Python 3, e as entradas em `data/`:
`baseline.txt`, `grafo_W_*.txt.gz`, `rede_colaboracao.txt`,
`rede_colaboracao_vertices.txt`.

## 3. Armadilhas da Azure for Students

### 3.1 Só algumas regiões são permitidas

Cada conta tem uma política com a lista de regiões. Veja a sua:

```bash
az policy assignment list --query "[].{name:displayName, params:parameters}" -o json
```

Procure "Allowed resource deployment regions". Na nossa conta eram
`chilecentral`, `francecentral`, `northcentralus`, `italynorth`,
`belgiumcentral`. Criar fora delas falha.

### 3.2 Provedores não registrados

Numa assinatura nova, `Microsoft.Compute` e `Microsoft.Network` vêm
`NotRegistered`. Sintoma: `az vm list-usage` devolve `[]` e o `check` mostra a
cota vazia. Registre uma vez (grátis):

```bash
az provider register -n Microsoft.Compute --wait
az provider register -n Microsoft.Network --wait
```

### 3.3 A cota é pequena, e conta vCPUs *totais*

Na nossa conta: **6 vCPUs por região** e **cota 0 para todas as famílias v5**
(`Standard_E*as_v5`, `D*as_v5`, ...). O `check` esconde as famílias com
limite 0; para vê-las:

```bash
az vm list-usage -l <região> \
  --query "[?contains(localName, 'v5') || contains(localName, 'v4')].{name:localName, limit:limit}" -o table
```

Tamanhos com vCPU restrito (ex.: `E16-4as_v5`, 128 GB e 4 vCPUs ativos) contam
na cota pelos vCPUs **totais** (16), não pelos ativos. Logo, nenhuma VM de
128 GB cabe em 6 vCPUs e a matriz do grafo_W_3 (~76 GB) não roda nessa conta.
A lista padrão de `AZ_SIZES` no script é toda v5: nessa conta, passe
`AZ_SIZES` explicitamente.

Para achar o maior tamanho que cabe na cota, cruze os SKUs disponíveis com a
cota de cada família:

```bash
R=belgiumcentral
az vm list-skus -l $R --resource-type virtualMachines -o json > /tmp/skus.json
az vm list-usage -l $R -o json > /tmp/usage.json
python3 - <<'EOF'
import json
skus = json.load(open("/tmp/skus.json")); usage = json.load(open("/tmp/usage.json"))
quota = {u["name"]["value"].lower(): int(u["limit"]) for u in usage}
rows = []
for s in skus:
    if s.get("restrictions"):
        continue
    cap = {c["name"]: c["value"] for c in s.get("capabilities", [])}
    v = int(cap.get("vCPUs", 0))
    if 0 < v <= min(quota.get((s.get("family") or "").lower(), 0), quota.get("cores", 0)):
        rows.append((float(cap.get("MemoryGB", 0)), s["name"], v))
for mem, name, v in sorted(rows, reverse=True)[:10]:
    print(f"{mem:6.0f} GB  {name:24} {v} vCPUs")
EOF
```

Na nossa conta, o melhor foi **`Standard_E4as_v4` em `belgiumcentral`**
(32 GB, 4 vCPUs, ~US$ 0,27/h). Evite a série B (*burstable*): os créditos de
CPU acabam e as medições de tempo ficam erradas.

Preços (Linux, sob demanda) pela API pública:

```bash
curl -s -G "https://prices.azure.com/api/retail/prices" --data-urlencode \
  "\$filter=serviceName eq 'Virtual Machines' and armSkuName eq 'Standard_E4as_v4' and priceType eq 'Consumption' and armRegionName eq 'belgiumcentral'"
```

Cada região lista dois preços; o menor é o Linux.

### 3.4 Desligamento automático não existe em toda região

O `create` agenda um *auto-shutdown* (~23 h depois) como rede de segurança.
Em `belgiumcentral` o serviço (`Microsoft.DevTestLab/schedules`) não existe:
a VM é criada, mas o script termina com
`LocationNotAvailableForResourceType`. A VM está ok; só não há desligamento
automático, então **não esqueça o `delete`**. Desligar pelo sistema
operacional da VM não adianta: a Azure continua cobrando até desalocar ou
apagar.

### 3.5 Grupo de recursos com o mesmo nome em outra região

Se um `cos242-part2` antigo existir em outra região, o `create` falha com
`InvalidResourceGroupLocation`. Confira se está vazio
(`az resource list -g cos242-part2 -o table`) e apague
(`az group delete -n cos242-part2 --yes`), ou use outro nome com `AZ_GROUP`
em **todos** os passos.

## 4. Passo a passo

Rode tudo da raiz do repositório. As variáveis valem para todos os passos;
exporte uma vez:

```bash
export AZ_LOCATION=belgiumcentral AZ_SIZES=Standard_E4as_v4
```

```bash
analysis/azure_part2.sh check     # confira cota e tamanhos (seção 3)
analysis/azure_part2.sh create    # anota o tamanho criado: ele decide quais matrizes rodam
analysis/azure_part2.sh setup     # tem que terminar com "Status: SUCCESS!" e imprimir environment.txt
analysis/azure_part2.sh upload    # ~2 GB; se cair, rode de novo (rsync retoma)
analysis/azure_part2.sh start
analysis/azure_part2.sh status    # deve mostrar "matrix memory limit: ... GB" e "processing grafo_W_3 ..."
```

O harness pula toda matriz que não cabe na memória (status `no_memory`, com a
estimativa registrada). Em 32 GB, todas as matrizes são puladas; isso é
esperado, não é erro. Uma linha como
`grafo_W_4 (matrix): needs ~... MB, limit ... MB: skipped` é normal.

## 5. Acompanhando a execução

### 5.1 `status`

```bash
analysis/azure_part2.sh status
```

O log só ganha uma linha quando um grafo **termina**. Num grafo grande, ele
fica parado por horas e isso é normal. Para saber se está trabalhando,
olhe a CPU do benchmark:

```bash
ssh azureuser@<ip> 'p=$(pgrep -f "[b]uild/benchmark_weighted"); ps -o etime=,pcpu=,rss= -p $p'
```

CPU perto de 100% e memória estável = tudo certo. Preocupe-se com `FAILED`,
traceback do Python, ou CPU parada sem o log avançar.

### 5.2 Watchdog com barra de progresso

`analysis/azure_watchdog.py` atualiza a cada 30 s:

- barra animada (alive-progress) com o grafo atual, tempo, CPU, memória e ETA;
- copia `results_weighted.csv`, `paths_weighted.csv`, `run.log` e
  `environment.txt` para `data/azure/` enquanto roda (o harness grava um grafo
  inteiro por vez, então os arquivos estão sempre consistentes);
- quando um grafo termina, imprime memória, medianas, os 5 caminhos e as
  verificações da seção 6;
- avisa se aparecer `FAILED` ou traceback; ao terminar, apita e sai.

Instale a dependência num venv fora do repositório (o projeto não tem
dependências Python de terceiros além de pandas/psutil do harness):

```bash
python3 -m venv ~/.venvs/cos242-watchdog
~/.venvs/cos242-watchdog/bin/pip install alive-progress
~/.venvs/cos242-watchdog/bin/python analysis/azure_watchdog.py      # ou: ... 60 para atualizar a cada 60 s
```

Ctrl-C para o watchdog, não a execução na VM. A barra é estimada: grafos
terminados contam inteiros, o atual avança pelo tempo decorrido contra
`EST_MIN` (no topo do script, medido na nossa rodada em `Standard_E4as_v4`),
limitado a 95% até terminar. Para outro tamanho de VM, ajuste `EST_MIN`.

Se abrir o watchdog com `open -a Terminal`, o prompt de atualização do
oh-my-zsh (`[Y/n]`) pode engolir o primeiro caractere do comando. Rode-o de
novo.

## 6. Buscar, conferir e apagar

```bash
analysis/azure_part2.sh fetch      # → data/azure/
```

Confira em `data/azure/`:

- `results_weighted.csv` tem linhas para os 6 grafos. Em `csr`,
  `memory_mb_graph`, `dijkstra_heap_micros` e `dijkstra_lazy_micros` com status
  `ok`. `dijkstra_array` é `ok` só no grafo_W_3 (`skipped` nos outros). Toda
  linha de matriz é `ok` ou `no_memory`, nunca `error`.
- `paths_weighted.csv` tem 5 alvos por grafo e representação medida (só `csr`
  quando as matrizes são puladas). Na rede de colaboração os caminhos usam
  nomes; Alan M. Turing aparece `unreachable` (outra componente).

```bash
grep -v ',ok$' data/azure/results_weighted.csv               # só no_memory e skipped esperados
tail -n +2 data/azure/paths_weighted.csv | cut -d, -f1,2 | sort | uniq -c
```

Antes de apagar, confira se a VM não tem mais nada útil e se as cópias
locais batem:

```bash
ssh azureuser@<ip> 'cd library && git status --short'
ssh azureuser@<ip> 'cd library && sha256sum data/results_weighted.csv data/paths_weighted.csv run.log environment.txt'
shasum -a 256 data/azure/results_weighted.csv data/azure/paths_weighted.csv data/azure/run.log data/azure/environment.txt
```

Então:

```bash
analysis/azure_part2.sh delete
az group list -o table             # cos242-part2 não pode aparecer
```

`NetworkWatcherRG` aparece sozinho na primeira rede criada numa região. O
Network Watcher em si não é cobrado; pode ficar.

## 7. Referência: a rodada de 2026-10-30

`Standard_E4as_v4`, `belgiumcentral`, AMD EPYC 7763, g++ 13.3, k = 100,
seed 2025. Cerca de 7,5 h de execução, ~US$ 2–3 no total.

| grafo | n | m | CSR (MB) | heap (mediana) | lazy (mediana) | array (mediana) | tempo aprox. |
|---|---|---|---|---|---|---|---|
| grafo_W_3 | 100 mil | 6,1 mi | 140,6 | 117,9 ms | 153,6 ms | 26,50 s | ~48 min |
| grafo_W_4 | 1 mi | 31,0 mi | 717,4 | 2,13 s | 2,04 s | — | ~10 min |
| grafo_W_5 | 10 mi | 40,0 mi | 992,0 | 23,71 s | 12,83 s | — | ~55 min |
| grafo_W_6 | 10 mi | 73,4 mi | 1756,5 | 27,79 s | 17,99 s | — | ~1 h 20 |
| grafo_W_7 | 50 mi | 60,0 mi | 1755,0 | 111,10 s | 40,96 s | — | ~4 h 15 |
| rede_colaboracao | 722 mil | 2,27 mi | 57,7 | 474,4 ms | 307,9 ms | — | minutos |

Todas as matrizes: `no_memory`. Para estimar outra rodada, uma execução
local só de tempo dá a escala (o modo `all` fica bloqueado esperando o
harness medir memória; use `time`):

```bash
build/benchmark_weighted grafo.txt csr time --k 1 --seed 2025 --array no
```

Nessa VM, o tempo por execução ficou 1,2–1,8× o de um Mac Apple Silicon.
