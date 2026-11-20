#!/usr/bin/env python3
"""RQ3 step 4: PRs of the last 6 / 12 / 18 months of one dataset, split into
similar and different classifications, plus a random sample of the different ones.

The data is stored in 6-month date windows, so "last 6 months" is the dataset's
last window, "last 12 months" its last 2 windows and "last 18 months" its last 3.
The printed date ranges show what each period covers. The final window of a
dataset is often cut short at the collection date; --exclude-partial-last drops
it when it covers fewer than --partial-days days.

If the name you give matches more than one dataset (a repo listed twice), the
script asks which one to use (or pass --pick N).

For every period it writes, into RQ3/output/samples/<dataset>/:
  <period>_similar_prs.json     both tools gave the same class
  <period>_different_prs.json   the tools disagree
  <period>_sample_prs.json      random sample of the different PRs
Each PR entry has "correct classification" and "Why that's correct" fields to
fill in by hand.

Sample size: Cochran's formula with finite population correction, at the given
confidence (default 95%) and margin of error (default 5%), p = 0.5, computed from
that dataset's number of different PRs in that period. The samples are nested:
one seeded random order is drawn over the 18-month different PRs and each period
takes its first n PRs from that order. Each sample is still a simple random
sample of its period, and the samples overlap as much as random sampling allows,
so labels done for one period carry over to the others wherever the same PR is drawn.

Re-running is safe: labels already filled in any JSON in the dataset folder are
copied into the regenerated files (matched on window + PR).

Usage:
  python RQ3/rq3_sample_prs.py DATASET_TEXT [--pick N] [--periods 6,12,18]
                                [--confidence 0.95] [--margin 0.05] [--seed 42]
                                [--exclude-partial-last] [--list]
Example:
  python RQ3/rq3_sample_prs.py apache_kafka
  python RQ3/rq3_sample_prs.py spesmilo_electrum --pick 2
"""

from __future__ import annotations

import argparse
import json
import math
import random
import sys
from pathlib import Path
from statistics import NormalDist

import rq3_common as c

LABEL_FIELD = "correct classification"
WHY_FIELD = "Why that's correct"
WINDOW_MONTHS = 6


def cochran_sample_size(population: int, confidence: float, margin: float, p: float = 0.5) -> int:
    if population <= 0:
        return 0
    z = NormalDist().inv_cdf(1 - (1 - confidence) / 2)
    n0 = (z ** 2) * p * (1 - p) / (margin ** 2)
    n = math.ceil(n0 / (1 + (n0 - 1) / population))
    return min(n, population)


def choose_dataset(datasets: list[c.Dataset], text: str, pick: int | None) -> c.Dataset | None:
    key = text.lower().replace("-", "_").replace("/", "_").replace(".", "_")
    matches = [d for d in datasets if key in d.name.lower()]
    exact = [d for d in matches if d.repo.lower() == key or f"{d.run}_{d.repo}".lower() == key]
    if exact:
        matches = exact
    if not matches:
        print(f"No dataset matches '{text}'. Use --list to see all datasets.")
        return None
    if len(matches) == 1:
        return matches[0]

    print(f"'{text}' matches {len(matches)} datasets:")
    for i, d in enumerate(matches, 1):
        n_g = len(d.pr_classes(c.GACPD))
        n_p = len(d.pr_classes(c.PARECO))
        print(f"  [{i}] {d.name}  (#{d.windows[0].idx}-#{d.windows[-1].idx}, {len(d.windows)} windows, "
              f"{c.GACPD} {n_g} PRs, {c.PARECO} {n_p} PRs){'  ' + d.source_repo + ' -> ' + (d.target_repo or '?') if d.source_repo else ''}")
    if pick is not None:
        if 1 <= pick <= len(matches):
            return matches[pick - 1]
        print(f"--pick {pick} is out of range 1-{len(matches)}")
        return None
    while True:
        try:
            answer = input(f"Which one? [1-{len(matches)}]: ").strip()
        except EOFError:
            print("\nNo answer received; re-run with --pick N.")
            return None
        if answer.isdigit() and 1 <= int(answer) <= len(matches):
            return matches[int(answer) - 1]
        print("Please type one of the numbers above.")


def pr_link(source_repo: str | None, pr: str) -> str:
    return f"https://github.com/{source_repo}/pull/{pr}" if source_repo and pr.isdigit() else ""


def existing_labels(folder: Path) -> dict[tuple[str, str], dict]:
    """(window, PR) -> {label, why} from every JSON already in the folder."""
    found: dict[tuple[str, str], dict] = {}
    for path in folder.glob("*.json"):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            print(f"WARNING: could not read {path}; its labels are not carried over")
            continue
        for e in data.get("prs", []) if isinstance(data, dict) else []:
            label = str(e.get(LABEL_FIELD, "")).strip()
            why = str(e.get(WHY_FIELD, "")).strip()
            if label or why:
                k = (e.get("window", ""), str(e.get("PR", "")))
                prev = found.get(k)
                if prev and prev[LABEL_FIELD] and label and prev[LABEL_FIELD].upper() != label.upper():
                    print(f"WARNING: PR {k[1]} has two different labels ({prev[LABEL_FIELD]} / {label}); "
                          f"keeping {prev[LABEL_FIELD]}")
                    continue
                found[k] = {LABEL_FIELD: label, WHY_FIELD: why}
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dataset", nargs="?", help="Text identifying the dataset, e.g. apache_kafka")
    ap.add_argument("--pick", type=int, help="Which match to use when several datasets match (1-based)")
    ap.add_argument("--list", action="store_true", help="List all datasets and exit")
    ap.add_argument("--periods", default="6,12,18", help="Months, multiples of 6 (default: 6,12,18)")
    ap.add_argument("--confidence", type=float, default=0.95)
    ap.add_argument("--margin", type=float, default=0.05, help="Margin of error (default 0.05)")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--exclude-partial-last", action="store_true",
                    help="Drop the dataset's last window if it is shorter than --partial-days")
    ap.add_argument("--partial-days", type=int, default=150)
    ap.add_argument("--results", default=str(c.DEFAULT_RESULTS))
    ap.add_argument("--out", default=str(c.DEFAULT_OUT / "samples"))
    args = ap.parse_args()

    datasets, warnings = c.load_datasets(Path(args.results))

    if args.list or not args.dataset:
        rows = [[d.name, len(d.windows), len(d.pr_classes(c.GACPD)), len(d.pr_classes(c.PARECO))]
                for d in datasets]
        print(c.format_table(["dataset", "windows", f"{c.GACPD} PRs", f"{c.PARECO} PRs"], rows))
        if not args.dataset:
            print("\nGive a dataset name (or part of it) to build the samples.")
        return 0

    ds = choose_dataset(datasets, args.dataset, args.pick)
    if ds is None:
        return 1

    periods = sorted({int(p) for p in args.periods.split(",") if p.strip()})
    if any(p <= 0 or p % WINDOW_MONTHS for p in periods):
        print(f"--periods must be positive multiples of {WINDOW_MONTHS}")
        return 1

    windows = list(ds.windows)
    last = windows[-1]
    if last.days < args.partial_days:
        if args.exclude_partial_last:
            print(f"Dropping partial last window {last.name} ({last.days} days)")
            windows = windows[:-1]
        else:
            print(f"Note: last window {last.name} covers only {last.days} days "
                  f"(use --exclude-partial-last to drop it)")

    print(f"\nDataset: {ds.name}  ({len(ds.windows)} windows)"
          + (f"  source repo: {ds.source_repo}" if ds.source_repo else ""))

    out_dir = Path(args.out) / ds.name
    out_dir.mkdir(parents=True, exist_ok=True)
    labels = existing_labels(out_dir)
    if labels:
        print(f"Carrying over {len(labels)} label(s) already filled in under {out_dir}")

    # One seeded random order over the longest period's different PRs; each period takes
    # its first n from it (nested simple random samples).
    rng = random.Random(args.seed)

    def entry(pr, g, p, kind):
        cls_g, win = g
        cls_p, _ = p
        e = {
            "PR": pr,
            "link": pr_link(ds.source_repo, pr),
            "window": win,
            f"{c.GACPD} Class": cls_g,
            f"{c.PARECO} Class": cls_p,
            "status": kind,
            LABEL_FIELD: "",
            WHY_FIELD: "",
        }
        e.update(labels.get((win, pr), {}))
        return e

    by_period = {}
    for months in periods:
        n_windows = months // WINDOW_MONTHS
        if n_windows > len(windows):
            print(f"WARNING: {months} months needs {n_windows} windows, dataset has {len(windows)}; using all")
        ws = windows[-n_windows:]
        g, p = ds.pr_classes(c.GACPD, ws), ds.pr_classes(c.PARECO, ws)
        sort_key = lambda s: int(s) if s.isdigit() else 0
        similar, different, only = [], [], []
        for pr in sorted(set(g) | set(p), key=sort_key):
            if pr in g and pr in p:
                kind = "similar" if g[pr][0] == p[pr][0] else "different"
                (similar if kind == "similar" else different).append(entry(pr, g[pr], p[pr], kind))
            else:
                tool, (cls, win) = (c.GACPD, g[pr]) if pr in g else (c.PARECO, p[pr])
                only.append({"PR": pr, "window": win, "classified by": tool, "class": cls})
        by_period[months] = (ws, similar, different, only)

    longest = by_period[periods[-1]][2]
    order = [(e["window"], e["PR"]) for e in longest]
    rng.shuffle(order)
    rank = {k: i for i, k in enumerate(order)}

    summary_rows = []
    for months in periods:
        ws, similar, different, only = by_period[months]
        period = f"{months}_months"
        meta = {
            "dataset": ds.name,
            "period": f"last {months} months",
            "from": ws[0].start.isoformat(),
            "to": ws[-1].end.isoformat(),
            "windows": [w.name for w in ws],
            "source repo": ds.source_repo or "",
            "allowed labels": c.PR_CLASS_ORDER,
        }
        n = cochran_sample_size(len(different), args.confidence, args.margin)
        sample = sorted(different, key=lambda e: rank.get((e["window"], e["PR"]), len(rank)))[:n]
        sample.sort(key=lambda e: int(e["PR"]) if e["PR"].isdigit() else 0)

        files = {
            "similar": dict(meta, count=len(similar), prs=similar),
            "different": dict(meta, count=len(different), prs=different),
            "sample": dict(meta, population=len(different), **{"sample size": n},
                           confidence=args.confidence, **{"margin of error": args.margin},
                           seed=args.seed, count=n, prs=sample),
        }
        if only:
            files["similar"]["PRs classified by only one tool"] = only
            files["different"]["PRs classified by only one tool"] = only
        for kind, payload in files.items():
            (out_dir / f"{period}_{kind}_prs.json").write_text(
                json.dumps(payload, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")

        labelled = sum(1 for e in sample if str(e[LABEL_FIELD]).strip())
        summary_rows.append([f"{months} months", f"{ws[0].start} -> {ws[-1].end}", len(ws),
                             len(similar) + len(different), len(similar), len(different), n,
                             f"{labelled}/{n}", len(only)])

    print()
    print(c.format_table(["period", "dates", "win", "both tools", "similar", "different",
                          "sample", "labelled", "one tool only"], summary_rows))
    print(f"\nSample size: Cochran, {args.confidence:.0%} confidence, {args.margin:.0%} margin, "
          f"finite population correction, seed {args.seed}")
    print(f"wrote {out_dir}")
    relevant = [w for w in warnings if any(win.name in w for win in ds.windows)]
    c.print_warnings(relevant)
    return 0


if __name__ == "__main__":
    sys.exit(main())
