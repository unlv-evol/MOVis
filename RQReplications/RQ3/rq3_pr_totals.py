#!/usr/bin/env python3
"""RQ3 step 1: total PRs and PRs per classification, for GACPD and PaReco.

Same rules as MOVis scripts/aggregate_repo_results.py (PR level):
  * windows are grouped into datasets (unbroken run of date windows per run + repo)
  * a repo listed twice becomes two datasets, never merged
  * each PR is counted once per dataset
  * SP is counted as MO ("NA ADDED" as NA)

GACPD is read from Results/Repos_results/*/classification_audit.csv,
PaReco from Results/PaReco_Results/*.pkl (see traverse_pareco_pickles()).

Usage:
  python RQ3/rq3_pr_totals.py [--results RESULTS_DIR] [--out OUT_DIR] [--tool GACPD|PaReco|both]
"""

from __future__ import annotations

import argparse
import sys
from collections import Counter
from pathlib import Path

import rq3_common as c


def traverse_pareco_pickles(pareco_dir: Path) -> dict[str, dict[str, str]]:
    """Walk every PaReco *.pkl and return {pickle name: {PR: class (SP -> MO)}}.

    Stand-alone view of the pickles; the totals below use the same reader through
    rq3_common.scan(), which also pairs each pickle with its date window.
    """
    out = {}
    for pkl in sorted(Path(pareco_dir).glob("*_results.pkl"), key=lambda p: int(p.name.split("_")[0])):
        data = c.load_pareco_pickle(pkl)
        out[pkl.name] = {pr: c.normalize(cls) for pr, cls in c.pareco_pr_classes(data).items()}
    return out


def totals_rows(tool: str, datasets: list[c.Dataset]):
    per_ds = {d.name: c.counter_of(d.pr_classes(tool)) for d in datasets}
    grand = Counter()
    for counts in per_ds.values():
        grand.update(counts)
    cols = c.order_labels(set(grand) | {"MO", "ED", "NA"})
    rows = []
    for d in datasets:
        counts = per_ds[d.name]
        # Where the numbers come from: GACPD folders, or the PaReco pickles paired with them.
        if tool == c.PARECO:
            sources = [w.pareco_pickle.name for w in d.windows if w.pareco_pickle]
        else:
            sources = [w.name for w in d.windows if w.prs.get(c.GACPD)]
        rows.append([d.name, d.run, d.repo, d.start.isoformat(), d.end.isoformat(), len(d.windows),
                     d.windows[0].idx, d.windows[-1].idx]
                    + [counts.get(k, 0) for k in cols] + [sum(counts.values()), len(sources), ";".join(sources)])
    total = sum(grand.values())
    n_src = sum(r[-2] for r in rows)
    all_row = ["ALL", "", "", "", "", sum(len(d.windows) for d in datasets), "", ""] \
        + [grand.get(k, 0) for k in cols] + [total, n_src, ""]
    pct_row = ["ALL %", "", "", "", "", "", "", ""] + [c.pct(grand.get(k, 0), total) for k in cols] + ["100.00", "", ""]
    src_name = "pickles" if tool == c.PARECO else "folders"
    header = ["dataset", "run", "repo", "start", "end", "windows", "first_#", "last_#"] + cols \
        + ["TOTAL", f"{tool} {src_name}", f"{tool} {src_name} used"]
    return header, rows, all_row, pct_row, grand, cols


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", default=str(c.DEFAULT_RESULTS), help="Results folder (default: ../Results)")
    ap.add_argument("--out", default=str(c.DEFAULT_OUT / "pr_totals"), help="Output folder for CSVs")
    ap.add_argument("--tool", choices=["GACPD", "PaReco", "both"], default="both")
    ap.add_argument("--show-datasets", action="store_true", help="Print the per-dataset table too")
    args = ap.parse_args()

    tools = c.TOOLS if args.tool == "both" else (args.tool,)
    datasets, warnings = c.load_datasets(Path(args.results), tools=tools)
    out_dir = Path(args.out)

    print(f"{len(datasets)} datasets, {sum(len(d.windows) for d in datasets)} windows")
    split = Counter((d.run, d.repo) for d in datasets)
    for d in datasets:
        if split[(d.run, d.repo)] > 1:
            print(f"  listed more than once (kept separate): {d.name} (#{d.windows[0].idx}-#{d.windows[-1].idx})")

    summary = []
    for tool in tools:
        header, rows, all_row, pct_row, grand, cols = totals_rows(tool, datasets)
        path = c.write_csv(out_dir / f"pr_totals_{tool}.csv", header, rows + [all_row, pct_row])
        total = sum(grand.values())
        print(f"\n=== {tool}: PR classification totals (SP counted as MO) ===")
        if args.show_datasets:
            short = [[r[0], r[5]] + r[8:-2] for r in rows if r[-3]]
            print(c.format_table(["dataset", "win"] + cols + ["TOTAL"], short))
            print()
        print(c.format_table(["class", "PRs", "%"],
                             [[k, grand.get(k, 0), c.pct(grand.get(k, 0), total)] for k in cols]
                             + [["TOTAL", total, "100.00"]]))
        print(f"wrote {path}")
        dups = sum(len(d.duplicate_prs(tool)) for d in datasets)
        if dups:
            warnings.append(f"{tool}: {dups} PR(s) appeared in more than one window of a dataset; counted once")
        summary.append((tool, grand, total))

    if len(summary) == 2:
        cols = c.order_labels(set(summary[0][1]) | set(summary[1][1]))
        rows = [[k] + sum(([g.get(k, 0), c.pct(g.get(k, 0), t)] for _, g, t in summary), []) for k in cols]
        rows.append(["TOTAL"] + sum(([t, "100.00"] for _, _, t in summary), []))
        header = ["class"] + sum(([f"{tool} PRs", f"{tool} %"] for tool, _, _ in summary), [])
        print("\n=== Side by side ===")
        print(c.format_table(header, rows))
        path = c.write_csv(out_dir / "pr_totals_side_by_side.csv", header, rows)
        (out_dir / "pr_totals_side_by_side.md").write_text(c.markdown_table(header, rows) + "\n", encoding="utf-8")
        print(f"wrote {path}")

    c.print_warnings(warnings)
    return 0


if __name__ == "__main__":
    sys.exit(main())
