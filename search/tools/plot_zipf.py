#!/usr/bin/env python3
"""Render the complete C++ frequency export; no counting, sorting or fitting here."""

import argparse
import csv
import gzip
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


class ZipfPlot:
    def Render(self, table: Path, metrics: Path, output: Path) -> None:
        opener = gzip.open if table.suffix == ".gz" else open
        with opener(table, "rt", encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source, delimiter="\t"))
        report = json.loads(metrics.read_text(encoding="utf-8"))
        if not rows:
            raise ValueError("Cannot plot an empty term distribution")

        ranks = [int(row["rank"]) for row in rows]
        observed = [int(row["frequency"]) for row in rows]
        figure, axes = plt.subplots(figsize=(10, 6.4), layout="constrained")
        axes.loglog(ranks, observed, color="#204b38", linewidth=1.5, label="Корпус: число вхождений")
        axes.loglog(ranks, [float(row["zipf"]) for row in rows], color="#b06427", linewidth=1.6,
                    linestyle="--", label="Ципф: C / r")
        if report["mandelbrot"]:
            axes.loglog(ranks, [float(row["mandelbrot"]) for row in rows], color="#466ca5", linewidth=1.6,
                        label="Мандельброт: C / (r + q)^s")
        axes.set(xlabel="Ранг терма (логарифмическая шкала)",
                 ylabel="Количество вхождений (логарифмическая шкала)",
                 title=f"Частоты терминов корпуса: {report['terms']:,} термов, {report['tokens']:,} вхождений")
        axes.grid(which="major", color="#d9dfd5", linewidth=0.7)
        axes.legend(loc="lower left")
        figure.patch.set_facecolor("#fffefa")
        axes.set_facecolor("#fffefa")
        output.parent.mkdir(parents=True, exist_ok=True)
        figure.savefig(output.with_suffix(".svg"))
        figure.savefig(output.with_suffix(".png"), dpi=180)
        plt.close(figure)
        print(f"Rendered {len(rows)} complete ranks with Matplotlib {matplotlib.__version__}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("table", type=Path)
    parser.add_argument("metrics", type=Path)
    parser.add_argument("output", type=Path, help="Output path without .svg/.png suffix")
    args = parser.parse_args()
    ZipfPlot().Render(args.table, args.metrics, args.output)


if __name__ == "__main__":
    main()
