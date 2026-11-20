"""
Step 3: attribute every audited Results folder to a variant pair and direction.

The folders only record the SOURCE repository and an index, so the pair and
direction have to be reconstructed. Two sources of truth are used:

1. Interval plans (exact). The PaReco notebook builds an interval_plan
   DataFrame (data_number, variant_index, variant1, variant2, label, start,
   end) before running. Saved as config.INTERVAL_PLAN_GLOB next to the
   pickles, every row names its pair and dates, so a folder whose index and
   source match a row is attributed directly.

2. Collection order (fallback, for folders no plan covers):
     - the direction follows from the source: every repository is either an
       upstream or a fork in config.VARIANT_PAIRS, never both
     - within one direction, pairs were processed in VARIANT_PAIRS order
   Walking that direction's folders in index order, consecutive folders with
   the same source stay on the current pair; when the source changes we jump
   forward to the first later pair with that source. Each direction is walked
   on its own, so it does not matter whether the two passes share a numbering
   (old runs) or each start their own (GACPD's dsep3_ / dsep3R_ scheme).
   Consecutive pairs sharing a source (spesmilo/electrum to EXOS-Electrum and
   to electrum-grs) cannot be told apart this way; a warning says so.

Repositories are matched under their listed and their canonical name
(config.RENAMED_REPOS), since PaReco now names results after the canonical
full_name GitHub reports.

Result indexes are unique only within a direction, so a folder is identified
by its name (Result_Folder).

Outputs:
    Results/data_division_manifest.csv   one row per audited folder
    Results/pair_division_summary.csv    one row per pair
"""

import csv
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from . import config
from .csv_io import write_rows
from .results_layout import list_result_folders, match_key, repo_key, repo_match_keys

UPSTREAM_TO_FORK = "upstream_to_fork"
FORK_TO_UPSTREAM = "fork_to_upstream"
DIRECTIONS = (UPSTREAM_TO_FORK, FORK_TO_UPSTREAM)

DIRECTION_LABELS = {
    UPSTREAM_TO_FORK: "variant_1 upstream (source) -> variant_2 fork (target)",
    FORK_TO_UPSTREAM: "variant_2 fork (source) -> variant_1 upstream (target)",
}

PLAN_SOURCE_INTERVAL_PLAN = "interval_plan"
PLAN_SOURCE_COLLECTION_ORDER = "collection_order"

MANIFEST_FIELDS = [
    "Result_Index", "Result_Folder", "Pair_List_Index", "Upstream", "Fork", "Direction_Key",
    "Direction", "Source", "Target", "Expected_Source_Key", "Interval_Number",
    "Interval_Count", "Pair_Start_Index", "Pair_End_Index", "Start", "End",
    "Label", "Starting_Flip_Index", "Attributed_By",
]

SUMMARY_FIELDS = [
    "Pair_List_Index", "Upstream", "Fork",
    "Upstream_to_Fork_Folders", "Upstream_to_Fork_Start", "Upstream_to_Fork_End",
    "Fork_to_Upstream_Folders", "Fork_to_Upstream_Start", "Fork_to_Upstream_End",
]


def source_and_target(pairs, pair_index, direction_key):
    upstream, fork = pairs[pair_index]
    if direction_key == UPSTREAM_TO_FORK:
        return upstream, fork
    if direction_key == FORK_TO_UPSTREAM:
        return fork, upstream
    raise ValueError("Unknown direction: {}".format(direction_key))


@dataclass(frozen=True)
class CollectionStage:
    """One (pair, direction) combination in the order it was collected."""
    pair_index: int
    direction_key: str
    upstream: str
    fork: str

    @property
    def source(self):
        return self.upstream if self.direction_key == UPSTREAM_TO_FORK else self.fork

    @property
    def target(self):
        return self.fork if self.direction_key == UPSTREAM_TO_FORK else self.upstream

    @property
    def expected_source_key(self):
        """Folder-name form of the source under its current GitHub name."""
        return repo_key(config.canonical_repo(self.source))

    def matches(self, folder):
        return match_key(folder.source_key) in repo_match_keys(self.source)


def build_collection_stages(pairs, direction_key):
    """The stages of one direction, in VARIANT_PAIRS order."""
    return [CollectionStage(pair_index, direction_key, upstream, fork)
            for pair_index, (upstream, fork) in enumerate(pairs)]


# ---------------------------------------------------------------------------
# Direction of a folder
# ---------------------------------------------------------------------------

def _side_keys(pairs):
    upstream_keys, fork_keys = set(), set()
    for upstream, fork in pairs:
        upstream_keys |= repo_match_keys(upstream)
        fork_keys |= repo_match_keys(fork)
    both = upstream_keys & fork_keys
    if both:
        raise ValueError("These repositories are both an upstream and a fork in VARIANT_PAIRS, "
                         "so a folder's direction cannot be derived from its source: {}".format(
                             sorted(both)))
    return upstream_keys, fork_keys


def folder_direction(folder, upstream_keys, fork_keys):
    key = match_key(folder.source_key)
    if key in upstream_keys:
        return UPSTREAM_TO_FORK
    if key in fork_keys:
        return FORK_TO_UPSTREAM
    raise RuntimeError(
        "Result folder {} has source {}, which is not a repository of VARIANT_PAIRS. If the "
        "repository was renamed or transferred, add it to config.RENAMED_REPOS.".format(
            folder.name, folder.source_key))


# ---------------------------------------------------------------------------
# Interval plans
# ---------------------------------------------------------------------------

def _pair_of_plan_row(pairs, variant1, variant2):
    """(pair_index, direction_key) of a plan row, from its two repositories."""
    first, second = match_key(variant1), match_key(variant2)
    for pair_index, (upstream, fork) in enumerate(pairs):
        if first in repo_match_keys(upstream) and second in repo_match_keys(fork):
            return pair_index, UPSTREAM_TO_FORK
        if first in repo_match_keys(fork) and second in repo_match_keys(upstream):
            return pair_index, FORK_TO_UPSTREAM
    return None


def load_interval_plans(plan_dir, pairs):
    """{(data_number, source match key): entry} from every saved interval plan."""
    entries = {}
    if plan_dir is None or not Path(plan_dir).exists():
        return entries

    for path in sorted(Path(plan_dir).glob(config.INTERVAL_PLAN_GLOB)):
        with path.open(newline="", encoding="utf-8") as handle:
            for line, row in enumerate(csv.DictReader(handle), start=2):
                try:
                    data_number = int(str(row["data_number"]).strip())
                    variant1, variant2 = row["variant1"].strip(), row["variant2"].strip()
                except (KeyError, ValueError, AttributeError):
                    raise ValueError("{} line {}: an interval plan needs data_number, variant1 "
                                     "and variant2.".format(path, line))

                located = _pair_of_plan_row(pairs, variant1, variant2)
                if located is None:
                    raise ValueError("{} line {}: {} -> {} is not a pair of VARIANT_PAIRS (add "
                                     "renamed repositories to config.RENAMED_REPOS).".format(
                                         path, line, variant1, variant2))

                entry = {
                    "pair_index": located[0],
                    "direction_key": located[1],
                    "label": (row.get("label") or "").strip(),
                    "start": (row.get("start") or "").strip(),
                    "end": (row.get("end") or "").strip(),
                    "plan_file": path.name,
                }
                key = (data_number, match_key(variant1))
                previous = entries.get(key)
                if previous is not None and any(previous[k] != entry[k] for k in
                                                ("pair_index", "direction_key", "label")):
                    raise ValueError("Interval plans disagree on data number {} of {} ({} and "
                                     "{}).".format(data_number, variant1, previous["plan_file"],
                                                   path.name))
                entries[key] = entry
    return entries


# ---------------------------------------------------------------------------
# Collection-order fallback
# ---------------------------------------------------------------------------

def assign_folders_to_stages(folders, stages):
    """Return a list of (folder, stage) for the folders of ONE direction, in index order."""
    assignments = []
    current = -1

    for folder in sorted(folders, key=lambda f: (f.index, f.name)):
        if current >= 0 and stages[current].matches(folder):
            assignments.append((folder, stages[current]))
            continue

        # Earliest LATER stage whose source matches this folder.
        next_stage = next((i for i in range(current + 1, len(stages)) if stages[i].matches(folder)),
                          None)
        if next_stage is None:
            where = ("pair {}".format(stages[current].pair_index)
                     if current >= 0 else "before first assigned stage")
            raise RuntimeError(
                "Result folder {} (index {}, source {}) cannot be matched to any later pair of "
                "its direction. Current stage: {}. Check the order of VARIANT_PAIRS, the Results "
                "folder name, or save the PaReco interval plan.".format(
                    folder.name, folder.index, folder.source_key, where))

        current = next_stage
        assignments.append((folder, stages[current]))

    return assignments


def _warn_ambiguous_neighbours(assignments, stages):
    """Consecutive pairs with the same source cannot be separated by order alone."""
    for first, second in zip(stages, stages[1:]):
        if not repo_match_keys(first.source) & repo_match_keys(second.source):
            continue
        landed = sum(1 for _, stage in assignments if stage.pair_index == first.pair_index)
        if landed:
            print("Warning: pairs {} and {} both have source {} in direction {}; without an "
                  "interval plan they cannot be told apart, so all {} folder(s) were attributed "
                  "to pair {}.".format(first.pair_index, second.pair_index, first.source,
                                       first.direction_key, landed, first.pair_index))


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

def assign_folders(folders, pairs, plan_entries):
    """(folder, stage, plan entry or None) for every folder."""
    upstream_keys, fork_keys = _side_keys(pairs)
    stages = {direction_key: build_collection_stages(pairs, direction_key)
              for direction_key in DIRECTIONS}

    assignments = []
    unplanned = defaultdict(list)
    for folder in folders:
        entry = plan_entries.get((folder.index, match_key(folder.source_key)))
        if entry is not None:
            assignments.append((folder, stages[entry["direction_key"]][entry["pair_index"]], entry))
        else:
            unplanned[folder_direction(folder, upstream_keys, fork_keys)].append(folder)

    for direction_key in DIRECTIONS:
        if not unplanned[direction_key]:
            continue
        ordered = assign_folders_to_stages(unplanned[direction_key], stages[direction_key])
        _warn_ambiguous_neighbours(ordered, stages[direction_key])
        assignments.extend((folder, stage, None) for folder, stage in ordered)

    return assignments


def build_manifest(results_root, pairs, plan_dir=None):
    """Manifest rows for all audited result folders."""
    folders = list_result_folders(results_root, audited_only=True)
    if not folders:
        raise FileNotFoundError(
            "No indexed Results folders containing audit CSVs were found under {}.".format(results_root)
        )

    plan_entries = load_interval_plans(plan_dir, pairs)
    assignments = assign_folders(folders, pairs, plan_entries)

    reverse_indexes = [folder.index for folder, stage, _ in assignments
                       if stage.direction_key == FORK_TO_UPSTREAM]
    first_reverse_index = min(reverse_indexes) if reverse_indexes else ""

    indexes_per_group = defaultdict(list)
    for folder, stage, _ in assignments:
        indexes_per_group[(stage.direction_key, stage.pair_index)].append(folder.index)
    for indexes in indexes_per_group.values():
        indexes.sort()

    manifest = []
    for folder, stage, entry in sorted(assignments, key=lambda item: (
            DIRECTIONS.index(item[1].direction_key), item[0].index, item[0].name)):
        group = indexes_per_group[(stage.direction_key, stage.pair_index)]
        entry = entry or {}
        manifest.append({
            "Result_Index": folder.index,
            "Result_Folder": folder.name,
            "Pair_List_Index": stage.pair_index,
            "Upstream": stage.upstream,
            "Fork": stage.fork,
            "Direction_Key": stage.direction_key,
            "Direction": DIRECTION_LABELS[stage.direction_key],
            "Source": stage.source,
            "Target": stage.target,
            "Expected_Source_Key": stage.expected_source_key,
            # These describe the audited folders found, not predicted intervals.
            "Interval_Number": group.index(folder.index) + 1,
            "Interval_Count": len(group),
            "Pair_Start_Index": min(group),
            "Pair_End_Index": max(group),
            "Start": folder.start or entry.get("start", ""),
            "End": folder.end or entry.get("end", ""),
            "Label": folder.label or entry.get("label", ""),
            "Starting_Flip_Index": first_reverse_index,
            "Attributed_By": PLAN_SOURCE_INTERVAL_PLAN if entry else PLAN_SOURCE_COLLECTION_ORDER,
        })
    return manifest


def validate_manifest(rows, pairs):
    """Sanity checks that do not rely on any numeric index boundary."""
    seen_folders = set()
    seen_indexes = set()

    for row in rows:
        index = int(row["Result_Index"])
        folder = row["Result_Folder"]
        if folder in seen_folders:
            raise ValueError("Data-division validation failed: duplicate result folder {}".format(folder))
        seen_folders.add(folder)

        direction_key = row["Direction_Key"]
        if direction_key not in DIRECTIONS:
            raise ValueError("Data-division validation failed: unknown direction {} "
                             "(result {})".format(direction_key, folder))

        # Each pass numbers its own results, so an index repeats at most across directions.
        if (direction_key, index) in seen_indexes:
            raise ValueError("Data-division validation failed: index {} occurs twice in direction "
                             "{} (second: {})".format(index, direction_key, folder))
        seen_indexes.add((direction_key, index))

        pair_index = int(row["Pair_List_Index"])
        if not 0 <= pair_index < len(pairs):
            raise ValueError("Data-division validation failed: pair index {} out of range "
                             "(result {})".format(pair_index, folder))

        expected_source, expected_target = source_and_target(pairs, pair_index, direction_key)
        if row.get("Source") != expected_source or row.get("Target") != expected_target:
            raise ValueError("Data-division validation failed: result {} has {} -> {}, expected "
                             "{} -> {}".format(folder, row.get("Source"), row.get("Target"),
                                               expected_source, expected_target))
    return True


def build_pair_summary(rows, pairs):
    summary = []
    for pair_index, (upstream, fork) in enumerate(pairs):
        entry = {"Pair_List_Index": pair_index, "Upstream": upstream, "Fork": fork}
        for direction_key, prefix in ((UPSTREAM_TO_FORK, "Upstream_to_Fork"),
                                      (FORK_TO_UPSTREAM, "Fork_to_Upstream")):
            indexes = sorted(int(row["Result_Index"]) for row in rows
                             if int(row["Pair_List_Index"]) == pair_index
                             and row["Direction_Key"] == direction_key)
            entry[prefix + "_Folders"] = len(indexes)
            entry[prefix + "_Start"] = indexes[0] if indexes else ""
            entry[prefix + "_End"] = indexes[-1] if indexes else ""
        summary.append(entry)
    return summary


class DivisionPlan:
    """The attributed result folders plus convenient lookups on them."""

    def __init__(self, rows, pairs):
        self.rows = rows
        self.pairs = pairs
        self.by_folder = {row["Result_Folder"]: row for row in rows}
        self.summary = build_pair_summary(rows, pairs)

    def rows_for(self, pair_index, direction_key):
        return sorted(
            (row for row in self.rows
             if int(row["Pair_List_Index"]) == int(pair_index) and row["Direction_Key"] == direction_key),
            key=lambda row: int(row["Result_Index"]),
        )

    def indexes(self, direction_key=None):
        return sorted(int(row["Result_Index"]) for row in self.rows
                      if direction_key is None or row["Direction_Key"] == direction_key)

    @property
    def starting_flip_index(self):
        """First result index of the reverse pass (None if there is none)."""
        reverse = self.indexes(FORK_TO_UPSTREAM)
        return reverse[0] if reverse else None


def create_division_plan(results_root=config.RESULTS_ROOT, pairs=config.VARIANT_PAIRS,
                         plan_dir=config.PICKLE_DIR):
    """Build, validate and save the division plan for the current Results folder."""
    results_root = Path(results_root)
    rows = build_manifest(results_root, pairs, plan_dir)
    validate_manifest(rows, pairs)

    manifest_path = results_root / "data_division_manifest.csv"
    write_rows(manifest_path, MANIFEST_FIELDS, rows, announce=False)
    from_plan = sum(1 for row in rows if row["Attributed_By"] == PLAN_SOURCE_INTERVAL_PLAN)
    print("Created: {} ({} audited result folders mapped: {} by interval plan, {} by collection "
          "order)".format(manifest_path, len(rows), from_plan, len(rows) - from_plan))

    plan = DivisionPlan(rows, pairs)
    summary_path = results_root / "pair_division_summary.csv"
    write_rows(summary_path, SUMMARY_FIELDS, plan.summary, announce=False)
    print("Created: {}".format(summary_path))
    return plan


def print_division_report(plan):
    """Same diagnostics as the original notebook cell."""
    all_indexes = plan.indexes()
    print()
    print("Mapped audited result folders = {}".format(len(plan.rows)))
    print("Observed result index range = {}..{}".format(all_indexes[0], all_indexes[-1]))

    forward = plan.indexes(UPSTREAM_TO_FORK)
    if forward:
        print("Forward audited indexes = {}..{} ({} audited folders)".format(
            forward[0], forward[-1], len(forward)))

    reverse = plan.indexes(FORK_TO_UPSTREAM)
    if reverse:
        print("Reverse audited indexes = {}..{} ({} audited folders)".format(
            reverse[0], reverse[-1], len(reverse)))
        print("First reverse-pass index = {}".format(plan.starting_flip_index))
    else:
        print("No reverse audited Results folders were discovered.")

    print()
    print("Pair audit-folder divisions:")
    for row in plan.summary:
        print("[{:02d}] {} -> {} | forward audits: {} [{}..{}] | reverse audits: {} [{}..{}]".format(
            row["Pair_List_Index"], row["Upstream"], row["Fork"],
            row["Upstream_to_Fork_Folders"], row["Upstream_to_Fork_Start"], row["Upstream_to_Fork_End"],
            row["Fork_to_Upstream_Folders"], row["Fork_to_Upstream_Start"], row["Fork_to_Upstream_End"],
        ))
