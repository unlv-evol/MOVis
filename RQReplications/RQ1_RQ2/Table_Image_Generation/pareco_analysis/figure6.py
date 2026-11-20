"""
Step 5: Figure 6, patch classes per variant pair in both directions.

Patch unit (same as described in the paper):
    one patch per hunk_audit.csv row
  + one patch per file_audit.csv row classified CC, NE or EE
    (those files were never split into hunks)

Either one pair is processed (selected by upstream name, optionally with an
occurrence number or an exact list index because some upstreams appear more
than once), or all pairs are processed. When all pairs are processed the
total is checked against the global audits.
"""

import re
from collections import Counter
from pathlib import Path

from . import config
from .csv_io import write_rows
from .data_division import FORK_TO_UPSTREAM, UPSTREAM_TO_FORK
from .labels import normalize_class
from .plots import plot_figure6
from .results_layout import folders_by_name, repo_key

FIGURE6_FIELDS = [
    "Pair_List_Index", "Upstream", "Fork", "Classification",
    "Upstream_to_Fork", "Fork_to_Upstream",
    "Upstream_Total_Patch_Observations", "Fork_Total_Patch_Observations",
    "Upstream_Hunk_Rows", "Fork_Hunk_Rows",
    "Upstream_Special_File_Patches", "Fork_Special_File_Patches",
    "Upstream_Start_Index", "Upstream_End_Index",
    "Upstream_Expected_Folders", "Upstream_Found_Folders",
    "Fork_Start_Index", "Fork_End_Index",
    "Fork_Expected_Folders", "Fork_Found_Folders",
    "Upstream_Missing_Indexes", "Fork_Missing_Indexes",
]


def choose_pair_index(pairs, upstream, pair_list_index=None, occurrence=1):
    """Resolve which pair is meant by an upstream name.

    pair_list_index (zero-based) wins if given; otherwise `occurrence`
    (one-based) picks between duplicate upstream names.
    """
    wanted = repo_key(upstream)

    if pair_list_index is not None:
        pair_list_index = int(pair_list_index)
        if not 0 <= pair_list_index < len(pairs):
            raise IndexError("pair_list_index is outside the variant pair list.")
        if repo_key(pairs[pair_list_index][0]) != wanted:
            raise ValueError("pair_list_index={} points to {}, not {}.".format(
                pair_list_index, pairs[pair_list_index][0], upstream))
        return pair_list_index

    positions = [i for i, (candidate, _) in enumerate(pairs) if repo_key(candidate) == wanted]
    if not positions:
        raise LookupError("{} was not found among the upstream repositories.".format(upstream))
    if not 1 <= int(occurrence) <= len(positions):
        raise IndexError("Upstream {} occurs {} time(s); occurrence {} is invalid.".format(
            upstream, len(positions), occurrence))
    return positions[int(occurrence) - 1]


def count_patches(plan, pair_index, direction_key, folders):
    """Patch-class counts of one pair in one direction."""
    planned_rows = plan.rows_for(pair_index, direction_key)
    counts = Counter()
    found, missing = [], []
    hunk_total = special_total = 0

    for plan_row in planned_rows:
        folder = folders.get(plan_row["Result_Folder"])
        if folder is None:
            missing.append(int(plan_row["Result_Index"]))
            continue

        hunk_rows = folder.audit_rows("hunk")
        special_files = [row for row in folder.audit_rows("file")
                         if normalize_class(row.get("file_classification", "NA")) in config.SPECIAL_FILE_CLASSES]

        hunk_total += len(hunk_rows)
        special_total += len(special_files)
        counts.update(normalize_class(row.get("hunk_classification", "NA")) for row in hunk_rows)
        counts.update(normalize_class(row.get("file_classification", "NA")) for row in special_files)
        found.append(folder)

    patch_total = hunk_total + special_total
    if sum(counts.values()) != patch_total:
        raise RuntimeError("Figure 6 class counts ({}) do not equal patch observations ({}) "
                           "for pair {} direction {}.".format(sum(counts.values()), patch_total,
                                                               pair_index, direction_key))
    return {
        "indexes": [int(row["Result_Index"]) for row in planned_rows],
        "counts": counts,
        "patches": patch_total,
        "hunks": hunk_total,
        "special_files": special_total,
        "found": len(found),
        "missing": missing,
    }


def pair_rows(plan, pair_index, folders, class_order):
    """One CSV row per class for a single variant pair."""
    upstream, fork = plan.pairs[pair_index]
    forward = count_patches(plan, pair_index, UPSTREAM_TO_FORK, folders)
    reverse = count_patches(plan, pair_index, FORK_TO_UPSTREAM, folders)

    def side(prefix, data):
        indexes = data["indexes"]
        return {
            prefix + "_Total_Patch_Observations": data["patches"],
            prefix + "_Hunk_Rows": data["hunks"],
            prefix + "_Special_File_Patches": data["special_files"],
            prefix + "_Start_Index": indexes[0] if indexes else "",
            prefix + "_End_Index": indexes[-1] if indexes else "",
            prefix + "_Expected_Folders": len(indexes),
            prefix + "_Found_Folders": data["found"],
            prefix + "_Missing_Indexes": ";".join(str(i) for i in data["missing"]),
        }

    shared = {"Pair_List_Index": pair_index, "Upstream": upstream, "Fork": fork}
    shared.update(side("Upstream", forward))
    shared.update(side("Fork", reverse))

    return [dict(shared, Classification=name,
                 Upstream_to_Fork=forward["counts"][name],
                 Fork_to_Upstream=reverse["counts"][name])
            for name in class_order]


def _safe_name(repo):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", repo_key(repo))


def save_pair(rows, output_dir, make_plot=True, show=False):
    first = rows[0]
    base_name = "figure6_pair_{:03d}_{}_to_{}".format(
        first["Pair_List_Index"], _safe_name(first["Upstream"]), _safe_name(first["Fork"]))
    csv_path = write_rows(Path(output_dir) / (base_name + ".csv"), FIGURE6_FIELDS, rows)
    if make_plot:
        plot_figure6(rows, Path(output_dir) / (base_name + ".png"), show=show)
    return csv_path


def print_pair_summary(rows):
    first = rows[0]
    print("\nPair {}: {} / {}".format(first["Pair_List_Index"], first["Upstream"], first["Fork"]))
    for prefix, text in (("Upstream", "upstream -> fork"), ("Fork", "fork -> upstream")):
        print("  {} folders: {}..{} (expected {}, found {}), patch observations={}".format(
            text, first[prefix + "_Start_Index"], first[prefix + "_End_Index"],
            first[prefix + "_Expected_Folders"], first[prefix + "_Found_Folders"],
            first[prefix + "_Total_Patch_Observations"]))
        if first[prefix + "_Missing_Indexes"]:
            print("  missing {}-side indexes: {}".format(prefix.lower(), first[prefix + "_Missing_Indexes"]))


def generate_figure6(plan, descriptive, upstream=None, pair_list_index=None, occurrence=1,
                     stop_after_pair_index=None, stop_after_upstream=None, stop_occurrence=1,
                     results_root=config.RESULTS_ROOT, make_plots=True, show=False):
    """Create Figure 6 data (and plots).

    upstream=None or "NONE" processes every pair; otherwise only the selected
    pair is processed. The stop_* arguments can end an all-pairs run early.
    Returns the list of all written rows.
    """
    results_root = Path(results_root)
    output_dir = results_root / "Figure6"
    folders = folders_by_name(results_root)
    if not folders:
        raise FileNotFoundError("No indexed result folders were found under {}.".format(results_root))

    if upstream is not None and str(upstream).strip().upper() != "NONE":
        index = choose_pair_index(plan.pairs, upstream, pair_list_index, occurrence)
        rows = pair_rows(plan, index, folders, descriptive.class_order)
        save_pair(rows, output_dir, make_plots, show)
        print_pair_summary(rows)
        return rows

    stop_key = repo_key(stop_after_upstream) if stop_after_upstream is not None else None
    seen_upstreams = Counter()
    all_rows = []
    stopped_early = False

    for index, (pair_upstream, _) in enumerate(plan.pairs):
        seen_upstreams[repo_key(pair_upstream)] += 1
        rows = pair_rows(plan, index, folders, descriptive.class_order)
        save_pair(rows, output_dir, make_plots, show=False)
        print_pair_summary(rows)
        all_rows.extend(rows)

        if stop_after_pair_index is not None and index == int(stop_after_pair_index):
            print("Reached requested pair-list stop index {}; stopping.".format(index))
            stopped_early = True
            break
        if stop_key == repo_key(pair_upstream) and seen_upstreams[stop_key] == int(stop_occurrence):
            print("Reached requested upstream stop {} occurrence {}; stopping.".format(
                stop_after_upstream, stop_occurrence))
            stopped_early = True
            break

    if all_rows:
        write_rows(output_dir / "figure6_all_pairs_data.csv", FIGURE6_FIELDS, all_rows)
        if not stopped_early:
            _check_all_pairs_total(all_rows, descriptive)
    return all_rows


def _check_all_pairs_total(all_rows, descriptive):
    """All pairs together must cover every hunk row and every CC/NE/EE file."""
    first_row_per_pair = {}
    for row in all_rows:
        first_row_per_pair.setdefault(row["Pair_List_Index"], row)
    total = sum(int(row["Upstream_Total_Patch_Observations"]) + int(row["Fork_Total_Patch_Observations"])
                for row in first_row_per_pair.values())

    expected = descriptive.global_patch_observations
    if total != expected:
        raise RuntimeError("Figure 6 ALL patch observations ({}) != combined global audit "
                           "patch observations ({}).".format(total, expected))
    print("Figure 6 ALL reconciliation passed: {} patch observations == hunk rows + "
          "CC/NE/EE file rows.".format(total))
