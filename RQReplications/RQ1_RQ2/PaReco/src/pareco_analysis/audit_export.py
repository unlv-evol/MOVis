"""
Step 1: turn the raw PaReco result pickles into JSON + audit CSV files.

Each pickle holds a two-element structure:
    data[0]  per-PR, per-file detailed results
    data[1]  per-PR summary (contains the PR classification)

For every pickle a folder Results/<index>_<source_repo>/ is created with
    <index>_<source_repo>.json       full dump of the pickle
    classification_audit.csv
    file_audit.csv
    hunk_audit.csv
"""

import json
import os
import pickle
import re
import sys
from pathlib import Path

from . import config
from .csv_io import AUDIT_SPECS, write_rows
from .labels import collapse_hunk_sides, normalize_class

try:
    from natsort import natsorted
except ImportError:  # natsort is optional; fall back to a simple natural sort
    def _natural_key(value):
        return [int(part) if part.isdigit() else part.lower()
                for part in re.split(r"(\d+)", str(value))]

    def natsorted(values):
        return sorted(values, key=_natural_key)


def _as_pr_value(pr_text):
    """PR numbers are written as ints whenever possible."""
    return int(pr_text) if pr_text.isdigit() else pr_text


# ---------------------------------------------------------------------------
# Classification helpers
# ---------------------------------------------------------------------------

def file_classification(file_payload):
    """Exported class of a file, including files PaReco never processed."""
    if not isinstance(file_payload, dict):
        return "EE"
    result = file_payload.get("result", {})
    if isinstance(result, str):
        return normalize_class(result)
    if isinstance(result, dict):
        return normalize_class(result.get("patchClass", file_payload.get("patchClass", "NA")))
    return normalize_class(file_payload.get("patchClass", "ERROR"))


def _get_hunk_entry(hunks, hunk_key):
    """Look up a hunk whether its key is an int (pickle) or a str (JSON)."""
    if not isinstance(hunks, dict):
        return {}

    candidates = [hunk_key, str(hunk_key)]
    try:
        candidates.append(int(hunk_key))
    except (TypeError, ValueError):
        pass

    for candidate in candidates:
        if candidate in hunks:
            entry = hunks[candidate]
            return entry if isinstance(entry, dict) else {}
    return {}


def hunk_classification(result, hunk_key):
    """Final class of one hunk, built from its buggy and patched side."""
    buggy = _get_hunk_entry(result.get("hunkMatchesBuggy"), hunk_key)
    patched = _get_hunk_entry(result.get("hunkMatchesPatch"), hunk_key)
    return normalize_class(collapse_hunk_sides(buggy.get("class", ""), patched.get("class", "")))


def _sorted_hunk_keys(buggy_hunks, patched_hunks):
    """Union of the hunk keys of both sides, as strings, in numeric order.

    Using strings makes the int key 0 and the JSON key "0" the same hunk, so
    no hunk can produce two audit rows.
    """
    keys = set()
    for hunks in (buggy_hunks, patched_hunks):
        if isinstance(hunks, dict):
            keys.update(str(key) for key in hunks.keys())
    return sorted(keys, key=lambda key: (0, int(key)) if key.isdigit() else (1, key))


# ---------------------------------------------------------------------------
# Audit creation
# ---------------------------------------------------------------------------

def recreate_audit_csvs(data, output_dir, audit_prs=None):
    """Write the three audit CSVs for one loaded result pickle.

    audit_prs can restrict the export to a subset of PR numbers.
    Returns the paths of the classification, file and hunk audits.
    """
    raw_results = data[0]
    pr_results = data[1] if len(data) > 1 and isinstance(data[1], dict) else {}
    wanted_prs = None if audit_prs is None else {str(pr) for pr in audit_prs}

    classification_rows, file_rows, hunk_rows = [], [], []

    for pr in natsorted(raw_results.keys()):
        pr_text = str(pr)
        if wanted_prs is not None and pr_text not in wanted_prs:
            continue

        files_total = 0
        hunks_total = 0

        for file_name in natsorted(raw_results[pr].keys()):
            file_payload = raw_results[pr][file_name]
            result = file_payload.get("result", {}) if isinstance(file_payload, dict) else {}

            # Every file of the source patch is listed, also CC/NE/EE files
            # that have no hunks: each of those counts as one patch later on.
            files_total += 1
            file_rows.append({
                "file_name": file_name,
                "file_classification": file_classification(file_payload),
                "file_extensions": Path(file_name).suffix,
                "pr_of_file": _as_pr_value(pr_text),
            })

            if not isinstance(result, dict):
                continue
            buggy_hunks = result.get("hunkMatchesBuggy")
            patched_hunks = result.get("hunkMatchesPatch")
            if not isinstance(buggy_hunks, dict) and not isinstance(patched_hunks, dict):
                continue

            for position, hunk_key in enumerate(_sorted_hunk_keys(buggy_hunks, patched_hunks), start=1):
                hunks_total += 1
                # Stored hunk keys are zero-based; the audit uses 1-based numbers.
                hunk_number = int(hunk_key) + 1 if hunk_key.isdigit() else position
                hunk_rows.append({
                    "hunk_number": hunk_number,
                    "hunk_classification": hunk_classification(result, hunk_key),
                    "file_of_hunk": file_name,
                    "pr_of_hunk": _as_pr_value(pr_text),
                })

        classification_rows.append({
            "PR": _as_pr_value(pr_text),
            "PR_Classification": normalize_class(pr_results.get(pr_text, {}).get("class", "NA")),
            "Files_Fetched_total": files_total,
            "Hunks_Fetched_total": hunks_total,
        })

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    paths = []
    for kind, rows in (("classification", classification_rows),
                       ("file", file_rows),
                       ("hunk", hunk_rows)):
        spec = AUDIT_SPECS[kind]
        path = output_dir / spec.local_name
        write_rows(path, spec.columns, rows, announce=False)
        print("Created: {}".format(path))
        paths.append(path)

    print("Rows -> PRs: {}, files: {}, hunks: {}".format(
        len(classification_rows), len(file_rows), len(hunk_rows)))
    return tuple(paths)


def _make_pareco_importable():
    """Pickles may reference PaReco classes, so PaReco has to be on sys.path."""
    for path in config.PARECO_SEARCH_PATHS:
        if path.exists() and str(path) not in sys.path:
            sys.path.insert(0, str(path))


def result_folder_name(pickle_name):
    """'9_ekohl_metadata_json_deps_results.pkl' -> '9_ekohl_metadata_json_deps'."""
    return "_".join(pickle_name.split("_")[:-1])


def export_all_pickles(pickle_dir=config.PICKLE_DIR, results_root=config.RESULTS_ROOT, audit_prs=None):
    """Create the JSON dump and audits for every pickle in pickle_dir."""
    pickle_dir = Path(pickle_dir)
    results_root = Path(results_root)
    if not pickle_dir.exists():
        raise FileNotFoundError("Pickle folder not found: {}".format(pickle_dir))

    _make_pareco_importable()

    # Only pickles: the folder may also hold the notebook's interval_plan CSVs.
    for file_name in natsorted(name for name in os.listdir(str(pickle_dir)) if name.endswith(".pkl")):
        with (pickle_dir / file_name).open("rb") as handle:
            data = pickle.load(handle)

        folder_name = result_folder_name(file_name)
        folder = results_root / folder_name
        folder.mkdir(parents=True, exist_ok=True)

        with (folder / (folder_name + ".json")).open("w", encoding="utf-8") as handle:
            json.dump(data, handle, indent=4, ensure_ascii=False, default=str)

        recreate_audit_csvs(data, folder, audit_prs=audit_prs)
