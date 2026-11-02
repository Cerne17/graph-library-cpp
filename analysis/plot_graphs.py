"""Save Part 1 and Part 2 SVG/PNG figures from validated exports; never run benchmarks."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
from pathlib import Path

# Keep plotting caches outside the user's home and the source tree.
os.environ.setdefault(
    "MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "cos242-matplotlib")
)
os.environ.setdefault(
    "XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "cos242-cache")
)
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
import scipy
from matplotlib.collections import LineCollection
from visualization_data import ALGORITHMS, ROOT, azure_data, load_data, read_rows

COLORS = ["#24758a", "#db8b34", "#9163ac"]
FIGURE_TITLES = {
    "graph_sizes": "Tamanho dos grafos",
    "degree_frequencies": "Frequências dos graus",
    "degree_tails": "Distribuições acumuladas complementares dos graus",
    "weight_distributions": "Distribuições dos pesos",
    "component_sizes": "Tamanhos das componentes",
    "case_distances": "Distâncias dos estudos de caso",
    "azure_timings": "Tempos no Azure",
    "azure_memory": "Memória no Azure",
    "search_depths": "Profundidades de BFS e DFS",
    "azure_traversals": "Tempos de BFS e DFS em CSR no Azure",
    "azure_traversals_memory": "Memória das representações na nova rodada do Azure",
    "azure_traversals_matrix": "Comparação CSR e matriz com bitmask no Azure",
}
STATUS_LABELS = {"unreachable": "inalcançável", "skipped": "não executado",
                 "unavailable": "indisponível", "not_implemented": "não implementado",
                 "no_memory": "memória insuficiente"}


def graph_name(graph):
    if graph["id"] == "rede_colaboracao":
        return "Rede de colaboração"
    if graph["id"].startswith("grafo_W_"):
        return "Grafo Ponderado " + graph["id"].removeprefix("grafo_W_")
    if graph["id"].startswith("grafo_"):
        return "Grafo " + graph["id"].removeprefix("grafo_")
    return graph["name"]


TITLES = ["Heap indexado", "Heap com remoção tardia", "Vetor"]


def degree_tail(rows):
    """Exact P(degree >= k), including isolated vertices in the denominator."""
    rows = sorted(rows)
    total = sum(count for _, count in rows)
    remaining = total
    result = []
    for degree, count in rows:
        result.append((degree, remaining / total))
        remaining -= count
    return result


def panel_grid(graphs, title, ylabel, xlabel):
    rows = (len(graphs) + 1) // 2
    fig, axes = plt.subplots(
        rows, 2, figsize=(12, 3.1 * rows), squeeze=False, layout="constrained"
    )
    fig.suptitle(title, fontsize=17)
    for ax, graph in zip(axes.flat, graphs):
        ax.set_title(graph_name(graph), loc="left", fontweight="bold")
        ax.set(xlabel=xlabel, ylabel=ylabel)
    for ax in list(axes.flat)[len(graphs) :]:
        ax.set_visible(False)
    return fig, list(axes.flat)


def save(fig, out, name, records, caption):
    for suffix in ("svg", "png"):
        path = out / f"{name}.{suffix}"
        fig.savefig(
            path, dpi=160, metadata={"Creator": "COS242 analysis/plot_graphs.py"}
        )
    plt.close(fig)
    records.append({"name": name, "caption": caption})
    print(f"Saved {name}", flush=True)


def statistical_figures(model, out, records):
    graphs = model["graphs"]
    labels = [graph_name(g) for g in graphs]
    x = np.arange(len(graphs))
    fig, ax = plt.subplots(figsize=(11, 4.6), layout="constrained")
    for offset, metric, color in [
        (-0.19, "vertices", COLORS[0]),
        (0.19, "edges", COLORS[1]),
    ]:
        values = [g["summary"][metric] for g in graphs]
        bars = ax.bar(
            x + offset,
            np.asarray(values) - 0.5,
            0.38,
            bottom=0.5,
            label={"vertices": "Vértices", "edges": "Arestas"}[metric],
            color=color,
        )
        ax.bar_label(
            bars,
            labels=[f"{v:,.0f}" for v in values],
            rotation=35,
            padding=4,
            fontsize=8,
        )
    ax.set(
        xticks=x,
        xticklabels=labels,
        yscale="log",
        ylabel="Quantidade (escala log)",
        title="Tamanho dos grafos",
    )
    ax.set_ylim(top=max(g["summary"]["edges"] for g in graphs) * 8)
    ax.legend(loc="upper left")
    save(
        fig,
        out,
        "graph_sizes",
        records,
        "Quantidades de vértices e arestas não direcionadas dos grafos originais; arestas paralelas são contadas individualmente.",
    )

    fig, axes = panel_grid(
        graphs, "Distribuições de graus · todos os vértices", "Vértices (escala log)", "Grau"
    )
    for graph, ax in zip(graphs, axes):
        values, counts = zip(*graph["degrees"])
        ax.scatter(values, counts, color=COLORS[0], s=9)
        ax.set_yscale("log")
        ax.set_xscale("symlog", linthresh=1)
        ax.set_xlabel("Grau (linear próximo de 0; log acima de 1)")
        ax.set_xlim(left=min(values))
    save(
        fig,
        out,
        "degree_frequencies",
        records,
        "Frequências exatas dos graus, incluindo grau zero. Arestas paralelas contribuem para o grau. Cada ponto representa um grau discreto; não há linhas de ligação.",
    )

    fig, axes = panel_grid(
        graphs, "Distribuições acumuladas complementares dos graus", "P(grau ≥ k) (escala log)", "Grau k"
    )
    for graph, ax in zip(graphs, axes):
        values, tails = zip(*degree_tail(graph["degrees"]))
        ax.scatter(values, tails, color=COLORS[0], s=9)
        ax.set(yscale="log", xscale="symlog")
        ax.set_xscale("symlog", linthresh=1)
        ax.set_xlabel("Grau k (linear próximo de 0; log acima de 1)")
        ax.set_xlim(left=min(values))
    save(
        fig,
        out,
        "degree_tails",
        records,
        "Distribuições acumuladas complementares exatas dos graus: P(grau ≥ k). O denominador inclui todos os vértices. Os valores discretos são representados apenas por pontos.",
    )

    if graphs[0]["weighting"] == "weighted":
        fig, axes = panel_grid(graphs, "Pesos das arestas · todas as arestas", "Arestas", "Peso")
        for graph, ax in zip(graphs, axes):
            bins = graph["weights"]
            log_counts = graph["id"] == "rede_colaboracao"
            baseline = 0.5 if log_counts else 0
            nonempty = [(lo, hi, n) for lo, hi, n in bins if n]
            ax.bar(
                [lo for lo, _, _ in nonempty],
                [n - baseline for _, _, n in nonempty],
                bottom=baseline,
                width=[hi - lo if hi > lo else 1 for lo, hi, _ in nonempty],
                align="edge",
                color=COLORS[0],
            )
            if log_counts:
                ax.set(yscale="log", ylabel="Arestas (escala log)", ylim=(baseline, None))
            ax.margins(y=0.25)
            s = graph["summary"]
            ax.text(
                0.98,
                0.95,
                f"Média {s['weight_mean']:.4g}\nArestas de peso zero: {s['zero_weight_edges']:,.0f}",
                ha="right",
                va="top",
                transform=ax.transAxes,
                fontsize=9,
            )
        save(
            fig,
            out,
            "weight_distributions",
            records,
            "Histograma dos pesos nos intervalos coletados. A contagem de pesos zero é exata. A rede de colaboração usa escala log nas contagens; intervalos vazios não têm barra.",
        )

    fig, axes = panel_grid(
        graphs,
        "Componentes conexas · todas as componentes",
        "Componentes (escala log)",
        "Tamanho da componente (escala log)",
    )
    for graph, ax in zip(graphs, axes):
        sizes, counts = zip(*graph["components"])
        ax.scatter(sizes, counts, color=COLORS[0], s=23)
        ax.set(xscale="log", yscale="log")
        s = graph["summary"]
        ax.text(
            0.98,
            0.95,
            f"Componentes: {s['component_count']:,.0f}\nIsolados: {s['isolated_vertices']:,.0f}\nMaior componente: {100 * s['component_largest'] / s['vertices']:.4g}%",
            ha="right",
            va="top",
            transform=ax.transAxes,
            fontsize=9,
        )
    save(
        fig,
        out,
        "component_sizes",
        records,
        "Frequências exatas dos tamanhos das componentes, representadas apenas por pontos. Cada ponto informa quantas componentes possuem aquele tamanho.",
    )

    fig, axes = panel_grid(
        graphs,
        "Caminhos mínimos exigidos · estudos de caso",
        "Distância ponderada" if graphs[0]["weighting"] == "weighted" else "Distância em arestas",
        "Vértice / pesquisador de destino",
    )
    for graph, ax in zip(graphs, axes):
        cases = [c for c in graph["cases"] if c["algorithm"] == ("dijkstra_heap" if graph["weighting"] == "weighted" else "bfs_distance")]
        for i, case in enumerate(cases):
            if case["status"] == "ok":
                ax.bar(i, case["distance"], color=COLORS[0])
                ax.annotate(
                    f"{case['hops']} arestas",
                    (i, case["distance"]),
                    xytext=(0, 4),
                    textcoords="offset points",
                    ha="center",
                    fontsize=8,
                )
            else:
                ax.text(
                    i,
                    0.04,
                    STATUS_LABELS.get(case["status"], case["status"]),
                    transform=ax.get_xaxis_transform(),
                    rotation=90,
                    ha="center",
                    va="bottom",
                    color="#a84c32",
                    fontsize=9,
                )
        names = [c["target_name"] or (str(c["target"]) if graph["weighting"] == "weighted" else f"{c['source']} → {c['target']}") for c in cases]
        ax.set_xticks(range(len(cases)), names, rotation=25, ha="right", fontsize=8)
        ax.margins(y=0.25)
        ax.set_xlim(-0.6, len(cases) - 0.4)
        source = cases[0]["source_name"] or str(cases[0]["source"])
        ax.set_title(f"{graph_name(graph)} · origem {source}" if graph["weighting"] == "weighted" else graph_name(graph), loc="left", fontsize=11)
    save(
        fig,
        out,
        "case_distances",
        records,
        "Distâncias dos caminhos mínimos e número de arestas dos caminhos salvos. Nos grafos ponderados, as distâncias são ponderadas; nos demais, contam arestas. Destinos inalcançáveis não possuem barra nem distância zero.",
    )

    if graphs[0]["weighting"] == "unweighted":
        fig, axes = panel_grid(graphs, "Profundidades das árvores de busca", "Profundidade (arestas)", "Origem → destino")
        for graph, ax in zip(graphs, axes):
            pairs = sorted({(c["source"], c["target"]) for c in graph["cases"] if c["algorithm"] == "bfs"})
            for algorithm, offset, color in (("bfs", -.12, COLORS[0]), ("dfs", .12, COLORS[1])):
                by_pair = {(c["source"], c["target"]): c for c in graph["cases"] if c["algorithm"] == algorithm}
                valid = [(i, by_pair[p]["hops"]) for i, p in enumerate(pairs) if by_pair[p]["status"] == "ok"]
                ax.scatter([i+offset for i, _ in valid], [h for _, h in valid], s=15, label=algorithm.upper(), color=color)
            ax.set_xticks(range(len(pairs)), [f"{s} → {t}" for s, t in pairs], rotation=55, ha="right", fontsize=8)
            ax.set_yscale("symlog", linthresh=1)
            ax.set_ylim(bottom=0)
        axes[0].legend()
        save(fig, out, "search_depths", records, "Profundidades das árvores BFS e DFS nos pares exigidos. BFS fornece distâncias mínimas em arestas; a profundidade da DFS não é uma distância mínima. Destinos inalcançáveis não possuem ponto. Escala vertical linear próxima de zero e logarítmica acima de uma aresta.")

    azure = model["azure"]
    if not azure["available"] or not any(
        azure["graphs"][g["id"]][a]["status"] == "ok" for g in graphs for a in ALGORITHMS
    ):
        return
    fig, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    for ax, statistic in zip(axes, ("mean_seconds", "median_seconds")):
        positive = [
            azure["graphs"][g["id"]][a][statistic]
            for g in graphs
            for a in ALGORITHMS
            if azure["graphs"][g["id"]][a]["status"] == "ok"
            and azure["graphs"][g["id"]][a][statistic] > 0
        ]
        baseline = min(positive, default=1) / 2
        for i, (algorithm, title, color) in enumerate(zip(ALGORITHMS, TITLES, COLORS)):
            positions, values = [], []
            for j, graph in enumerate(graphs):
                record = azure["graphs"][graph["id"]][algorithm]
                if record["status"] == "ok":
                    positions.append(j + (i - 1) * 0.24)
                    values.append(record[statistic])
            ax.bar(
                positions,
                np.asarray(values) - baseline,
                0.23,
                bottom=baseline,
                label=title,
                color=color,
            )
        ax.set(
            xticks=x,
            xticklabels=labels,
            yscale="log",
            ylabel="Segundos (escala log)",
            title={"mean_seconds": "Média", "median_seconds": "Mediana"}[statistic],
        )
        ax.set_ylim(bottom=baseline)
        ax.tick_params(axis="x", rotation=25)
    axes[0].legend()
    fig.suptitle("Dijkstra com CSR · execuções existentes no Azure", fontsize=16)
    save(
        fig,
        out,
        "azure_timings",
        records,
        "Médias e medianas dos tempos no Azure, convertidas de microssegundos para segundos. Há medições com vetor apenas para o Grafo Ponderado 3; barras ausentes indicam medições não realizadas. A tabela informa o número de amostras.",
    )

    fig, ax = plt.subplots(figsize=(10, 4.4), layout="constrained")
    values = [azure["graphs"][g["id"]]["csr_mib"] for g in graphs]
    valid = [(i, v) for i, v in enumerate(values) if v is not None]
    bars = ax.bar([i for i, _ in valid], [v for _, v in valid], color=COLORS[0])
    ax.bar_label(bars, labels=[f"{v:,.1f}" for _, v in valid], padding=4)
    ax.set(
        xticks=x,
        xticklabels=labels,
        ylabel="MiB",
        title="Memória da representação CSR · medições no Azure",
    )
    ax.margins(y=0.15)
    save(
        fig,
        out,
        "azure_memory",
        records,
        "RSS medido para a representação CSR no Azure. As estimativas da matriz aparecem separadamente na tabela; esses valores não representam o pico de memória de Dijkstra.",
    )


def traversal_figures(model, out, records):
    azure = model.get("traversals", {})
    if not azure.get("available"):
        return
    graphs = model["graphs"]
    fig, axes = plt.subplots(1, 2, figsize=(15, 6), layout="constrained")
    x = np.arange(len(graphs))
    for ax, statistic, title in zip(axes, ("mean_seconds", "median_seconds"), ("Média", "Mediana")):
        positive = [azure["graphs"][g["id"]][a][statistic] for g in graphs for a in ("bfs", "dfs") if azure["graphs"][g["id"]][a][statistic] > 0]
        baseline = min(positive, default=1) / 2
        for algorithm, offset, color in (("bfs", -.18, COLORS[0]), ("dfs", .18, COLORS[1])):
            values = [azure["graphs"][g["id"]][algorithm][statistic] for g in graphs]
            ax.bar(x+offset, np.asarray(values)-baseline, width=.35, bottom=baseline, color=color, label=algorithm.upper())
        ax.set(yscale="log", ylim=(baseline, None), ylabel="Segundos (escala log)", title=title)
        ax.set_xticks(x, [graph_name(g) for g in graphs], rotation=60, ha="right", fontsize=8)
    axes[0].legend()
    fig.suptitle("BFS e DFS com CSR · repetições ajustadas ao custo", fontsize=16)
    save(fig, out, "azure_traversals", records, "Tempos de BFS e DFS da nova rodada no Azure. O número de origens aleatórias distintas varia por grafo e armazenamento; BFS e DFS mantêm as mesmas origens. A tabela informa os números efetivos de amostras. As buscas ignoram pesos; resultados da rodada anterior de Dijkstra aparecem separadamente.")
    eligible = [g for g in graphs if g["id"] in {"grafo_1", "grafo_2", "grafo_3", "grafo_4"}]
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    x = np.arange(len(eligible))
    combinations = [(rep, algorithm) for rep in ("csr", "matrix") for algorithm in ("bfs", "dfs")]
    colors = [COLORS[0], COLORS[1], COLORS[2], "#648346"]
    for ax, statistic, title in zip(axes, ("mean_seconds", "median_seconds"), ("Média", "Mediana")):
        series = []
        for rep, algorithm in combinations:
            group = model["traversals_paired_csr"] if rep == "csr" else model["traversals_paired_matrix"]
            series.append([group["graphs"][g["id"]][algorithm][statistic] for g in eligible])
        baseline = min(v for values in series for v in values if v > 0) / 2
        for i, ((rep, algorithm), values, color) in enumerate(zip(combinations, series, colors)):
            label = ("CSR" if rep == "csr" else "Matriz") + " · " + algorithm.upper()
            ax.bar(x+(i-1.5)*.19, np.asarray(values)-baseline, width=.18, bottom=baseline, color=color, label=label)
        ax.set(yscale="log", ylim=(baseline,None), ylabel="Segundos (escala log)", title=title)
        ax.set_xticks(x, [graph_name(g) for g in eligible])
    axes[0].legend(fontsize=8)
    fig.suptitle("Parte 1 · CSR e matriz com bitmask no Azure", fontsize=16)
    save(fig, out, "azure_traversals_matrix", records, "Comparação de BFS e DFS em CSR e matriz com bitmask nos grafos 1 a 4, restrita às mesmas origens aleatórias nas quatro combinações. A média CSR deste gráfico usa o prefixo comum com a matriz, não necessariamente todas as amostras CSR. As matrizes dos grafos 5 e 6 excedem o limite de memória e não são executadas.")
    fig, ax = plt.subplots(figsize=(15, 6), layout="constrained")
    x = np.arange(len(graphs))
    for key, offset, color, label in (("csr_mib", -.18, COLORS[0], "CSR"), ("matrix_mib", .18, COLORS[1], "Matriz com bitmask")):
        pairs = [(i, azure["graphs"][g["id"]][key] / 1024) for i, g in enumerate(graphs) if azure["graphs"][g["id"]][key] is not None and azure["graphs"][g["id"]][key] > 0]
        ax.bar([i+offset for i, _ in pairs], [value for _, value in pairs], width=.35, color=color, label=label)
    ax.set(yscale="log", ylabel="Memória da representação (GiB, escala log)", title="RSS antes das buscas · nova rodada no Azure")
    ax.set_xticks(x, [graph_name(g) for g in graphs], rotation=60, ha="right", fontsize=8)
    ax.legend()
    save(fig, out, "azure_traversals_memory", records, "Memória residente da representação antes das buscas, descontado o processo mínimo: média das cargas de BFS e DFS. Os valores não representam o pico de memória dos algoritmos. Barras ausentes indicam representações não executadas; as matrizes ponderadas não fazem parte desta rodada.")



def network_figures(model, out, records):
    for graph in model["graphs"]:
        fig, axes = plt.subplots(1, 2, figsize=(13, 6), layout="constrained")
        for ax, view, title in zip(
            axes,
            ("neighborhood", "overview"),
            ("Vizinhança limitada", "Amostra por tamanhos de componentes"),
        ):
            sample = graph["networks"][view]
            network = nx.Graph()
            network.add_nodes_from(n["id"] for n in sample["nodes"])
            network.add_edges_from((u, v) for u, v, _ in sample["edges"])
            # Uniform spring strengths: geometric lengths do not encode edge weights.
            pos = nx.spring_layout(
                network, seed=2025, iterations=45, weight=None, method="force"
            )
            segments = [[pos[u], pos[v]] for u, v in network.edges]
            ax.add_collection(
                LineCollection(segments, colors="#71868c", linewidths=0.35, alpha=0.18)
            )
            nodes = sample["nodes"]
            points = np.array([pos[n["id"]] for n in nodes])
            if len(nodes):
                full_isolates = [n["degree"] == 0 for n in nodes]
                ax.scatter(
                    points[:, 0],
                    points[:, 1],
                    s=[3 + 3 * np.log1p(n["degree"]) for n in nodes],
                    c=[
                        COLORS[1] if isolated else COLORS[0]
                        for isolated in full_isolates
                    ],
                    linewidths=0,
                    alpha=0.85,
                )
            ax.autoscale()
            ax.margins(0.08)
            ax.set_aspect("equal")
            ax.axis("off")
            removed = sample["induced_edges"] - len(sample["edges"])
            ax.set_title(
                f"{title}\n{len(nodes):,} vértices · {len(sample['edges']):,} arestas exportadas\n{removed:,} arestas induzidas omitidas",
                fontsize=10,
            )
        fig.suptitle(f"{graph_name(graph)} · estrutura amostrada", fontsize=16)
        fig.supxlabel(
            "Disposição ilustrativa · tamanho do vértice: grau no grafo original · laranja: vértice isolado no grafo original",
            fontsize=10,
        )
        save(
            fig,
            out,
            f"network_{graph['id']}",
            records,
            f"{graph_name(graph)}: duas amostras limitadas por busca em largura (até 1.000 vértices e 4.000 arestas cada), com semente 2025. As amostras não são aleatórias uniformes. Arestas paralelas se sobrepõem neste desenho da topologia simples. Os comprimentos desenhados não representam pesos; o limite de arestas pode produzir desconexões visuais.",
        )


def write_gallery(model, out, records, gallery):
    lines = [
        "# Galeria de visualizações — Partes 1 e 2",
        "",
        "Gerada por `analysis/plot_graphs.py` a partir dos dados coletados e dos resultados salvos do Azure.",
        "A geração das figuras não executa Dijkstra, análises integrais dos grafos nem novas repetições dos benchmarks.",
        "",
        "## Regenerar",
        "",
        "```bash",
        "make figures",
        "make test-visualizations",
        "```",
        "",
        "O interpretador Python precisa de Matplotlib, NumPy, NetworkX e SciPy, já disponíveis no ambiente local atual. São ferramentas opcionais de visualização; a biblioteca C++ e o coletor não dependem delas. Use `make figures PYTHON=/caminho/do/python` para selecionar outro ambiente. O script não instala pacotes.",
        "",
        "As figuras são salvas em SVG editável e PNG de 160 dpi em `docs/figures/`, para uso posterior na apresentação. Execute os comandos na raiz do repositório. Opções: `python3 analysis/plot_graphs.py --help`.",
        "",
        "As distribuições exatas incluem todos os vértices e arestas. Dados discretos, como graus e tamanhos de componentes, são representados apenas por pontos, sem segmentos de ligação. As imagens das redes usam amostras limitadas e não estimam propriedades globais. Os diâmetros em número de arestas nos dados de origem desconsideram pesos; o resultado da dupla varredura é um limite inferior restrito à maior componente.",
        "",
        "`figures/provenance.json` registra hashes dos dados de entrada e do executável, commits da biblioteca, versões das ferramentas e hashes das figuras. O ambiente do Azure e o número de amostras dos tempos aparecem abaixo.",
        "",
    ]
    lines += ["## Resumo estrutural", "", "Laços são descartados pelo leitor e contados separadamente. CSR preserva arestas paralelas.", "", "| Grafo | Vértices | Arestas | Componentes | Maior componente (%) | Laços descartados | Paralelas |", "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for graph in model["graphs"]:
        s = graph["summary"]
        lines.append(f"| {graph_name(graph)} | {s['vertices']:,.0f} | {s['edges']:,.0f} | {s['component_count']:,.0f} | {100*s['component_largest']/s['vertices']:.4f} | {s.get('skipped_self_loops', 0):,.0f} | {s['parallel_edges']:,.0f} |")
    lines += ["", "## Graus e diâmetros em arestas", "", "| Grafo | Grau mínimo | Mediana | Média | Máximo | Diâmetro / limite |", "| --- | ---: | ---: | ---: | ---: | --- |"]
    for graph in model["graphs"]:
        s = graph["summary"]
        exact = next((r for r in graph["diameter"] if r["method"] == "exact" and r["status"] == "ok"), None)
        lower = next((r for r in graph["diameter"] if r["method"] == "double_sweep" and r["status"] == "lower_bound"), None)
        diameter = (exact["value"] + " (exato, máximo nas componentes)") if exact else ("≥ " + lower["value"] + " (maior componente)") if lower else "indisponível"
        lines.append(f"| {graph_name(graph)} | {s['degree_min']:g} | {s['degree_median']:g} | {s['degree_mean']:.6g} | {s['degree_max']:g} | {diameter} |")
    lines += [""]
    for record in records:
        name = record["name"]
        title = FIGURE_TITLES.get(Path(name).name)
        if title and "/" in name:
            title = ("Parte 1 — " if name.startswith("parte1/") else "Parte 2 — ") + title
        if title is None:
            graph = next(g for g in model["graphs"] if name == "network_" + g["id"])
            title = f"Estrutura amostrada — {graph_name(graph)}"
        relative = os.path.relpath(out / name, gallery.parent).replace(os.sep, "/")
        lines += [
            f"## {title}",
            "",
            record["caption"],
            "",
            f"![{record['caption']}]({relative}.png)",
            "",
            f"[SVG]({relative}.svg) · [PNG]({relative}.png)",
            "",
        ]
    lines += [
        "## Rodada anterior de Dijkstra no Azure",
        "",
        "As figuras de desempenho usam apenas os resultados salvos do Azure, sem os testes locais rápidos. Tempos ausentes não equivalem a zero. Os valores `memory_mb_*` do CSV histórico estão em MiB.",
        "",
        "| Grafo | Amostras: heap indexado / tardio / vetor | CSR medido (MiB) | Matriz estimada (MiB) |",
        "| --- | --- | ---: | ---: |",
    ]
    azure = model["azure"]
    for graph in model["graphs"]:
        if not azure["available"]:
            break
        if graph["weighting"] != "weighted":
            continue
        measurements = azure["graphs"][graph["id"]]
        samples = " / ".join(
            str(measurements[a]["samples"])
            if measurements[a]["status"] == "ok"
            else STATUS_LABELS.get(measurements[a]["status"], measurements[a]["status"])
            for a in ALGORITHMS
        )

        def cell(value):
            return "indisponível" if value is None else f"{value:,.1f}"

        lines.append(
            f"| {graph_name(graph)} | {samples} | {cell(measurements['csr_mib'])} | {cell(measurements['matrix_estimate_mib'])} |"
        )
    lines += [
        "",
        "As estimativas da matriz não são alocações medidas. Na rodada anterior dos grafos ponderados no Azure, todas as construções de matriz foram omitidas por falta de memória.",
        "",
        "```text",
        azure["environment"].rstrip(),
        "```",
        "",
    ]
    lines += ["## Nova rodada de BFS e DFS no Azure", ""]
    if model.get("traversals", {}).get("available"):
        lines += ["| Grafo | BFS: média / mediana (s) | DFS: média / mediana (s) | Amostras CSR por busca |", "| --- | ---: | ---: | ---: |"]
        for graph in model["graphs"]:
            data = model["traversals"]["graphs"][graph["id"]]
            def timing(algorithm):
                return f"{data[algorithm]['mean_seconds']:.6g} / {data[algorithm]['median_seconds']:.6g}"
            lines.append(f"| {graph_name(graph)} | {timing('bfs')} | {timing('dfs')} | {data['bfs']['samples']} |")
        lines += ["", "Medições da nova rodada; não misturar diretamente com os tempos da rodada anterior de Dijkstra.", "", "Tabelas: [resumo detalhado](results/traversals/traversal_summary.csv), [comparação pareada](results/traversals/results_traversals_paired.csv). Consulte [a análise da execução](AZURE_TRAVERSALS.md) para duração, validações e diferenças entre componentes.", ""]
    else:
        lines += ["Ainda não há resultados da nova rodada. Os scripts usam quantidades de repetições ajustadas ao custo, com origens aleatórias distintas e a mesma sequência entre as duas buscas. A comparação CSR/matriz utiliza somente o prefixo comum de origens. Consulte a [estimativa de duração](TRAVERSAL_ESTIMATE.md). Não há tempos locais apresentados como Azure. Consulte [AZURE_TRAVERSALS.md](AZURE_TRAVERSALS.md).", ""]
    lines += ["## Matrizes com bitmask — Parte 1", "", "A alocação considera o arredondamento de cada linha para palavras de 64 bits, incluindo a linha e coluna zero. O limite é 28 GiB (30,06 GB decimais), com orçamento total do processo também limitado a 28 GiB para reservar aproximadamente 2 a 4 GiB na VM de 32 GiB.", "", "| Grafo | Matriz (GiB) | Rodar matriz? |", "| --- | ---: | --- |"]
    for graph in model["graphs"]:
        if graph["weighting"] == "unweighted":
            n = int(graph["summary"]["vertices"])
            size = (n+1)*((n+64)//64)*8/1024**3
            lines.append(f"| {graph_name(graph)} | {size:.4f} | {'Sim' if size < 28 else 'Não: excede o limite'} |")
    lines += ["", "## Repetições planejadas por busca", "", "| Grafo | CSR | Matriz |", "| --- | ---: | ---: |"]
    for graph in model["graphs"]:
        plan = model["traversal_plan"][graph["id"]]
        lines.append(f"| {graph_name(graph)} | {plan.get('k_csr') or 100} | {plan.get('k_matrix') or 'não executada'} |")
    lines += ["", "O plano considera tamanho, arestas, componentes e armazenamento. Veja [TRAVERSAL_ESTIMATE.md](TRAVERSAL_ESTIMATE.md) para as faixas de duração e limitações. O pré-teste de memória inclui também a construção e as buscas.", ""]
    lines += ["## Casos artificiais", "", "Consulte os [casos de referência](BASELINES.md), com resultados esperados calculados manualmente e comparados às implementações.", ""]
    gallery.parent.mkdir(parents=True, exist_ok=True)
    gallery.write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name, default in [
        ("collected", ROOT / "data/collected"),
        ("azure", ROOT / "data/azure"),
        ("traversals", ROOT / "data/azure_traversals"),
        ("traversal-config", ROOT / "config_traversals.csv"),
        ("data", ROOT / "data"),
        ("config", ROOT / "config_collect.csv"),
        ("out", ROOT / "docs/figures"),
        ("gallery", ROOT / "docs/VISUALIZATIONS.md"),
    ]:
        parser.add_argument("--" + name, type=Path, default=default)
    parser.add_argument(
        "--skip-networks",
        action="store_true",
        help="Regenerate statistical figures only",
    )
    args = parser.parse_args()
    model = load_data(args.collected, args.azure, args.data, args.config)
    model["traversal_plan"] = {r["graph"]: r for r in read_rows(args.traversal_config)}
    model["traversals_paired_csr"] = azure_data(args.traversals, model["graphs"], filename="results_traversals_paired.csv", algorithms=("bfs", "dfs"), environment_file="environment_traversals.txt")
    model["traversals_paired_matrix"] = azure_data(args.traversals, model["graphs"], filename="results_traversals_paired.csv", algorithms=("bfs", "dfs"), environment_file="environment_traversals.txt", representation="matrix")
    model["traversals"] = azure_data(args.traversals, model["graphs"], filename="results_traversals.csv", algorithms=("bfs", "dfs"), environment_file="environment_traversals.txt")
    model["traversals_matrix"] = azure_data(args.traversals, model["graphs"], filename="results_traversals.csv", algorithms=("bfs", "dfs"), environment_file="environment_traversals.txt", representation="matrix")
    if model["traversals"]["available"]:
        for graph in model["graphs"]:
            input_record = json.loads((args.traversals / "collected_traversals" / graph["id"] / "input.json").read_text())
            if input_record["sha256_plaintext"] != graph["provenance"]["input_sha256"]:
                raise ValueError("Azure traversal input differs from statistics input")
            for algorithm in ("bfs", "dfs"):
                result = model["traversals"]["graphs"][graph["id"]][algorithm]
                if result["status"] != "ok" or result["samples"] != int(model["traversal_plan"][graph["id"]].get("k_csr") or 100):
                    raise ValueError("Azure BFS/DFS chart requires the planned successful CSR sample count")
                if graph["id"] in {"grafo_1", "grafo_2", "grafo_3", "grafo_4"}:
                    result = model["traversals_matrix"]["graphs"][graph["id"]][algorithm]
                    if result["status"] != "ok" or result["samples"] != int(model["traversal_plan"][graph["id"]].get("k_matrix") or 100):
                        raise ValueError("eligible Part 1 matrix requires the planned successful sample count")
    args.out.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "font.size": 10,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "svg.fonttype": "none",
            "svg.hashsalt": "cos242",
            "axes.titlepad": 12,
        }
    )
    records = []
    for weighting, part in (("unweighted", "parte1"), ("weighted", "parte2")):
        graphs = [g for g in model["graphs"] if g["weighting"] == weighting]
        if not graphs:
            continue
        directory = args.out / part
        directory.mkdir(parents=True, exist_ok=True)
        section_records = []
        statistical_figures({**model, "graphs": graphs}, directory, section_records)
        records.extend({**r, "name": part + "/" + r["name"]} for r in section_records)
    traversal_figures(model, args.out, records)
    if not args.skip_networks:
        network_figures(model, args.out, records)
    write_gallery(model, args.out, records, args.gallery)
    versions = {
        "matplotlib": matplotlib.__version__,
        "numpy": np.__version__,
        "networkx": nx.__version__,
        "scipy": scipy.__version__,
        "python": sys.version,
    }
    provenance = {
        "graphs": {g["id"]: g["provenance"] for g in model["graphs"]},
        "plotting_versions": versions,
        "network_layout": {
            "seed": 2025,
            "iterations": 45,
            "method": "force",
            "weight": None,
        },
        "azure_commit": model["azure"].get("commit"),
        "figures": records,
        "sha256": {
            f"{r['name']}.{ext}": hashlib.sha256(
                (args.out / f"{r['name']}.{ext}").read_bytes()
            ).hexdigest()
            for r in records
            for ext in ("svg", "png")
        },
    }
    provenance["script_sha256"] = {
        name: hashlib.sha256((ROOT / "analysis" / name).read_bytes()).hexdigest()
        for name in ("plot_graphs.py", "visualization_data.py")
    }
    for name, path in [
        ("config", args.config),
        ("traversal_config", args.traversal_config),
        ("traversal_forecast", ROOT / "docs/traversal_forecast.json"),
        ("azure_results", args.azure / "results_weighted.csv"),
        ("azure_environment", args.azure / "environment.txt"),
        ("azure_paths", args.azure / "paths_weighted.csv"),
        ("traversal_results", args.traversals / "results_traversals.csv"),
        ("traversal_paired_results", args.traversals / "results_traversals_paired.csv"),
        ("traversal_summary", args.traversals / "traversal_summary.csv"),
        ("traversal_environment", args.traversals / "environment_traversals.txt"),
        ("traversal_source_manifest", args.traversals / "source_manifest.json"),
    ]:
        if path.is_file():
            provenance[name + "_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    (args.out / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"Gallery: {args.gallery}")


if __name__ == "__main__":
    main()
