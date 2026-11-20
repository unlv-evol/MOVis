# RQ3 scripts

Python 3.10+, standard library only (matplotlib optional, for confusion-matrix PNGs).
Run from anywhere; data is read from `../Results`, output goes to `RQ3/output/`.

## Before you start: set up the global `Results` folder

Every script reads from the global `Results` folder at the root of the replication
package, next to `RQ3/`, not inside it. Before running anything, make sure that
folder exists and uses these exact names:

```
Replication Package/
├── Results/
│   ├── PaReco_Results/      PaReco results: <i>_<owner>_<name>_results.pkl
│   ├── Repos_results/       GACPD (MOVis) results: <run>_<#>_<owner>_<name>_<YYYY_MM_DD>_<YYYY_MM_DD>/
└── RQ3/
    └──ManualChecks           Contains .xlsx files that showcase why 50/40/30/20 was choosen
```

- **PaReco results folder:** `Results/PaReco_Results`
- **GACPD results folder:** `Results/Repos_results`

The folder names must match exactly, including case: the scripts look for
`PaReco_Results` and `Repos_results`. If your results are somewhere else, pass
`--results <path to the folder that contains both>` to the scripts that accept it.

| Step | Script | What it does |
|---|---|---|
| 1 | `rq3_pr_totals.py` | Total PRs and PRs per classification for GACPD and PaReco (SP counted as MO) |
| 2 | `rq3_file_extensions.py` | Every unique file extension with its file count, per tool (`--by-class` for a class split) |
| 3 | `rq3_classification_transitions.py` | Transition matrix PaReco → GACPD (`--from GACPD` flips it), plus `transitions_prs.csv` with every PR (number, link, both classes, GACPD folder, PaReco pickle); `--show CC:NE` prints one cell's PRs |
| 4 | `rq3_sample_prs.py DATASET` | Last 6/12/18 months of one dataset → similar / different / sample JSONs to label |
| 5 | `rq3_metrics.py` | Confusion matrices, per-class P/R/F1, macro/weighted/micro, kappa, MCC, McNemar |

`rq3_common.py` holds the shared loading code: the dataset rule from MOVis
`scripts/aggregate_repo_results.py` and the PaReco pickle reader, which doesn't need
PaReco's `Methods` package or `bitarray`.

PaReco numbers always come from the pickles in `Results/PaReco_Results`; every pickle
is counted (the `PaReco pickles used` column in `pr_totals_PaReco.csv` lists them).
To compare the tools, each pickle is paired with the GACPD folder that has the same repo
and date window, whatever the GACPD run is called (`dsep3`, `dsep3R`, ...) or numbered.
The pickle's repo and dates are stored in `pareco_windows.csv`. That file is built once by
`rq3_build_pareco_map.py` from `Results/Repos_results_old`, the run the pickles were numbered
after (pickle `<i>` = `dsept3_<i>` for i < 419, `dsept3R_<i-419>` otherwise; 381 PR sets match
exactly). After that, `Repos_results_old` is no longer needed. A pickle with no matching folder
pairs by PR set, or is counted on its own.

Labelling workflow:

```
python rq3_sample_prs.py apache_kafka            # asks which one if the name matches 2 datasets
# fill "correct classification" (MO, ED, CC, NE, NA, ERROR) and "Why that's correct"
# in output/samples/<dataset>/<N>_months_sample_prs.json
python rq3_metrics.py                            # all labelled PRs, each counted once
python rq3_metrics.py --ignore-pareco-cc         # drop PRs PaReco called CC (both tools)
python rq3_metrics.py --include-similar          # add the PRs both tools agreed on
```

Re-running `rq3_sample_prs.py` keeps labels already filled in.
