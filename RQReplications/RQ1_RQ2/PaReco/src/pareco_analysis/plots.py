"""
All matplotlib figures of the paper.

Each function takes the rows produced by the analysis modules, saves a PNG
and returns its path. Pass show=True inside a notebook to also display the
figure; from the command line figures are only saved.

If matplotlib is not installed the functions print a note and return None,
the CSV data behind each figure is still written by the analysis steps.
"""

import statistics
from pathlib import Path

from .data_division import DIRECTION_LABELS, FORK_TO_UPSTREAM, UPSTREAM_TO_FORK

try:
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
except ImportError:
    plt = None

DPI = 200
FORWARD_COLOUR = "tab:blue"
REVERSE_COLOUR = "tab:orange"


def _matplotlib_available(figure_name):
    if plt is None:
        print("matplotlib is not installed; {} CSV data was still created.".format(figure_name))
        return False
    return True


def _finish(fig, output_path, show):
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(str(output_path), dpi=DPI, bbox_inches="tight")
    print("Created: {}".format(output_path))
    if show:
        plt.show()
    else:
        plt.close(fig)
    return output_path


def _colour_boxplot(boxplot, colour):
    for box in boxplot["boxes"]:
        box.set_facecolor(colour)
        box.set_edgecolor(colour)
    for part in ("whiskers", "caps"):
        for line in boxplot[part]:
            line.set_color(colour)
    for median in boxplot["medians"]:
        median.set_color("black")


def plot_figure4(per_direction_counts, class_order, output_path, show=False):
    """Grouped boxplot: PRs per variant pair for every PR classification."""
    if not _matplotlib_available("Figure 4"):
        return None

    forward_label = DIRECTION_LABELS[UPSTREAM_TO_FORK]
    reverse_label = DIRECTION_LABELS[FORK_TO_UPSTREAM]
    forward = per_direction_counts.get(forward_label, [])
    reverse = per_direction_counts.get(reverse_label, [])
    if not forward or not reverse:
        print("Figure 4 plot skipped because both directions were not found.")
        return None

    positions = list(range(1, len(class_order) + 1))
    fig, ax = plt.subplots(figsize=(11, 5.5))
    left = ax.boxplot([[row[name] for row in forward] for name in class_order],
                      positions=[p - 0.18 for p in positions], widths=0.30, patch_artist=True)
    right = ax.boxplot([[row[name] for row in reverse] for name in class_order],
                       positions=[p + 0.18 for p in positions], widths=0.30, patch_artist=True)
    _colour_boxplot(left, FORWARD_COLOUR)
    _colour_boxplot(right, REVERSE_COLOUR)

    ax.set_xticks(positions)
    ax.set_xticklabels(class_order)
    ax.set_xlabel("PR classification")
    ax.set_ylabel("Number of PRs per variant pair")
    ax.set_yscale("log")
    ax.set_title("Distribution of patch classification")
    ax.grid(axis="y", alpha=0.25)
    ax.legend(handles=[
        Patch(facecolor=FORWARD_COLOUR, edgecolor=FORWARD_COLOUR, label=forward_label),
        Patch(facecolor=REVERSE_COLOUR, edgecolor=REVERSE_COLOUR, label=reverse_label),
    ], loc="best")
    return _finish(fig, output_path, show)


def plot_figure5(figure5_rows, output_path, show=False):
    """Violin plot of the per-PR proportion of ED and MO files."""
    if not _matplotlib_available("Figure 5"):
        return None

    values, labels = [], []
    for class_name in ("ED", "MO"):
        class_values = [float(row["File_Classification_Proportion"]) for row in figure5_rows
                        if row["File_Classification"] == class_name]
        if class_values:
            values.append(class_values)
            labels.append(class_name)
    if not values:
        print("Figure 5 plot skipped because file_audit.csv contains no MO/ED file classifications.")
        return None

    fig, ax = plt.subplots(figsize=(7.5, 5.5))
    violins = ax.violinplot(values, showmeans=False, showmedians=True, showextrema=True)
    colours = ["tab:blue", "tab:orange"][:len(labels)]
    for body, colour in zip(violins["bodies"], colours):
        body.set_facecolor(colour)
        body.set_edgecolor(colour)
    for part in ("cmedians", "cbars", "cmins", "cmaxes"):
        violins[part].set_color(colours)
    ax.set_xticks(list(range(1, len(labels) + 1)))
    ax.set_xticklabels(labels)
    ax.set_xlabel("File classification")
    ax.set_ylabel("Proportion of files in PR with this file classification")
    ax.set_title("Distribution of interesting files within the patches")
    ax.set_ylim(0.0, 1.15)
    ax.grid(axis="y", alpha=0.25)
    return _finish(fig, output_path, show)


def plot_figure6(pair_rows, output_path, show=False):
    """Bar chart of patch classes in both directions for one variant pair."""
    if not _matplotlib_available("Figure 6"):
        return None

    classes = [row["Classification"] for row in pair_rows]
    positions = list(range(len(classes)))
    width = 0.36

    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.bar([x - width / 2 for x in positions], [row["Upstream_to_Fork"] for row in pair_rows],
           width=width, label="variant_1 upstream -> variant_2 fork")
    ax.bar([x + width / 2 for x in positions], [row["Fork_to_Upstream"] for row in pair_rows],
           width=width, label="variant_2 fork -> variant_1 upstream")
    ax.set_xticks(positions)
    ax.set_xticklabels(classes)
    ax.set_xlabel("Patch classification")
    ax.set_ylabel("Number of patch observations")
    ax.set_title("{} / {}".format(pair_rows[0]["Upstream"], pair_rows[0]["Fork"]))
    ax.legend(loc="best")
    ax.grid(axis="y", alpha=0.25)
    return _finish(fig, output_path, show)


def _lag_line_plot(weeks, output_path, show, line_label, xlabel, title, marker_size, line_width):
    fig, ax = plt.subplots(figsize=(11, 5.5))
    ax.plot(list(range(1, len(weeks) + 1)), weeks, marker="o",
            markersize=marker_size, linewidth=line_width, label=line_label)
    ax.axhline(statistics.median(weeks), linewidth=1.5, label="Median")
    ax.set_xlabel(xlabel)
    ax.set_ylabel("technical lag (weeks)")
    ax.set_title(title)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="best")
    return _finish(fig, output_path, show)


def plot_figure7(lag_rows, output_path, show=False):
    """Sorted technical lag of every MO patch observation."""
    if not _matplotlib_available("Figure 7"):
        return None
    if not lag_rows:
        print("Figure 7 skipped: no MO technical-lag observations.")
        return None
    weeks = [float(row["Technical_Lag_Weeks"]) for row in lag_rows]
    return _lag_line_plot(weeks, output_path, show, "Weeks",
                          "interesting patch observations (MO)", "MO's Patch technical lag", 2.5, 1.5)


def plot_figure8(target_rows, output_path, show=False):
    """Sorted mean technical lag per target variant."""
    if not _matplotlib_available("Figure 8"):
        return None
    if not target_rows:
        print("Figure 8 skipped: no target variants with MO technical-lag observations.")
        return None
    weeks = [float(row["Mean_Technical_Lag_Weeks"]) for row in target_rows]
    return _lag_line_plot(weeks, output_path, show, "Weeks (mean)",
                          "target variants - MO patch observations",
                          "Mean MO patch technical lag per target variant", 4, 1.25)
