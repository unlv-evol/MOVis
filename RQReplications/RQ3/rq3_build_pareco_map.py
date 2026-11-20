#!/usr/bin/env python3
"""Build RQ3/pareco_windows.csv: the repo and date window of every PaReco pickle.

The PaReco pickles are numbered after the GACPD run they were produced from
(Results/Repos_results_old, runs dsept3 / dsept3R):
  pickle <i>_... with i <  419  ->  dsept3_<i>_<repo>_<start>_<end>
  pickle <i>_... with i >= 419  ->  dsept3R_<i - 419>_<repo>_<start>_<end>
This script reads that folder list once, checks each pairing on the PR sets, and
writes repo + start + end per pickle. The other RQ3 scripts then pair pickles with
any GACPD run by repo + dates, so renumbered or renamed runs keep working and
Repos_results_old is no longer needed.

Usage:
  python RQ3/rq3_build_pareco_map.py [--old Results/Repos_results_old] [--pareco Results/PaReco_Results]
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

import rq3_common as c


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--old", default=str(c.DEFAULT_RESULTS / "Repos_results_old"),
                    help="GACPD run the pickles were numbered after")
    ap.add_argument("--pareco", default=str(c.DEFAULT_RESULTS / c.PARECO_DIR))
    ap.add_argument("--forward-run", default="dsept3")
    ap.add_argument("--reverse-run", default="dsept3R")
    ap.add_argument("--offset", type=int, default=419, help="First pickle index of the reverse run")
    ap.add_argument("--out", default=str(c.PARECO_MAP))
    args = ap.parse_args()

    old = {}
    for f in Path(args.old).iterdir():
        m = c.FOLDER_RE.match(f.name) if f.is_dir() else None
        if m:
            old[(m["run"], int(m["idx"]))] = (f, m)

    rows, counts = [], {"yes": 0, "subset": 0, "no": 0, "missing": 0}
    for pkl in sorted(Path(args.pareco).glob("*.pkl"), key=lambda p: int(p.name.split("_")[0])):
        i = int(pkl.name.split("_")[0])
        key = (args.forward_run, i) if i < args.offset else (args.reverse_run, i - args.offset)
        if key not in old:
            counts["missing"] += 1
            print(f"  {pkl.name}: no folder {key[0]}_{key[1]}_* in {args.old}")
            continue
        folder, m = old[key]
        prs = set(c.pareco_pr_classes(c.load_pareco_pickle(pkl)))
        audit = folder / "classification_audit.csv"
        gacpd = set()
        if audit.is_file():
            with audit.open(newline="", encoding="utf-8-sig") as fh:
                gacpd = {r[0] for r in list(csv.reader(fh))[1:] if r}
        match = "yes" if prs == gacpd else ("subset" if prs and prs <= gacpd else "no")
        counts[match] += 1
        if match == "no":
            print(f"  {pkl.name}: PRs differ from {folder.name} ({len(prs)} vs {len(gacpd)})")
        rows.append([pkl.name, i, m["repo"], c.parse_date(m["start"]).isoformat(),
                     c.parse_date(m["end"]).isoformat(), folder.name, match])

    c.write_csv(Path(args.out), c.PARECO_MAP_COLUMNS, rows)
    print(f"wrote {args.out}: {len(rows)} pickles  (PR sets equal: {counts['yes']}, pickle subset: "
          f"{counts['subset']}, different: {counts['no']}, no folder: {counts['missing']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
