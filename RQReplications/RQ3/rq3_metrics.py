#!/usr/bin/env python3
"""RQ3 step 5: score GACPD and PaReco against the manual "correct classification".

Reads the JSON files written by rq3_sample_prs.py (RQ3/output/samples/<dataset>/),
takes every PR whose "correct classification" is filled in, and treats that label as
the ground truth for both tools. A PR that appears in several files (6/12/18 months,
different/sample) is counted once. PRs with a blank label are skipped and counted.

Results per tool:
  confusion matrix (rows = correct class, columns = tool's class)
  per class: support, TP, FP, FN, TN, precision, recall, specificity, F1
  accuracy, balanced accuracy, macro / weighted / micro precision-recall-F1,
  Cohen's kappa, Matthews correlation coefficient (multi-class)
and a paired comparison of the two tools (who got each PR right) with an exact
McNemar test. Precision/recall/F1 of a class with no predictions/support is 0.
Macro averages run over the classes that appear in the truth or in that tool's
predictions (the scikit-learn convention); balanced accuracy over truth classes.

Options:
  --ignore-pareco-cc   leave out PRs that PaReco classified CC (cannot classify).
                       --cc-scope both (default) drops them for both tools so the
                       two are scored on the same PRs; --cc-scope pareco drops them
                       only from PaReco's scores.
  --include-similar    also count the PRs both tools agreed on (*_similar_prs.json).
                       Their "correct classification" is used when filled in;
                       otherwise the class both tools gave is taken as correct.
  --source             which files hold the labels: all (default), sample, different.
  --dataset / --period restrict to some datasets (name contains text) / periods.

Outputs go to RQ3/output/metrics/<variant>/ : report.md, results.json, summary.csv,
per_class_<tool>.csv, confusion_<tool>.csv, per_dataset.csv, per_period.csv,
tables.tex and, when matplotlib is installed, confusion_<tool>.png.

Usage:
  python RQ3/rq3_metrics.py [--include-similar] [--ignore-pareco-cc [--cc-scope pareco]]
                            [--dataset TEXT] [--period 6,12,18] [--source all|sample|different]
                            [--samples DIR] [--out DIR]
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

import rq3_common as c

LABEL_FIELD = "correct classification"
WHY_FIELD = "Why that's correct"
TOOLS = (c.GACPD, c.PARECO)
FILE_RE = re.compile(r"^(?P<months>\d+)_months_(?P<kind>similar|different|sample)_prs\.json$")


# ---------------------------------------------------------------------------
# Loading labels
# ---------------------------------------------------------------------------

def load_records(samples_dir: Path, args, notes: list[str]) -> list[dict]:
    kinds = {"all": {"different", "sample"}, "sample": {"sample"}, "different": {"different"}}[args.source]
    if args.include_similar:
        kinds = kinds | {"similar"}
    periods = {int(p) for p in args.period.split(",")} if args.period else None

    records: dict[tuple[str, str, str], dict] = {}
    blank = Counter()
    files_read = 0
    for path in sorted(samples_dir.glob("*/*.json")):
        m = FILE_RE.match(path.name)
        if not m or m["kind"] not in kinds:
            continue
        if periods and int(m["months"]) not in periods:
            continue
        dataset = path.parent.name
        if args.dataset and args.dataset.lower() not in dataset.lower():
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            notes.append(f"could not read {path}: {exc}")
            continue
        files_read += 1
        for e in data.get("prs", []):
            key = (dataset, e.get("window", ""), str(e.get("PR", "")))
            raw = str(e.get(LABEL_FIELD, "")).strip()
            rec = records.get(key)
            if rec is None:
                rec = records[key] = {
                    "dataset": dataset, "window": key[1], "PR": key[2],
                    c.GACPD: c.normalize(e.get(f"{c.GACPD} Class")),
                    c.PARECO: c.normalize(e.get(f"{c.PARECO} Class")),
                    "kind": "similar" if m["kind"] == "similar" else "different",
                    "periods": set(), "truth": None, "why": "", "assumed": False,
                }
            rec["periods"].add(int(m["months"]))
            if raw:
                label = c.normalize(raw)
                if label not in c.PR_CLASS_ORDER:
                    notes.append(f"{path.name} PR {key[2]}: unknown label '{raw}', ignored")
                    continue
                if rec["truth"] and not rec["assumed"] and rec["truth"] != label:
                    notes.append(f"{dataset} PR {key[2]}: conflicting labels {rec['truth']} / {label}; "
                                 f"kept {rec['truth']}")
                    continue
                rec["truth"], rec["why"], rec["assumed"] = label, str(e.get(WHY_FIELD, "")), False

    if files_read == 0:
        notes.append(f"no matching JSON files found under {samples_dir}")

    out = []
    for rec in records.values():
        if rec["truth"] is None and rec["kind"] == "similar":
            rec["truth"], rec["assumed"] = rec[c.GACPD], True
        if rec["truth"] is None:
            blank[rec["kind"]] += 1
            continue
        out.append(rec)
    if blank:
        notes.append(f"skipped {sum(blank.values())} PR(s) with no correct classification yet")
    assumed = sum(1 for r in out if r["assumed"])
    if assumed:
        notes.append(f"{assumed} similar PR(s) had no label; the class both tools agreed on was used")
    print(f"Read {files_read} file(s): {len(records)} unique PRs, {len(out)} with a correct classification")
    return out


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def safe_div(a: float, b: float) -> float:
    return a / b if b else 0.0


def evaluate(pairs: list[tuple[str, str]], labels: list[str] | None = None) -> dict:
    """pairs = [(truth, predicted)]. Returns all metrics for one tool."""
    if labels is None:
        labels = c.order_labels({t for t, _ in pairs} | {p for _, p in pairs})
    n = len(pairs)
    cm = Counter(pairs)
    per_class = {}
    for k in labels:
        tp = cm[(k, k)]
        fp = sum(cm[(t, k)] for t in labels if t != k)
        fn = sum(cm[(k, p)] for p in labels if p != k)
        tn = n - tp - fp - fn
        prec, rec = safe_div(tp, tp + fp), safe_div(tp, tp + fn)
        per_class[k] = {
            "support": tp + fn, "predicted": tp + fp, "TP": tp, "FP": fp, "FN": fn, "TN": tn,
            "precision": prec, "recall": rec, "specificity": safe_div(tn, tn + fp),
            "F1": safe_div(2 * prec * rec, prec + rec),
        }
    correct = sum(cm[(k, k)] for k in labels)
    acc = safe_div(correct, n)
    # Macro averages over classes in the truth or in this tool's predictions (as scikit-learn does);
    # the shared label list only keeps both tools' matrices aligned.
    active = [k for k in labels if per_class[k]["support"] or per_class[k]["predicted"]]
    macro = {x: safe_div(sum(per_class[k][x] for k in active), len(active)) for x in ("precision", "recall", "F1")}
    weighted = {x: safe_div(sum(per_class[k][x] * per_class[k]["support"] for k in labels), n)
                for x in ("precision", "recall", "F1")}
    tp_all = sum(per_class[k]["TP"] for k in labels)
    fp_all = sum(per_class[k]["FP"] for k in labels)
    fn_all = sum(per_class[k]["FN"] for k in labels)
    micro_p, micro_r = safe_div(tp_all, tp_all + fp_all), safe_div(tp_all, tp_all + fn_all)
    micro = {"precision": micro_p, "recall": micro_r, "F1": safe_div(2 * micro_p * micro_r, micro_p + micro_r)}

    # Cohen's kappa and multi-class MCC (Gorodkin's R_K)
    t_k = {k: per_class[k]["support"] for k in labels}
    p_k = {k: per_class[k]["predicted"] for k in labels}
    pe = safe_div(sum(t_k[k] * p_k[k] for k in labels), n * n)
    kappa = safe_div(acc - pe, 1 - pe) if n else 0.0
    cov_tp = correct * n - sum(t_k[k] * p_k[k] for k in labels)
    denom = math.sqrt((n * n - sum(p_k[k] ** 2 for k in labels)) * (n * n - sum(t_k[k] ** 2 for k in labels)))
    mcc = safe_div(cov_tp, denom)

    present = [k for k in labels if per_class[k]["support"]]
    balanced = safe_div(sum(per_class[k]["recall"] for k in present), len(present))
    return {
        "labels": labels, "n": n, "correct": correct, "confusion": {f"{t}|{p}": v for (t, p), v in cm.items()},
        "per_class": per_class, "accuracy": acc, "balanced_accuracy": balanced,
        "macro": macro, "weighted": weighted, "micro": micro, "kappa": kappa, "mcc": mcc,
    }


def mcnemar_exact(b: int, c_: int) -> float:
    """Two-sided exact McNemar p-value from the discordant counts b and c."""
    n = b + c_
    if n == 0:
        return 1.0
    k = min(b, c_)
    tail = sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n
    return min(1.0, 2 * tail)


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def f3(x: float) -> str:
    return f"{x:.3f}"


def confusion_rows(res: dict) -> tuple[list, list]:
    labels = res["labels"]
    cm = Counter({tuple(k.split("|")): v for k, v in res["confusion"].items()})
    header = ["correct \\ predicted"] + labels + ["TOTAL"]
    rows = [[t] + [cm[(t, p)] for p in labels] + [sum(cm[(t, p)] for p in labels)] for t in labels]
    rows.append(["TOTAL"] + [sum(cm[(t, p)] for t in labels) for p in labels] + [res["n"]])
    return header, rows


def per_class_rows(res: dict) -> tuple[list, list]:
    header = ["class", "support", "predicted", "TP", "FP", "FN", "TN", "precision", "recall", "specificity", "F1"]
    rows = []
    for k in res["labels"]:
        p = res["per_class"][k]
        rows.append([k, p["support"], p["predicted"], p["TP"], p["FP"], p["FN"], p["TN"],
                     f3(p["precision"]), f3(p["recall"]), f3(p["specificity"]), f3(p["F1"])])
    return header, rows


SUMMARY_KEYS = [
    ("PRs", lambda r: r["n"]),
    ("correct", lambda r: r["correct"]),
    ("accuracy", lambda r: f3(r["accuracy"])),
    ("balanced accuracy", lambda r: f3(r["balanced_accuracy"])),
    ("macro precision", lambda r: f3(r["macro"]["precision"])),
    ("macro recall", lambda r: f3(r["macro"]["recall"])),
    ("macro F1", lambda r: f3(r["macro"]["F1"])),
    ("weighted precision", lambda r: f3(r["weighted"]["precision"])),
    ("weighted recall", lambda r: f3(r["weighted"]["recall"])),
    ("weighted F1", lambda r: f3(r["weighted"]["F1"])),
    ("micro F1", lambda r: f3(r["micro"]["F1"])),
    ("Cohen's kappa", lambda r: f3(r["kappa"])),
    ("MCC", lambda r: f3(r["mcc"])),
]


def latex_table(caption: str, label: str, header: list, rows: list) -> str:
    esc = lambda s: str(s).replace("\\", "$\\backslash$").replace("%", "\\%").replace("_", "\\_").replace("&", "\\&")
    lines = ["\\begin{table}[ht]", "\\centering", f"\\caption{{{caption}}}", f"\\label{{{label}}}",
             "\\begin{tabular}{l" + "r" * (len(header) - 1) + "}", "\\hline",
             " & ".join(esc(h) for h in header) + " \\\\", "\\hline"]
    lines += [" & ".join(esc(v) for v in r) + " \\\\" for r in rows]
    lines += ["\\hline", "\\end{tabular}", "\\end{table}", ""]
    return "\n".join(lines)


def try_plot(res: dict, title: str, path: Path) -> bool:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return False
    labels = res["labels"]
    cm = Counter({tuple(k.split("|")): v for k, v in res["confusion"].items()})
    grid = [[cm[(t, p)] for p in labels] for t in labels]
    fig, ax = plt.subplots(figsize=(1.1 * len(labels) + 2, 1.0 * len(labels) + 1.5))
    im = ax.imshow(grid, cmap="Blues")
    ax.set_xticks(range(len(labels)), labels)
    ax.set_yticks(range(len(labels)), labels)
    ax.set_xlabel("Predicted")
    ax.set_ylabel("Correct classification")
    ax.set_title(title)
    top = max(max(r) for r in grid) if grid else 0
    for i, r in enumerate(grid):
        for j, v in enumerate(r):
            ax.text(j, i, str(v), ha="center", va="center", color="white" if v > top / 2 else "black")
    fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    fig.tight_layout()
    fig.savefig(path, dpi=200)
    plt.close(fig)
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--samples", default=str(c.DEFAULT_OUT / "samples"), help="Folder written by rq3_sample_prs.py")
    ap.add_argument("--out", help="Output folder (default: RQ3/output/metrics/<variant>)")
    ap.add_argument("--ignore-pareco-cc", action="store_true", help="Leave out PRs PaReco classified CC")
    ap.add_argument("--cc-scope", choices=["both", "pareco"], default="both",
                    help="With --ignore-pareco-cc: drop those PRs for both tools (default) or only for PaReco")
    ap.add_argument("--include-similar", action="store_true", help="Also count PRs both tools agreed on")
    ap.add_argument("--source", choices=["all", "sample", "different"], default="all",
                    help="Which files to read labels from (default: all)")
    ap.add_argument("--dataset", help="Only datasets whose name contains this text")
    ap.add_argument("--period", help="Only these periods in months, e.g. 12 or 6,12")
    args = ap.parse_args()

    notes: list[str] = []
    records = load_records(Path(args.samples), args, notes)
    if not records:
        print("No labelled PRs found. Fill in \"correct classification\" in the JSONs from rq3_sample_prs.py.")
        c.print_warnings(notes)
        return 1

    variant = "metrics"
    if args.include_similar:
        variant += "_with_similar"
    if args.ignore_pareco_cc:
        variant += "_no_pareco_cc" + ("_pareco_only" if args.cc_scope == "pareco" else "")
    if args.source != "all":
        variant += f"_{args.source}"
    out_dir = Path(args.out) if args.out else c.DEFAULT_OUT / "metrics" / variant
    out_dir.mkdir(parents=True, exist_ok=True)

    pareco_cc = [r for r in records if r[c.PARECO] == "CC"]
    per_tool_records = {t: list(records) for t in TOOLS}
    if args.ignore_pareco_cc:
        if args.cc_scope == "both":
            per_tool_records = {t: [r for r in records if r[c.PARECO] != "CC"] for t in TOOLS}
        else:
            per_tool_records[c.PARECO] = [r for r in records if r[c.PARECO] != "CC"]
        notes.append(f"left out {len(pareco_cc)} PR(s) PaReco classified CC "
                     f"({'both tools' if args.cc_scope == 'both' else 'PaReco only'})")

    # Shared label set so both matrices line up.
    labels = c.order_labels({r["truth"] for rs in per_tool_records.values() for r in rs}
                            | {r[t] for t, rs in per_tool_records.items() for r in rs})
    results = {t: evaluate([(r["truth"], r[t]) for r in per_tool_records[t]], labels) for t in TOOLS}

    # Paired comparison on PRs both tools are scored on.
    both_keys = {(r["dataset"], r["window"], r["PR"]) for r in per_tool_records[c.GACPD]} & \
                {(r["dataset"], r["window"], r["PR"]) for r in per_tool_records[c.PARECO]}
    paired = Counter()
    for r in records:
        if (r["dataset"], r["window"], r["PR"]) in both_keys:
            paired[(r[c.GACPD] == r["truth"], r[c.PARECO] == r["truth"])] += 1
    only_g, only_p = paired[(True, False)], paired[(False, True)]
    p_value = mcnemar_exact(only_g, only_p)

    # ---- files
    md = [f"# RQ3 classification results ({variant})", ""]
    md.append(f"Labelled PRs: {len(records)} "
              f"({sum(1 for r in records if r['kind'] == 'different')} different, "
              f"{sum(1 for r in records if r['kind'] == 'similar')} similar). "
              f"Datasets: {len({r['dataset'] for r in records})}.")
    md.append(f"Options: include similar = {args.include_similar}, ignore PaReco CC = {args.ignore_pareco_cc}"
              + (f" (scope {args.cc_scope})" if args.ignore_pareco_cc else "") + f", source = {args.source}.")
    md.append("")

    tex = []
    sum_header = ["metric"] + list(TOOLS)
    sum_rows = [[name] + [fn(results[t]) for t in TOOLS] for name, fn in SUMMARY_KEYS]
    c.write_csv(out_dir / "summary.csv", sum_header, sum_rows)
    md += ["## Summary", "", c.markdown_table(sum_header, sum_rows), ""]
    tex.append(latex_table("Overall classification performance.", "tab:rq3-summary", sum_header, sum_rows))

    print(f"\n=== Summary ({variant}) ===")
    print(c.format_table(sum_header, sum_rows))

    for t in TOOLS:
        h, rows = confusion_rows(results[t])
        c.write_csv(out_dir / f"confusion_{t}.csv", h, rows)
        ph, prow = per_class_rows(results[t])
        c.write_csv(out_dir / f"per_class_{t}.csv", ph, prow)
        md += [f"## {t}", "", "Confusion matrix (rows: correct classification, columns: " + t + ")", "",
               c.markdown_table(h, rows), "", "Per class", "", c.markdown_table(ph, prow), ""]
        tex.append(latex_table(f"Confusion matrix of {t} (rows: correct classification).",
                               f"tab:rq3-confusion-{t.lower()}", h, rows))
        tex.append(latex_table(f"Per-class results of {t}.", f"tab:rq3-perclass-{t.lower()}", ph, prow))
        print(f"\n=== {t}: confusion matrix (rows = correct, columns = {t}) ===")
        print(c.format_table(h, rows))
        print(f"\n=== {t}: per class ===")
        print(c.format_table(ph, prow))
        if try_plot(results[t], f"{t} vs correct classification", out_dir / f"confusion_{t}.png"):
            notes.append(f"wrote {out_dir / f'confusion_{t}.png'}")

    pair_header = ["", f"{c.PARECO} correct", f"{c.PARECO} wrong", "TOTAL"]
    pair_rows = [[f"{c.GACPD} correct", paired[(True, True)], only_g, paired[(True, True)] + only_g],
                 [f"{c.GACPD} wrong", only_p, paired[(False, False)], only_p + paired[(False, False)]],
                 ["TOTAL", paired[(True, True)] + only_p, only_g + paired[(False, False)], sum(paired.values())]]
    c.write_csv(out_dir / "paired_comparison.csv", pair_header, pair_rows)
    md += ["## Paired comparison", "", c.markdown_table(pair_header, pair_rows), "",
           f"Exact McNemar test on the {only_g + only_p} discordant PRs: p = {p_value:.4g}", ""]
    tex.append(latex_table(f"Paired comparison (exact McNemar p = {p_value:.4g}).", "tab:rq3-paired",
                           pair_header, pair_rows))
    print("\n=== Paired comparison ===")
    print(c.format_table(pair_header, pair_rows))
    print(f"Exact McNemar test: {only_g} PRs only {c.GACPD} right, {only_p} only {c.PARECO} right, p = {p_value:.4g}")

    # Breakdowns
    def breakdown(group_of) -> list[list]:
        groups = defaultdict(lambda: {t: [] for t in TOOLS})
        for t in TOOLS:
            for r in per_tool_records[t]:
                for g in group_of(r):
                    groups[g][t].append((r["truth"], r[t]))
        rows = []
        for g in sorted(groups, key=lambda s: (int(s.split()[0]), s) if s.split()[0].isdigit() else (0, s)):
            res = {t: evaluate(groups[g][t], labels) for t in TOOLS}
            rows.append([g] + sum(([res[t]["n"], f3(res[t]["accuracy"]), f3(res[t]["macro"]["F1"])]
                                   for t in TOOLS), []))
        return rows

    bd_header = lambda first: [first] + sum(([f"{t} PRs", f"{t} accuracy", f"{t} macro F1"] for t in TOOLS), [])
    ds_rows = breakdown(lambda r: [r["dataset"]])
    per_rows = breakdown(lambda r: [f"{m} months" for m in sorted(r["periods"])])
    c.write_csv(out_dir / "per_dataset.csv", bd_header("dataset"), ds_rows)
    c.write_csv(out_dir / "per_period.csv", bd_header("period"), per_rows)
    md += ["## Per dataset", "", c.markdown_table(bd_header("dataset"), ds_rows), "",
           "## Per period (a PR counts in every period it falls in)", "",
           c.markdown_table(bd_header("period"), per_rows), ""]
    print("\n=== Per period ===")
    print(c.format_table(bd_header("period"), per_rows))

    labelled = sorted(records, key=lambda r: (r["dataset"], int(r["PR"]) if r["PR"].isdigit() else 0))
    c.write_csv(out_dir / "labelled_prs.csv",
                ["dataset", "window", "PR", "kind", "correct", "label assumed", c.GACPD, c.PARECO, "why"],
                [[r["dataset"], r["window"], r["PR"], r["kind"], r["truth"], r["assumed"],
                  r[c.GACPD], r[c.PARECO], r["why"]] for r in labelled])

    (out_dir / "report.md").write_text("\n".join(md), encoding="utf-8")
    (out_dir / "tables.tex").write_text("\n".join(tex), encoding="utf-8")
    payload = {
        "options": {"include_similar": args.include_similar, "ignore_pareco_cc": args.ignore_pareco_cc,
                    "cc_scope": args.cc_scope, "source": args.source, "dataset": args.dataset,
                    "period": args.period},
        "labelled_prs": len(records),
        "results": results,
        "paired": {"both_correct": paired[(True, True)], f"only_{c.GACPD}": only_g,
                   f"only_{c.PARECO}": only_p, "both_wrong": paired[(False, False)],
                   "mcnemar_exact_p": p_value},
    }
    (out_dir / "results.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"\nwrote {out_dir}")
    c.print_warnings(notes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
