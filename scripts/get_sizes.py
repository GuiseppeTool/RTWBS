import os
import re
import subprocess
import csv

EVAL_DIR = "assets/eval"
PARSE_BIN = "release/examples/parse_generated_benchmark"
OUTPUT_CSV = "assets/system_size.csv"
MAX_AUTOMATA = 5

def parse_stats(output):
    # Extract total zones and transitions
    tot_zones = int(re.search(r"Total zones: (\d+)", output).group(1))
    tot_trans = int(re.search(r"Total zone graph transitions: (\d+)", output).group(1))
    # Extract per-automaton stats
    automata_stats = []
    for i in range(MAX_AUTOMATA):
        match = re.search(
            rf"--- Automaton {i}:.*?Number of zones: (\d+).*?Number of zone graph transitions: (\d+)",
            output, re.DOTALL)
        if match:
            zones = int(match.group(1))
            trans = int(match.group(2))
            automata_stats.append(f"{zones},{trans}")
        else:
            automata_stats.append("None")
    # Extract number of automata
    comp_match = re.search(r"Number of Automata: (\d+)", output)
    comp = int(comp_match.group(1)) if comp_match else len([a for a in automata_stats if a != "None"])
    return tot_zones, tot_trans, automata_stats, comp

def main():
    rows = []
    for fname in os.listdir(EVAL_DIR):
        if not fname.endswith(".xml"):
            continue
        path = os.path.join(EVAL_DIR, fname)
        result = subprocess.run([PARSE_BIN, path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        output = result.stdout
        tot_zones, tot_trans, automata_stats, comp = parse_stats(output)
        system_size = tot_zones * tot_trans
        row = [fname.replace('.xml', ''), system_size, tot_zones, tot_trans] + automata_stats + [comp]
        rows.append(row)

    header = ["name", "system_size", "tot_states", "tot_transitions"] + \
             [f"automaton{i}" for i in range(MAX_AUTOMATA)] + ["comp"]
    with open(OUTPUT_CSV, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

if __name__ == "__main__":
    main()