"""
Step 2: combine the per-folder audits into global audits, and check them.

combine_all_audits() writes Results/global_*.csv.

load_and_reconcile_global_audits() is used by every later step. It re-reads
the global files and proves they are exactly the multiset union of all
per-folder audits, so no table or figure can silently gain or lose rows.
"""

from collections import Counter
from pathlib import Path

from . import config
from .csv_io import AUDIT_SPECS, read_audit_csv, read_header_and_rows, write_rows


def _local_audit_paths(results_root, local_name):
    """All per-folder copies of an audit file (the root itself is excluded)."""
    results_root = Path(results_root)
    return sorted(
        (path for path in results_root.rglob(local_name) if path.parent != results_root),
        key=str,
    )


def combine_audit_csvs(results_root, kind):
    """Concatenate every per-folder audit of one kind into its global file.

    Files with an unexpected header are reported and skipped. The global file
    is overwritten, so this can be re-run safely.
    """
    spec = AUDIT_SPECS[kind]
    results_root = Path(results_root)
    paths = _local_audit_paths(results_root, spec.local_name)

    rows = []
    for path in paths:
        header, file_rows = read_header_and_rows(path)
        if header != spec.columns:
            print("Skipping {}: unexpected columns {}; expected {}".format(
                path, header, spec.columns))
            continue
        rows.extend(file_rows)

    output_path = results_root / spec.global_name
    write_rows(output_path, spec.columns, rows, announce=False)
    print("Created: {} ({} rows from {} files)".format(output_path, len(rows), len(paths)))
    return output_path


def combine_all_audits(results_root=config.RESULTS_ROOT):
    results_root = Path(results_root)
    results_root.mkdir(parents=True, exist_ok=True)
    return {kind: combine_audit_csvs(results_root, kind) for kind in AUDIT_SPECS}


# ---------------------------------------------------------------------------
# Reconciliation
# ---------------------------------------------------------------------------

_ROLE_IN_ANALYSIS = {
    "classification": "PR classifications for Table 1 / Figure 4",
    "file": "file observations for Figure 5 and special-file observations for Figure 6",
    "hunk": "hunk observations retained for Figures 6-8",
}


def _row_signature(row, columns):
    return tuple(str(row.get(column, "")) for column in columns)


def load_and_reconcile_global_audits(results_root=config.RESULTS_ROOT):
    """Load the global audits and check them against the per-folder files.

    Returns a dict  kind -> {"global_path", "global_rows", "local_paths"}
    and writes Results/global_audit_reconciliation.csv.
    Raises RuntimeError on any mismatch.
    """
    results_root = Path(results_root)
    audits = {}
    report_rows = []

    for kind, spec in AUDIT_SPECS.items():
        global_path = results_root / spec.global_name
        global_rows = read_audit_csv(global_path, spec.columns)

        local_paths = _local_audit_paths(results_root, spec.local_name)
        local_rows = []
        for path in local_paths:
            local_rows.extend(read_audit_csv(path, spec.columns))

        global_counter = Counter(_row_signature(row, spec.columns) for row in global_rows)
        local_counter = Counter(_row_signature(row, spec.columns) for row in local_rows)
        exact_match = global_counter == local_counter

        report_rows.append({
            "Audit": kind,
            "Global_File": spec.global_name,
            "Global_Data_Rows": len(global_rows),
            "Local_Audit_Files": len(local_paths),
            "Local_Data_Rows": len(local_rows),
            "Exact_Row_Multiset_Match": exact_match,
            "Role_In_Analysis": _ROLE_IN_ANALYSIS[kind],
        })

        if not exact_match:
            missing = sum((local_counter - global_counter).values())
            extra = sum((global_counter - local_counter).values())
            raise RuntimeError(
                "{} does not exactly match its per-folder {} files: {} local rows "
                "missing from global, {} extra global rows. Re-run the combine step "
                "before generating figures.".format(spec.global_name, spec.local_name, missing, extra)
            )

        audits[kind] = {
            "global_path": global_path,
            "global_rows": global_rows,
            "local_paths": local_paths,
        }

    write_rows(
        results_root / "global_audit_reconciliation.csv",
        ["Audit", "Global_File", "Global_Data_Rows", "Local_Audit_Files",
         "Local_Data_Rows", "Exact_Row_Multiset_Match", "Role_In_Analysis"],
        report_rows,
    )
    return audits
