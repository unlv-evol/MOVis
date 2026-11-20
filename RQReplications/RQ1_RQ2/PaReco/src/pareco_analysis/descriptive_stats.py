"""
Step 4: data behind Table 1, Figure 4 and Figure 5.

Counting units
--------------
Table 1 / Figure 4  one classification_audit.csv row == one PR observation.
Figure 5            file_audit.csv only. For every PR, the share of its files
                    that are classified MO (and separately ED). A PR adds a
                    point for a class only if it has at least one such file.

Pair/direction attribution comes from the per-folder audits combined with the
division plan; the global audits are used to check that every row is counted
exactly once.
"""

import statistics
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List

from . import config
from .audit_combine import load_and_reconcile_global_audits
from .csv_io import write_rows
from .data_division import DIRECTION_LABELS, DIRECTIONS
from .labels import normalize_class
from .results_layout import list_result_folders, match_key, repo_match_keys

TABLE1_FIELDS = ["Direction", "Metric", "Mean", "Min", "Median", "Max", "Total",
                 "Variant_Pairs", "Counting_Source"]
# Columns that describe a (pair, direction) combination.
_PAIR_INFO_KEYS = ["Pair_List_Index", "Upstream", "Fork", "Direction",
                   "Expected_Result_Folders", "Found_Result_Folders",
                   "Pair_Start_Index", "Pair_End_Index"]
FIGURE4_FIELDS = _PAIR_INFO_KEYS + ["PR_Observations", "Classification", "Count"]
PR_OBSERVATION_FIELDS = ["Pair_List_Index", "Upstream", "Fork", "Result_Index", "Result_Folder",
                         "Direction", "PR", "PR_Classification", "Files_Fetched_total",
                         "Hunks_Fetched_total"]
FIGURE5_FIELDS = ["Pair_List_Index", "Upstream", "Fork", "Result_Index", "Result_Folder",
                  "Direction", "PR", "File_Classification", "Files_In_PR_From_File_Audit",
                  "Files_In_Classification", "File_Classification_Proportion"]


@dataclass
class DescriptiveResults:
    """Everything later steps need from this step."""
    class_order: List[str]
    audits: Dict                      # output of load_and_reconcile_global_audits
    table1_rows: List[dict]
    figure4_rows: List[dict]
    figure5_rows: List[dict]
    pr_observation_rows: List[dict]
    per_direction_counts: Dict[str, List[dict]]
    global_pr_class_counts: Counter
    global_special_file_patch_count: int
    paths: Dict[str, Path] = field(default_factory=dict)

    @property
    def global_classification_rows(self):
        return len(self.audits["classification"]["global_rows"])

    @property
    def global_file_rows(self):
        return len(self.audits["file"]["global_rows"])

    @property
    def global_hunk_rows(self):
        return len(self.audits["hunk"]["global_rows"])

    @property
    def global_patch_observations(self):
        """Hunk rows plus CC/NE/EE file rows: the patch unit used by Figure 6."""
        return self.global_hunk_rows + self.global_special_file_patch_count


def _pr_key(value):
    return str(value).strip()


def _class_order(classification_rows):
    """Fixed presentation order plus any unexpected class that actually occurs."""
    order = list(config.CLASS_ORDER)
    for row in classification_rows:
        label = normalize_class(row.get("PR_Classification", "NA"))
        if label not in order:
            order.append(label)
    return order


def _empty_aggregates(plan):
    """One accumulator per (direction, pair)."""
    aggregates = {}
    for pair_index, (upstream, fork) in enumerate(plan.pairs):
        for direction_key in DIRECTIONS:
            planned = [int(row["Result_Index"]) for row in plan.rows_for(pair_index, direction_key)]
            aggregates[(direction_key, pair_index)] = {
                "Pair_List_Index": pair_index,
                "Upstream": upstream,
                "Fork": fork,
                "Direction": DIRECTION_LABELS[direction_key],
                "Expected_Result_Folders": len(planned),
                "Found_Result_Folders": 0,
                "Pair_Start_Index": planned[0] if planned else "",
                "Pair_End_Index": planned[-1] if planned else "",
                "PRs": 0,
                "counts": Counter(),
            }
    return aggregates


def _figure5_rows_for_folder(file_rows, base):
    """Per-PR proportion of MO / ED files for one result folder."""
    files_by_pr = defaultdict(list)
    for row in file_rows:
        files_by_pr[_pr_key(row.get("pr_of_file", ""))].append(
            normalize_class(row.get("file_classification", "NA")))

    rows = []
    for pr, classes in files_by_pr.items():
        counts = Counter(classes)
        for file_class in ("ED", "MO"):
            if counts[file_class] == 0:
                continue
            row = dict(base)
            row.update({
                "PR": pr,
                "File_Classification": file_class,
                "Files_In_PR_From_File_Audit": len(classes),
                "Files_In_Classification": counts[file_class],
                "File_Classification_Proportion": float(counts[file_class]) / float(len(classes)),
            })
            rows.append(row)
    return rows


def _check_equal(name, actual, expected):
    if actual != expected:
        raise RuntimeError("{}: {} != {}.".format(name, actual, expected))


def _table1_rows(per_direction_counts, class_order):
    rows = []
    for direction_key in DIRECTIONS:
        label = DIRECTION_LABELS[direction_key]
        pair_rows = per_direction_counts.get(label, [])
        for metric in class_order + ["PRs"]:
            values = [int(row[metric]) for row in pair_rows]
            if not values:
                continue
            rows.append({
                "Direction": label,
                "Metric": metric,
                "Mean": round(float(sum(values)) / float(len(values)), 1),
                "Min": min(values),
                "Median": statistics.median(values),
                "Max": max(values),
                "Total": sum(values),
                "Variant_Pairs": len(values),
                "Counting_Source": "PR rows from global/per-folder classification_audit.csv",
            })
    return rows


def build_descriptive_results(plan, results_root=config.RESULTS_ROOT):
    """Compute, check and save the data for Table 1, Figure 4 and Figure 5."""
    results_root = Path(results_root)
    audits = load_and_reconcile_global_audits(results_root)
    class_order = _class_order(audits["classification"]["global_rows"])
    aggregates = _empty_aggregates(plan)

    figure5_rows = []
    pr_observation_rows = []
    unplanned = []
    mismatched_sources = []
    rows_seen = Counter()

    for folder in list_result_folders(results_root):
        if not folder.has_audits():
            continue
        plan_row = plan.by_folder.get(folder.name)
        if plan_row is None:
            unplanned.append(folder.name)
            continue

        pair_index = int(plan_row["Pair_List_Index"])
        aggregate = aggregates[(plan_row["Direction_Key"], pair_index)]
        if match_key(folder.source_key) not in repo_match_keys(plan_row["Source"]):
            mismatched_sources.append((folder.index, folder.source_key,
                                       plan_row["Expected_Source_Key"], pair_index))

        # All three are read so the totals can be reconciled below, but this
        # step only uses the classification and file audits.
        classification_rows = folder.audit_rows("classification")
        file_rows = folder.audit_rows("file")
        hunk_rows = folder.audit_rows("hunk")
        rows_seen.update({"classification": len(classification_rows),
                          "file": len(file_rows), "hunk": len(hunk_rows)})

        aggregate["Found_Result_Folders"] += 1
        aggregate["PRs"] += len(classification_rows)
        aggregate["counts"].update(normalize_class(row.get("PR_Classification", "NA"))
                                   for row in classification_rows)

        base = {
            "Pair_List_Index": pair_index,
            "Upstream": aggregate["Upstream"],
            "Fork": aggregate["Fork"],
            "Result_Index": folder.index,
            "Result_Folder": folder.name,
            "Direction": aggregate["Direction"],
        }
        for row in classification_rows:
            observation = dict(base)
            observation.update({
                "PR": _pr_key(row.get("PR", "")),
                "PR_Classification": normalize_class(row.get("PR_Classification", "NA")),
                "Files_Fetched_total": row.get("Files_Fetched_total", ""),
                "Hunks_Fetched_total": row.get("Hunks_Fetched_total", ""),
            })
            pr_observation_rows.append(observation)

        figure5_rows.extend(_figure5_rows_for_folder(file_rows, base))

    # --- consistency checks --------------------------------------------------
    if unplanned:
        raise RuntimeError(
            "{} indexed Results folder(s) contain audit CSVs but are outside the division "
            "plan. First few: {}. Refusing to generate partial totals.".format(len(unplanned), unplanned[:10]))

    if mismatched_sources:
        print("Warning: {} folder/source names differ from the reconstructed plan.".format(
            len(mismatched_sources)))
        for mismatch in mismatched_sources[:10]:
            print("  index {}: found {}, expected {}, pair {}".format(*mismatch))

    for kind in ("hunk", "file", "classification"):
        _check_equal("Planned {} rows vs global audit".format(kind),
                     rows_seen[kind], len(audits[kind]["global_rows"]))

    global_pr_class_counts = Counter(normalize_class(row.get("PR_Classification", "NA"))
                                     for row in audits["classification"]["global_rows"])
    planned_pr_class_counts = Counter()
    for aggregate in aggregates.values():
        planned_pr_class_counts.update(aggregate["counts"])
    _check_equal("Table 1/Figure 4 PR class totals vs global classification audit",
                 dict(planned_pr_class_counts), dict(global_pr_class_counts))

    # --- table / figure rows ---------------------------------------------------
    per_direction_counts = defaultdict(list)
    figure4_rows = []
    for direction_key in DIRECTIONS:
        for pair_index in range(len(plan.pairs)):
            aggregate = aggregates[(direction_key, pair_index)]
            pair_info = {key: aggregate[key] for key in _PAIR_INFO_KEYS}

            pair_row = dict(pair_info, PRs=aggregate["PRs"])
            for class_name in class_order:
                pair_row[class_name] = aggregate["counts"][class_name]
            per_direction_counts[aggregate["Direction"]].append(pair_row)

            for class_name in class_order:
                figure4_rows.append(dict(pair_info,
                                         PR_Observations=aggregate["PRs"],
                                         Classification=class_name,
                                         Count=aggregate["counts"][class_name]))

    table1_rows = _table1_rows(per_direction_counts, class_order)

    special_file_count = sum(
        1 for row in audits["file"]["global_rows"]
        if normalize_class(row.get("file_classification", "NA")) in config.SPECIAL_FILE_CLASSES)

    results = DescriptiveResults(
        class_order=class_order,
        audits=audits,
        table1_rows=table1_rows,
        figure4_rows=figure4_rows,
        figure5_rows=figure5_rows,
        pr_observation_rows=pr_observation_rows,
        per_direction_counts=dict(per_direction_counts),
        global_pr_class_counts=global_pr_class_counts,
        global_special_file_patch_count=special_file_count,
    )
    _save_outputs(results, results_root)
    _print_summary(results)
    return results


def _save_outputs(results, results_root):
    paths = results.paths
    paths["pr_class_totals"] = write_rows(
        results_root / "global_pr_classification_totals_used.csv",
        ["Classification", "PR_Count_From_Global_Classification_Audit"],
        [{"Classification": name,
          "PR_Count_From_Global_Classification_Audit": results.global_pr_class_counts[name]}
         for name in results.class_order],
    )
    paths["table1"] = write_rows(results_root / "table1_descriptive_statistics.csv",
                                 TABLE1_FIELDS, results.table1_rows)
    paths["figure4"] = write_rows(results_root / "figure4_pr_distribution_data.csv",
                                  FIGURE4_FIELDS, results.figure4_rows)
    paths["pr_observations"] = write_rows(results_root / "table_figure_pr_observations.csv",
                                          PR_OBSERVATION_FIELDS, results.pr_observation_rows)
    paths["figure5"] = write_rows(results_root / "figure5_interesting_file_proportions.csv",
                                  FIGURE5_FIELDS, results.figure5_rows)
    paths["totals"] = write_rows(
        results_root / "global_audit_totals_used.csv",
        ["Source", "Data_Rows", "CSV_Lines_Including_Header", "Meaning_In_This_Notebook"],
        [
            {"Source": "global_classification_audit.csv",
             "Data_Rows": results.global_classification_rows,
             "CSV_Lines_Including_Header": results.global_classification_rows + 1,
             "Meaning_In_This_Notebook": "PR observations for Table 1 / Figure 4"},
            {"Source": "global_file_audit.csv",
             "Data_Rows": results.global_file_rows,
             "CSV_Lines_Including_Header": results.global_file_rows + 1,
             "Meaning_In_This_Notebook": "direct source of Figure 5 file-classification proportions"},
            {"Source": "global_hunk_audit.csv",
             "Data_Rows": results.global_hunk_rows,
             "CSV_Lines_Including_Header": results.global_hunk_rows + 1,
             "Meaning_In_This_Notebook": "not used by Table 1/Figures 4-5; retained for Figures 6-8"},
            {"Source": "global_file_audit.csv [CC/NE/EE subset]",
             "Data_Rows": results.global_special_file_patch_count,
             "CSV_Lines_Including_Header": "",
             "Meaning_In_This_Notebook": "additional special-file patch observations retained for Figure 6"},
        ],
    )


def _print_summary(results):
    print("\nGLOBAL AUDIT TOTALS USED (data rows, header not counted):")
    print("  global_classification_audit.csv = {} PR observations".format(results.global_classification_rows))
    print("  global_file_audit.csv           = {} file rows".format(results.global_file_rows))
    print("  global_hunk_audit.csv           = {} hunk rows (Figures 6-8 only)".format(results.global_hunk_rows))
    print("  Table 1 / Figure 4 PR total     = {}".format(
        sum(row["PRs"] for rows in results.per_direction_counts.values() for row in rows)))
    print("  PR classes used                 = {}".format(results.class_order))

    print("\nTable 1 data (PR rows from classification audit):")
    for row in results.table1_rows:
        print(row)
