"""
Small CSV helpers and the definition of the three audit file formats.

Every Results/<index>_<repo>/ folder contains three audit CSVs:

    classification_audit.csv   one row per PR
    file_audit.csv             one row per file of a PR
    hunk_audit.csv             one row per processed hunk

The root of Results/ additionally holds global_*.csv files, which are the
plain concatenation of all per-folder files.
"""

import csv
from pathlib import Path
from typing import NamedTuple, List


class AuditSpec(NamedTuple):
    local_name: str     # file name inside each result folder
    global_name: str    # combined file name in the Results root
    columns: List[str]  # exact expected header


AUDIT_SPECS = {
    "classification": AuditSpec(
        "classification_audit.csv",
        "global_classification_audit.csv",
        ["PR", "PR_Classification", "Files_Fetched_total", "Hunks_Fetched_total"],
    ),
    "file": AuditSpec(
        "file_audit.csv",
        "global_file_audit.csv",
        ["file_name", "file_classification", "file_extensions", "pr_of_file"],
    ),
    "hunk": AuditSpec(
        "hunk_audit.csv",
        "global_hunk_audit.csv",
        ["hunk_number", "hunk_classification", "file_of_hunk", "pr_of_hunk"],
    ),
}

AUDIT_FILE_NAMES = tuple(spec.local_name for spec in AUDIT_SPECS.values())


def write_rows(path, fieldnames, rows, announce=True):
    """Write a list of dicts to a CSV file, creating parent folders."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    if announce:
        print("Created: {} ({} rows)".format(path, len(rows)))
    return path


def read_header_and_rows(path):
    """Return (header, rows) for a CSV file without validating anything."""
    with Path(path).open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        return reader.fieldnames, [dict(row) for row in reader]


def read_audit_csv(path, expected_columns):
    """Read an audit CSV exactly as it was exported.

    Raises if the file is missing or its header differs from the expected one,
    because silently accepting a malformed audit would corrupt every figure.
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(
            "Required audit CSV is missing: {}. Run the audit export first.".format(path)
        )
    header, rows = read_header_and_rows(path)
    if header != list(expected_columns):
        raise RuntimeError(
            "Unexpected columns in {}: {} (expected {}).".format(
                path, header, list(expected_columns)
            )
        )
    return rows
