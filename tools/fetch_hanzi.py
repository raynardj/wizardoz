#!/usr/bin/env python3
"""Extract unique CJK characters from HanziList CSV for hanzi_frequency.csv.

Usage (from project root, use poetry in server folder):
    cd server && poetry run python ../tools/fetch_hanzi.py
"""
import csv
import urllib.request
from pathlib import Path

URL = "https://raw.githubusercontent.com/yannickmahe/HanziList/master/hanzi.csv"
OUTPUT_RELPATH = "tools/data/hanzi_frequency.csv"


def main():
    with urllib.request.urlopen(URL) as f:
        reader = csv.DictReader(f.read().decode("utf-8").splitlines())
        seen = {}
        for row in reader:
            rank = int(row["rank"])
            ch = row["char"]
            if len(ch) != 1:
                continue
            if 0x4E00 <= ord(ch) <= 0x9FFF and ch not in seen:
                seen[ch] = rank
    items = [(r, c) for c, r in seen.items()]
    items.sort(key=lambda x: (x[0], x[1]))
    out = [(i, c) for i, (_, c) in enumerate(items[:3500], 1)]

    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    output_path = project_root / OUTPUT_RELPATH
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        for rank, ch in out:
            f.write(f"{rank},{ch}\n")
    print(f"Wrote {len(out)} characters to {output_path}")


if __name__ == "__main__":
    main()
