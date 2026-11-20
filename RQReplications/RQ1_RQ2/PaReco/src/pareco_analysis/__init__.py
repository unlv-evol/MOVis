"""
Analysis of PaReco results for patch propagation between software variants.

The pipeline is split into steps, one module each:

    audit_export       raw result pickles -> JSON + per-folder audit CSVs
    audit_combine      per-folder audits  -> global audits (+ reconciliation)
    data_division      result folder      -> variant pair and direction
    descriptive_stats  Table 1, Figure 4, Figure 5
    figure6            patch classes per variant pair
    technical_lag      Figures 7 and 8

Supporting modules: config (settings), labels, csv_io, results_layout,
github_api, pareco_probe and plots. run_analysis.py in the project root runs
any of the steps from the command line.
"""

from . import config
from .audit_combine import combine_all_audits, load_and_reconcile_global_audits
from .audit_export import export_all_pickles, recreate_audit_csvs
from .data_division import create_division_plan, print_division_report
from .descriptive_stats import build_descriptive_results
from .figure6 import generate_figure6
from .technical_lag import build_technical_lag_data

__all__ = [
    "config",
    "export_all_pickles",
    "recreate_audit_csvs",
    "combine_all_audits",
    "load_and_reconcile_global_audits",
    "create_division_plan",
    "print_division_report",
    "build_descriptive_results",
    "generate_figure6",
    "build_technical_lag_data",
]
