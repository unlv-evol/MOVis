#!/usr/bin/env python3
"""RQ3 step 3: how PR classifications moved between the two tools.

For every PR classified by both tools (same dataset, same PR number; SP counted as MO)
builds a transition matrix: rows = PaReco class, columns = GACPD class. Use --from to
flip the direction.

Outputs (RQ3/output/transitions/):
  transition_matrix.csv / .md / .tex   counts
  transition_matrix_row_pct.csv        each row as % of that PaReco class
  transitions_long.csv                 one line per from -> to pair, with counts and %
  transitions_per_dataset.csv          same per dataset, with the PR numbers of each pair
  transitions_prs.csv                  one line per PR (PR number, GitHub link, both classes,
                                       GACPD PR folder, PaReco pickle) for manual checking
  transitions_prs.json                 the same PRs grouped by "PaReco X -> GACPD Y"
  prs_only_in_one_tool.csv             PRs that only one of the tools classified

--show FROM:TO prints the PRs behind one cell of the matrix, e.g. --show CC:NE
(classes in the --from tool first).

Usage:
  python RQ3/rq3_classification_transitions.py [--results DIR] [--out DIR] [--from PaReco|GACPD]
                                               [--dataset TEXT] [--show FROM:TO]
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

import rq3_common as c


def pr_link(repo: str | None, pr: str) -> str:
    return f"https://github.com/{repo}/pull/{pr}" if repo and pr.isdigit() else ""


class GacpdFolders:
    """Finds the <PR>_<CLASS> folder GACPD writes for each PR inside a window."""

    def __init__(self, windows: dict[str, c.Window]):
        self.windows = windows
        self.cache: dict[str, dict[str, str]] = {}

    def get(self, window_name: str, pr: str) -> str:
        if window_name not in self.cache:
            w = self.windows.get(window_name)
            found = {}
            if w is not None and w.path.is_dir():
                for sub in w.path.iterdir():
                    if not sub.is_dir():
                        continue
                    m = c.PR_DIR_RE.match(sub.name)
                    pr_no = m["pr"] if m else (sub.name if sub.name.isdigit() else None)  # "<PR>_<CLASS>" or "<PR>"
                    if pr_no:
                        found[pr_no] = f"{c.GACPD_DIR}/{w.name}/{sub.name}"
            self.cache[window_name] = found
        return self.cache[window_name].get(pr, "")


def latex_matrix(row_tool: str, col_tool: str, labels: list[str], matrix: Counter) -> str:
    cols = "l" + "r" * (len(labels) + 1)
    lines = [
        "\\begin{table}[ht]",
        "\\centering",
        f"\\caption{{PR classification transitions from {row_tool} (rows) to {col_tool} (columns).}}",
        "\\label{tab:rq3-transitions}",
        f"\\begin{{tabular}}{{{cols}}}",
        "\\hline",
        f"{row_tool} $\\downarrow$ / {col_tool} $\\rightarrow$ & " + " & ".join(labels) + " & Total \\\\",
        "\\hline",
    ]
    for r in labels:
        vals = [matrix[(r, k)] for k in labels]
        cells = [f"\\textbf{{{v}}}" if r == k else str(v) for v, k in zip(vals, labels)]
        lines.append(f"{r} & " + " & ".join(cells) + f" & {sum(vals)} \\\\")
    lines.append("\\hline")
    col_tot = [sum(matrix[(r, k)] for r in labels) for k in labels]
    lines.append("Total & " + " & ".join(map(str, col_tot)) + f" & {sum(col_tot)} \\\\")
    lines += ["\\hline", "\\end{tabular}", "\\end{table}"]
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", default=str(c.DEFAULT_RESULTS))
    ap.add_argument("--out", default=str(c.DEFAULT_OUT / "transitions"))
    ap.add_argument("--from", dest="from_tool", choices=list(c.TOOLS), default=c.PARECO,
                    help="Tool on the rows (default: PaReco, so the table reads PaReco -> GACPD)")
    ap.add_argument("--dataset", help="Only datasets whose name contains this text")
    ap.add_argument("--show", metavar="FROM:TO", help="Print the PRs of one cell, e.g. CC:NE")
    args = ap.parse_args()
    show = None
    if args.show:
        parts = [c.normalize(p) for p in args.show.split(":")]
        if len(parts) != 2:
            print("--show takes FROM:TO, e.g. --show CC:NE")
            return 1
        show = tuple(parts)

    row_tool = args.from_tool
    col_tool = c.GACPD if row_tool == c.PARECO else c.PARECO
    datasets, warnings = c.load_datasets(Path(args.results))
    if args.dataset:
        datasets = [d for d in datasets if args.dataset.lower() in d.name.lower()]
        if not datasets:
            print(f"No dataset name contains '{args.dataset}'")
            return 1
    out_dir = Path(args.out)

    windows = {w.name: w for d in datasets for w in d.windows}
    gacpd_dirs = GacpdFolders(windows)

    def pickle_of(window_name: str) -> str:
        w = windows.get(window_name)
        return w.pareco_pickle.name if w is not None and w.pareco_pickle else ""

    matrix: Counter = Counter()
    per_dataset_rows, only_one, pr_rows = [], [], []
    grouped: dict[str, list[dict]] = defaultdict(list)
    for d in datasets:
        a, b = d.pr_classes(row_tool), d.pr_classes(col_tool)
        local: dict[tuple[str, str], list[str]] = defaultdict(list)
        source_repo = d.source_repo
        for pr in sorted(set(a) | set(b), key=lambda s: int(s) if s.isdigit() else 0):
            link = pr_link(source_repo, pr)
            if pr in a and pr in b:
                f, t = a[pr][0], b[pr][0]
                local[(f, t)].append(pr)
                win = {row_tool: a[pr][1], col_tool: b[pr][1]}
                status = "same" if f == t else "moved"
                row = {
                    "dataset": d.name, "PR": pr, "link": link,
                    f"{row_tool} class": f, f"{col_tool} class": t, "status": status,
                    "window": win[c.GACPD],
                    "GACPD PR folder": gacpd_dirs.get(win[c.GACPD], pr),
                    "PaReco pickle": pickle_of(win[c.PARECO]),
                }
                pr_rows.append(row)
                grouped[f"{row_tool} {f} -> {col_tool} {t}"].append(row)
            else:
                tool, (cls, win) = (row_tool, a[pr]) if pr in a else (col_tool, b[pr])
                only_one.append([d.name, win, pr, link, tool, cls])
        for (f, t), prs in local.items():
            matrix[(f, t)] += len(prs)
        for (f, t), prs in sorted(local.items()):
            per_dataset_rows.append([d.name, f, t, len(prs), "same" if f == t else "moved", ";".join(prs)])

    labels = c.order_labels({k for pair in matrix for k in pair})
    total = sum(matrix.values())
    same = sum(n for (f, t), n in matrix.items() if f == t)

    header = [f"{row_tool} \\ {col_tool}"] + labels + ["TOTAL"]
    rows = []
    for r in labels:
        vals = [matrix[(r, k)] for k in labels]
        rows.append([r] + vals + [sum(vals)])
    col_tot = [sum(matrix[(r, k)] for r in labels) for k in labels]
    rows.append(["TOTAL"] + col_tot + [total])

    pct_rows = []
    for r in labels:
        rt = sum(matrix[(r, k)] for k in labels)
        pct_rows.append([r] + [c.pct(matrix[(r, k)], rt) for k in labels] + [rt])

    long_rows = []
    for (f, t), n in sorted(matrix.items(), key=lambda kv: -kv[1]):
        ft = sum(matrix[(f, k)] for k in labels)
        long_rows.append([f, t, n, c.pct(n, ft), c.pct(n, total), "same" if f == t else "moved"])

    c.write_csv(out_dir / "transition_matrix.csv", header, rows)
    c.write_csv(out_dir / "transition_matrix_row_pct.csv", header[:-1] + [f"{row_tool} total"], pct_rows)
    c.write_csv(out_dir / "transitions_long.csv",
                [f"{row_tool} class", f"{col_tool} class", "PRs", f"% of {row_tool} class", "% of all", "status"],
                long_rows)
    c.write_csv(out_dir / "transitions_per_dataset.csv",
                ["dataset", f"{row_tool} class", f"{col_tool} class", "PRs", "status", "PR numbers"],
                per_dataset_rows)

    # One line per PR: moved PRs first, then by the size of their matrix cell, dataset, PR number.
    order = {lab: i for i, lab in enumerate(labels)}
    pr_rows.sort(key=lambda r: (r["status"] != "moved", order.get(r[f"{row_tool} class"], 99),
                                order.get(r[f"{col_tool} class"], 99), r["dataset"],
                                int(r["PR"]) if r["PR"].isdigit() else 0))
    pr_cols = ["dataset", "PR", "link", f"{row_tool} class", f"{col_tool} class", "status",
               "window", "GACPD PR folder", "PaReco pickle"]
    c.write_csv(out_dir / "transitions_prs.csv", pr_cols, [[r[k] for k in pr_cols] for r in pr_rows])
    grouped_sorted = {k: grouped[k] for k in sorted(grouped, key=lambda k: -len(grouped[k]))}
    (out_dir / "transitions_prs.json").write_text(json.dumps(
        {k: {"count": len(v), "prs": [{x: r[x] for x in pr_cols if x not in (f"{row_tool} class", f"{col_tool} class")}
                                      for r in v]}
         for k, v in grouped_sorted.items()}, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")
    c.write_csv(out_dir / "prs_only_in_one_tool.csv",
                ["dataset", "window", "PR", "link", "classified by", "class"], only_one)
    (out_dir / "transition_matrix.md").write_text(
        f"Rows: {row_tool}, columns: {col_tool}. PRs classified by both tools: {total}; "
        f"same class: {same} ({c.pct(same, total)}%), moved: {total - same} ({c.pct(total - same, total)}%).\n\n"
        + c.markdown_table(header, rows) + "\n\nRow percentages:\n\n"
        + c.markdown_table(header[:-1] + [f"{row_tool} total"], pct_rows) + "\n", encoding="utf-8")
    (out_dir / "transition_matrix.tex").write_text(latex_matrix(row_tool, col_tool, labels, matrix), encoding="utf-8")

    print(f"\nPRs classified by both tools: {total}")
    print(f"  same class : {same} ({c.pct(same, total)}%)")
    print(f"  moved      : {total - same} ({c.pct(total - same, total)}%)")
    print(f"  only one tool classified it: {len(only_one)} "
          f"({sum(1 for r in only_one if r[4] == row_tool)} only {row_tool}, "
          f"{sum(1 for r in only_one if r[4] == col_tool)} only {col_tool})")
    print(f"\n=== Transition matrix (rows: {row_tool}, columns: {col_tool}) ===")
    print(c.format_table(header, rows))
    print(f"\n=== Row % (where each {row_tool} class went) ===")
    print(c.format_table(header[:-1] + [f"{row_tool} total"], pct_rows))
    print("\n=== Largest moves ===")
    moved = [r for r in long_rows if r[5] == "moved"][:10]
    print(c.format_table([f"{row_tool}", f"{col_tool}", "PRs", f"% of {row_tool} class", "% of all"],
                         [r[:5] for r in moved]))
    if show:
        cell = [r for r in pr_rows if (r[f"{row_tool} class"], r[f"{col_tool} class"]) == show]
        print(f"\n=== PRs {row_tool} {show[0]} -> {col_tool} {show[1]} ({len(cell)}) ===")
        if cell:
            print(c.format_table(["dataset", "PR", "link"], [[r["dataset"], r["PR"], r["link"]] for r in cell]))
    print(f"\nPer-PR list for checking: {out_dir / 'transitions_prs.csv'}")
    print(f"wrote {out_dir}")
    c.print_warnings(warnings)
    return 0


if __name__ == "__main__":
    sys.exit(main())
