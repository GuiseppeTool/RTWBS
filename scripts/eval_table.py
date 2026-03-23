#!/usr/bin/env python3
"""
Reads all UPPAAL XML files from assets/eval/ and generates a LaTeX table
summarising each benchmark instance:
  - Number of components (templates)
  - States per component
  - Transitions per component
  - Number of clocks per component
  - Number of synchronisation sends (!) per component
  - Number of synchronisation receives (?) per component
"""

import csv
import math
import os
import re
import xml.etree.ElementTree as ET
from pathlib import Path


EVAL_DIR = Path(__file__).resolve().parent.parent / "assets" / "syn_eval"
SIZE_CSV = Path(__file__).resolve().parent.parent / "assets" / "system_size.csv"
ZONE_STATS_CSV = Path(__file__).resolve().parent.parent / "assets" / "zone_stats.csv"


def load_system_sizes() -> dict[str, int]:
    """Load system_size.csv and return a dict mapping filename stem to system_size."""
    sizes: dict[str, int] = {}
    with open(SIZE_CSV, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            sizes[row["name"].strip()] = int(row["system_size"].strip())
    return sizes


def load_zone_stats() -> dict[str, dict]:
    """Load zone_stats.csv and return a dict mapping name to its row data."""
    stats: dict[str, dict] = {}
    with open(ZONE_STATS_CSV, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row["name"].strip()
            stats[name] = {
                "total_zones": int(row["total_zones"]),
                "max_constant": int(row["max_constant"]),
            }
    return stats


def fmt_system_size(n: int) -> str:
    """Format a system size as $\\sim 3.36 \\times 10^{3}$ style LaTeX."""
    if n == 0:
        return "$0$"
    exp = int(math.floor(math.log10(n)))
    mantissa = n / (10 ** exp)
    return f"$\\sim${mantissa:.2f}$\\times$10$^{{{exp}}}$"

# Ordering key: size category then numeric suffix
SIZE_ORDER = {"s": 0, "m": 1, "l": 2, "xl": 3, "xxl": 4}

# Human-readable size names
SIZE_LABELS = {"s": "Small", "m": "Medium", "l": "Large", "xl": "Extra Large", "xxl": "Extra Extra Large"}


def sort_key(filename: str):
    """Return a tuple (size_rank, number) for natural ordering."""
    stem = Path(filename).stem  # e.g. "xl_5"
    parts = stem.rsplit("_", 1)
    size = parts[0].lower()
    num = int(parts[1]) if len(parts) > 1 else 0
    return (SIZE_ORDER.get(size, 99), num)


def bench_label(filename: str) -> str:
    """Return a human-readable benchmark label, e.g. 'Small (3 Comp)'."""
    stem = Path(filename).stem
    parts = stem.rsplit("_", 1)
    size = parts[0].lower()
    num = int(parts[1]) if len(parts) > 1 else 1
    label = SIZE_LABELS.get(size, stem)
    if num > 1:
        label += f" ({num} Comp)"
    return label


def parse_clocks(declaration_text: str) -> list[str]:
    """Extract clock names from a UPPAAL declaration string."""
    clocks = []
    for m in re.finditer(r"clock\s+([\w\s,]+);", declaration_text):
        clocks.extend(c.strip() for c in m.group(1).split(",") if c.strip())
    return clocks


def parse_file(filepath: str) -> dict:
    """Parse a single UPPAAL XML file and return benchmark info."""
    tree = ET.parse(filepath)
    root = tree.getroot()

    # --- Global channels --------------------------------------------------
    global_decl = root.findtext("declaration") or ""
    channels: set[str] = set()
    for m in re.finditer(r"chan\s+([\w\s,]+);", global_decl):
        channels.update(c.strip() for c in m.group(1).split(",") if c.strip())

    # --- Per-template info ------------------------------------------------
    components = []
    for tmpl in root.iter("template"):
        name = tmpl.findtext("name") or "?"
        decl = tmpl.findtext("declaration") or ""

        clocks = parse_clocks(decl)
        locations = list(tmpl.iter("location"))
        transitions = list(tmpl.iter("transition"))

        send_chans: set[str] = set()
        recv_chans: set[str] = set()

        for trans in transitions:
            for label in trans.findall("label"):
                if label.get("kind") == "synchronisation" and label.text:
                    txt = label.text.strip()
                    if txt.endswith("!"):
                        send_chans.add(txt[:-1])
                    elif txt.endswith("?"):
                        recv_chans.add(txt[:-1])

        components.append(
            {
                "name": name,
                "states": len(locations),
                "transitions": len(transitions),
                "n_clocks": len(clocks),
                "n_send": len(send_chans),
                "n_recv": len(recv_chans),
            }
        )

    return {
        "file": os.path.basename(filepath),
        "channels": sorted(channels),
        "components": components,
    }


def generate_latex(benchmarks: list[dict], sys_sizes: dict[str, int], zone_stats: dict[str, dict]) -> str:
    """Generate a LaTeX table string from parsed benchmark data."""
    lines: list[str] = []

    lines.append(r"\begin{table}[t]")
    lines.append(r"  \centering")
    lines.append(r"  \caption{Summary of evaluation benchmarks.}")
    lines.append(r"  \label{tab:eval_benchmarks}")
    lines.append(r"  \resizebox{\columnwidth}{!}{%")
    lines.append(r"  \begin{tabular}{lp{1cm}cccccccc}")
    lines.append(r"    \toprule")
    lines.append(
        r"    \textbf{Configuration} "
        #r"& \textbf{\#\,Components} "
        #r"& \textbf{System Size}"
        r" & \textbf{Locations} "
        #r"& \textbf{Transitions} "
        
        r"& $\boldsymbol{\mu_O}$ & $\boldsymbol{\mu_I}$ "
        r"& $\boldsymbol{\tau}$\textbf{-trans.} "
        r"& \textbf{Clocks} "
        r"& \textbf{Zones} & $\boldsymbol{c_{\max}}$ \\"
    )
    lines.append(r"    \midrule")

    for bench in benchmarks:
        label = bench_label(bench["file"])
        stem = Path(bench["file"]).stem
        n_comp = len(bench["components"])
        total_states = sum(c["states"] for c in bench["components"])
        total_trans = sum(c["transitions"] for c in bench["components"])
        total_clocks = sum(c["n_clocks"] for c in bench["components"])
        total_send = sum(c["n_send"] for c in bench["components"])
        total_recv = sum(c["n_recv"] for c in bench["components"])
        tau_trans = total_trans - total_send - total_recv

        zs = zone_stats.get(stem, {})
        total_zones = zs.get("total_zones", "--")
        max_const = zs.get("max_constant", "--")

        size_val = sys_sizes.get(stem)
        size_str = fmt_system_size(size_val) if size_val is not None else "--"
        print(stem, size_str)
        row = (
            f"    {label}   "
            #f"& {size_str} "
            f"& {total_states}"
            #f"& {total_trans} "
            
            f"& {total_send} "
            f"& {total_recv} "
            f"& {tau_trans} "

            f"& {total_clocks} "
            f"& {total_zones} & {max_const} \\\\"
        )
        lines.append(row)

    lines.append(r"    \bottomrule")
    lines.append(r"  \end{tabular}%")
    lines.append(r"  }")
    lines.append(r"\end{table}")

    return "\n".join(lines)


def main():
    xml_files = sorted(EVAL_DIR.glob("*.xml"), key=lambda p: sort_key(p.name))

    if not xml_files:
        print(f"No XML files found in {EVAL_DIR}")
        return

    benchmarks = [parse_file(str(f)) for f in xml_files]
    sys_sizes = load_system_sizes()
    zone_stats = load_zone_stats()
    latex = generate_latex(benchmarks, sys_sizes, zone_stats)
    print(latex)

    # Also write to a .tex file next to this script
    out_path = Path(__file__).resolve().parent / "eval_table.tex"
    out_path.write_text(latex + "\n", encoding="utf-8")
    print(f"\nTable written to {out_path}")


if __name__ == "__main__":
    main()
