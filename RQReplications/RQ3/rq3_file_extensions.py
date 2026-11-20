#!/usr/bin/env python3
"""RQ3 step 2: every unique file extension and how many files have it, per tool.

GACPD files come from Results/Repos_results/*/file_audit.csv, PaReco files from the
per-PR file lists in Results/PaReco_Results/*.pkl. Files are counted under the same
dataset rule as the PR totals: a PR that appears in several windows of one dataset
has its files counted once. Extensions are lower-cased; files without one are
listed as "(no extension)".

Usage:
  python RQ3/rq3_file_extensions.py [--results DIR] [--out DIR] [--tool GACPD|PaReco|both]
                                    [--by-class] [--top N]
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

import rq3_common as c

FILE_CLASS_ALIASES = {"SP": "MO", "NA ADDED": "NA", "OTHER EXT": "CC", "NOT EXISTING": "NE", "EE": "ERROR"}


def file_class(raw: str) -> str:
    label = (raw or "").strip().upper()
    return FILE_CLASS_ALIASES.get(label, label) or "(blank)"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", default=str(c.DEFAULT_RESULTS))
    ap.add_argument("--out", default=str(c.DEFAULT_OUT / "file_extensions"))
    ap.add_argument("--tool", choices=["GACPD", "PaReco", "both"], default="both")
    ap.add_argument("--by-class", action="store_true",
                    help="Also write extension x file-classification tables (SP->MO, OTHER EXT->CC, NOT EXISTING->NE)")
    ap.add_argument("--top", type=int, default=30, help="Rows to print (all rows are written to CSV)")
    args = ap.parse_args()

    tools = c.TOOLS if args.tool == "both" else (args.tool,)
    datasets, warnings = c.load_datasets(Path(args.results), tools=tools, with_files=True)
    out_dir = Path(args.out)

    ext_counts: dict[str, Counter] = {t: Counter() for t in tools}
    ext_by_class: dict[str, dict[str, Counter]] = {t: defaultdict(Counter) for t in tools}
    for d in datasets:
        for tool in tools:
            for _pr, name, cls in d.file_rows(tool):
                ext = c.file_extension(name)
                ext_counts[tool][ext] += 1
                ext_by_class[tool][ext][file_class(cls)] += 1

    all_exts = sorted(set().union(*(ext_counts[t].keys() for t in tools)),
                      key=lambda e: (-sum(ext_counts[t][e] for t in tools), e))
    header = ["extension"] + sum(([f"{t} files", f"{t} %"] for t in tools), [])
    totals = {t: sum(ext_counts[t].values()) for t in tools}
    rows = [[e] + sum(([ext_counts[t][e], c.pct(ext_counts[t][e], totals[t])] for t in tools), [])
            for e in all_exts]
    total_row = ["TOTAL"] + sum(([totals[t], "100.00"] for t in tools), [])

    path = c.write_csv(out_dir / "file_extensions.csv", header, rows + [total_row])
    with (out_dir / "file_extensions.json").open("w", encoding="utf-8") as fh:
        json.dump({t: dict(ext_counts[t].most_common()) for t in tools}, fh, indent=4, ensure_ascii=False)

    print(f"\n{len(all_exts)} unique extensions  "
          + "  ".join(f"{t}: {len(ext_counts[t])} extensions / {totals[t]} files" for t in tools))
    shown = rows[: args.top] if args.top else rows
    print(c.format_table(header, shown + [total_row]))
    if args.top and len(rows) > args.top:
        print(f"... {len(rows) - args.top} more in the CSV")
    print(f"\nwrote {path}")
    print(f"wrote {out_dir / 'file_extensions.json'}")

    if args.by_class:
        for tool in tools:
            classes = sorted({k for ctr in ext_by_class[tool].values() for k in ctr},
                             key=lambda k: (["MO", "ED", "CC", "NE", "NA", "ERROR"] + [k]).index(k))
            exts = sorted(ext_by_class[tool], key=lambda e: -ext_counts[tool][e])
            p = c.write_csv(out_dir / f"file_extensions_by_class_{tool}.csv",
                            ["extension"] + classes + ["TOTAL"],
                            [[e] + [ext_by_class[tool][e].get(k, 0) for k in classes] + [ext_counts[tool][e]]
                             for e in exts])
            print(f"wrote {p}")

    c.print_warnings(warnings)
    return 0


if __name__ == "__main__":
    sys.exit(main())
