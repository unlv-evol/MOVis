"""
Step 6: technical lag of missed patches (Figures 7 and 8).

Definition used in the paper:

    technical lag = commit time of the target's HEAD at the experiment cutoff
                    - merge time of the source PR

Only MO hunk rows are candidates (the old SP class is already merged into
MO); CC/NE/EE files are never lag candidates.

    Figure 7  one point per MO patch observation, sorted by lag
    Figure 8  mean lag per target repository, sorted

The target HEAD is the latest commit on its default branch at or before the
direction's cutoff. The cutoff comes from the division plan when the folders
are dated, otherwise from PaReco (if available), and it is never later than
config.HARD_VARIANT_END. Observations with a negative lag (PR merged after
the target HEAD) and PRs without a merge date are left out but written to
their own CSV so nothing disappears silently.
"""

import statistics
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

from . import config
from .csv_io import write_rows
from .data_division import DIRECTIONS, source_and_target
from .github_api import ApiCache, GitHubClient, load_tokens
from .labels import normalize_class
from .pareco_probe import find_pareco_root, probe_pair_windows
from .results_layout import folders_by_name

LAG_FIELDS = [
    "Patch_Observation_Index", "Pair_List_Index", "Direction_Key",
    "Source_Repository", "Target_Repository", "Result_Index", "Result_Folder",
    "PR", "File", "Hunk_Number", "Patch_Classification",
    "Source_PR_Merged_At", "Target_Head_SHA", "Target_Head_Commit_Time",
    "Experiment_Cutoff", "Technical_Lag_Days", "Technical_Lag_Weeks",
]
NEGATIVE_LAG_FIELDS = [field for field in LAG_FIELDS if field != "Patch_Observation_Index"] + ["Ignored_Reason"]
TARGET_MEAN_FIELDS = ["Target_Variant_Index", "Target_Repository", "MO_Patch_Observations",
                      "Mean_Technical_Lag_Weeks"]
HEAD_FIELDS = ["Pair_List_Index", "Direction_Key", "Source_Repository", "Target_Repository",
               "Experiment_Cutoff", "Target_Head_SHA", "Target_Head_Commit_Time", "Target_Head_Source"]

SECONDS_PER_DAY = 86400.0


# ---------------------------------------------------------------------------
# Date helpers
# ---------------------------------------------------------------------------

def parse_timestamp(value):
    """ISO string -> aware UTC datetime (None for empty values)."""
    if value is None or not str(value).strip():
        return None
    text = str(value).strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    parsed = datetime.fromisoformat(text)
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def clamp_cutoff(value):
    """ISO UTC cutoff that is never later than the hard variant limit."""
    if value is None or not str(value).strip():
        return config.HARD_VARIANT_END_TIMESTAMP
    text = str(value).strip()
    if "T" not in text:
        text += "T23:59:59Z"
    parsed = parse_timestamp(text)
    if parsed is None or parsed > config.HARD_VARIANT_END:
        return config.HARD_VARIANT_END_TIMESTAMP
    return parsed.strftime("%Y-%m-%dT%H:%M:%SZ")


def _require_within_limit(value, context):
    parsed = parse_timestamp(value)
    if parsed is not None and parsed > config.HARD_VARIANT_END:
        raise ValueError("{} timestamp {} exceeds the hard variant limit {}.".format(
            context, value, config.HARD_VARIANT_END_TIMESTAMP))
    return parsed


def _commit_time(payload):
    """Committer (or author) date of a commit API payload."""
    commit = payload.get("commit", {}) if isinstance(payload, dict) else {}
    if not isinstance(commit, dict):
        return None
    for side in ("committer", "author"):
        person = commit.get(side, {})
        if isinstance(person, dict) and person.get("date"):
            return person["date"]
    return None


# ---------------------------------------------------------------------------
# Lag calculator
# ---------------------------------------------------------------------------

class TechnicalLagCalculator:
    """Fetches (and caches) PR merge times and target HEADs."""

    def __init__(self, plan, output_dir, tokens=None, head_overrides=None):
        self.plan = plan
        self.output_dir = Path(output_dir)
        self.head_overrides = head_overrides or {}

        self.pareco_root = find_pareco_root()
        if tokens is None:
            tokens = load_tokens(extra_dirs=[self.pareco_root] if self.pareco_root else [])
        self.tokens = tokens
        if not tokens:
            print("Warning: no GitHub token found; Figures 7/8 will use the much lower "
                  "anonymous API rate limit.")
        if self.pareco_root is None:
            print("Note: PaReco.py was not found, so undated directions use the hard "
                  "limit {} as their cutoff.".format(config.HARD_VARIANT_END_TIMESTAMP))

        self.api = GitHubClient(tokens)
        self.cache = ApiCache(self.output_dir / "technical_lag_api_cache.json")

    # -- cutoff ----------------------------------------------------------------

    def direction_cutoff(self, pair_index, direction_key):
        ends = [str(row.get("End", "")).strip() for row in self.plan.rows_for(pair_index, direction_key)]
        ends = [end for end in ends if end]
        if ends:
            return clamp_cutoff(max(ends))

        # Legacy folders do not encode their dates; ask PaReco if we can.
        if self.pareco_root is not None:
            source, target = source_and_target(self.plan.pairs, pair_index, direction_key)
            try:
                windows = probe_pair_windows(source, target, self.tokens)
                if windows:
                    return clamp_cutoff(max(w["end"] for w in windows if w.get("end")))
            except Exception as error:
                print("Warning: could not recover cutoff for pair {} {}: {}: {}".format(
                    pair_index, direction_key, type(error).__name__, error))

        # Never fall back to the current HEAD.
        return config.HARD_VARIANT_END_TIMESTAMP

    # -- GitHub lookups --------------------------------------------------------

    def pr_merge_time(self, source_repo, pr_number):
        # Ask GitHub under the current name of renamed repositories.
        source_repo = config.canonical_repo(source_repo)
        key = "{}#{}".format(source_repo, pr_number)
        cached = self.cache.pull_requests.get(key)
        if cached is not None:
            return parse_timestamp(cached.get("merged_at"))

        payload = self.api.get_json("/repos/{}/pulls/{}".format(source_repo, pr_number), allow_404=True)
        if payload is None:
            self.cache.pull_requests[key] = {"merged_at": None, "status": "404"}
        else:
            self.cache.pull_requests[key] = {
                "merged_at": payload.get("merged_at"),
                "merge_commit_sha": payload.get("merge_commit_sha"),
                "status": "ok",
            }
        self.cache.save()  # saved after every request so an interrupted run loses nothing
        return parse_timestamp(self.cache.pull_requests[key]["merged_at"])

    def target_head(self, target_repo, cutoff):
        """{sha, date, source, cutoff} of the target HEAD at the cutoff."""
        cutoff = clamp_cutoff(cutoff)
        override = self.head_overrides.get(target_repo)
        if override:
            return self._head_from_override(target_repo, override, cutoff)

        target_repo = config.canonical_repo(target_repo)
        cache_key = "{}|{}".format(target_repo, cutoff)
        cached = self.cache.target_heads.get(cache_key)
        if cached is not None:
            cached_date = parse_timestamp(cached.get("date"))
            if cached_date is not None and cached_date <= config.HARD_VARIANT_END:
                return {"sha": cached.get("sha", ""), "date": cached_date,
                        "source": cached.get("source", "cache"), "cutoff": cutoff}
            # A cached value beyond the hard limit is stale; fetch it again.

        commits = self.api.get_json("/repos/{}/commits".format(target_repo),
                                    params={"per_page": 1, "until": cutoff})
        if not isinstance(commits, list) or not commits:
            raise RuntimeError("Could not find a target commit for {} at/before cutoff {}.".format(
                target_repo, cutoff))
        date_text = _commit_time(commits[0])
        if not date_text:
            raise RuntimeError("Target commit {} has no commit timestamp.".format(commits[0].get("sha", "")))
        _require_within_limit(date_text, "Target HEAD")

        entry = {"sha": commits[0].get("sha", ""), "date": date_text,
                 "source": "github-latest-at-cutoff", "cutoff": cutoff}
        self.cache.target_heads[cache_key] = entry
        self.cache.save()
        return dict(entry, date=parse_timestamp(date_text))

    def _head_from_override(self, target_repo, override, cutoff):
        if override.get("date"):
            return {"sha": override.get("sha", ""),
                    "date": _require_within_limit(override["date"], "Override"),
                    "source": "override-date", "cutoff": cutoff}
        if override.get("sha"):
            payload = self.api.get_json("/repos/{}/commits/{}".format(target_repo, override["sha"]))
            return {"sha": payload.get("sha", override["sha"]),
                    "date": _require_within_limit(_commit_time(payload), "Override commit"),
                    "source": "override-sha", "cutoff": cutoff}
        raise ValueError("Override for {} needs a 'sha' or a 'date'.".format(target_repo))


def build_technical_lag_data(plan, descriptive, results_root=config.RESULTS_ROOT,
                             tokens=None, head_overrides=None):
    """Compute and save the data behind Figures 7 and 8."""
    results_root = Path(results_root)
    output_dir = results_root / "TechnicalLag"
    calculator = TechnicalLagCalculator(plan, output_dir, tokens, head_overrides)
    folders = folders_by_name(results_root)

    lag_rows, head_rows, missing_merges, negative_rows = [], [], [], []

    for pair_index in range(len(plan.pairs)):
        for direction_key in DIRECTIONS:
            source, target = source_and_target(plan.pairs, pair_index, direction_key)
            cutoff = calculator.direction_cutoff(pair_index, direction_key)
            head = calculator.target_head(target, cutoff)
            if head["date"] is None:
                raise RuntimeError("No target-head timestamp for {}.".format(target))

            head_rows.append({
                "Pair_List_Index": pair_index, "Direction_Key": direction_key,
                "Source_Repository": source, "Target_Repository": target,
                "Experiment_Cutoff": cutoff, "Target_Head_SHA": head["sha"],
                "Target_Head_Commit_Time": head["date"].isoformat(),
                "Target_Head_Source": head["source"],
            })

            for plan_row in plan.rows_for(pair_index, direction_key):
                folder = folders.get(plan_row["Result_Folder"])
                if folder is None:
                    continue
                for hunk in folder.audit_rows("hunk"):
                    if normalize_class(hunk.get("hunk_classification", "NA")) != "MO":
                        continue

                    pr = str(hunk.get("pr_of_hunk", "")).strip()
                    merged_at = calculator.pr_merge_time(source, pr)
                    if merged_at is None:
                        missing_merges.append({"Source_Repository": source, "PR": pr,
                                               "Result_Index": folder.index})
                        continue

                    seconds = (head["date"] - merged_at).total_seconds()
                    row = {
                        "Pair_List_Index": pair_index, "Direction_Key": direction_key,
                        "Source_Repository": source, "Target_Repository": target,
                        "Result_Index": folder.index, "Result_Folder": folder.name,
                        "PR": pr, "File": hunk.get("file_of_hunk", ""),
                        "Hunk_Number": hunk.get("hunk_number", ""),
                        "Patch_Classification": "MO",
                        "Source_PR_Merged_At": merged_at.isoformat(),
                        "Target_Head_SHA": head["sha"],
                        "Target_Head_Commit_Time": head["date"].isoformat(),
                        "Experiment_Cutoff": cutoff,
                        "Technical_Lag_Days": seconds / SECONDS_PER_DAY,
                        "Technical_Lag_Weeks": seconds / (SECONDS_PER_DAY * 7.0),
                    }
                    if seconds < 0:
                        # PR merged after the target HEAD: not a meaningful lag.
                        row["Ignored_Reason"] = "negative technical lag"
                        negative_rows.append(row)
                    else:
                        lag_rows.append(row)

    _check_mo_population(descriptive, lag_rows, missing_merges, negative_rows)

    lag_rows.sort(key=lambda r: float(r["Technical_Lag_Weeks"]))
    for position, row in enumerate(lag_rows, start=1):
        row["Patch_Observation_Index"] = position

    target_rows = _mean_lag_per_target(lag_rows)

    paths = {
        "figure7": write_rows(output_dir / "figure7_patch_technical_lag.csv", LAG_FIELDS, lag_rows),
        "figure8": write_rows(output_dir / "figure8_target_variant_mean_technical_lag.csv",
                              TARGET_MEAN_FIELDS, target_rows),
        "heads": write_rows(output_dir / "technical_lag_target_heads.csv", HEAD_FIELDS, head_rows),
        "missing": write_rows(output_dir / "technical_lag_missing_pr_merge_dates.csv",
                              ["Source_Repository", "PR", "Result_Index"], missing_merges),
        "negative": write_rows(output_dir / "technical_lag_negative_ignored.csv",
                               NEGATIVE_LAG_FIELDS, negative_rows),
    }

    patch_median = statistics.median([float(r["Technical_Lag_Weeks"]) for r in lag_rows]) if lag_rows else None
    target_median = (statistics.median([float(r["Mean_Technical_Lag_Weeks"]) for r in target_rows])
                     if target_rows else None)
    print("Figure 7 MO patch observations with lag: {}".format(len(lag_rows)))
    print("Figure 7 median technical lag (weeks): {}".format(patch_median))
    print("Figure 8 target variants: {}".format(len(target_rows)))
    print("Figure 8 median of target mean lags (weeks): {}".format(target_median))
    if missing_merges:
        print("Warning: {} MO observations have no source PR merged_at; see {}.".format(
            len(missing_merges), paths["missing"]))
    if negative_rows:
        print("Ignored {} negative technical-lag observations; see {}.".format(
            len(negative_rows), paths["negative"]))

    return {
        "lag_rows": lag_rows,
        "target_rows": target_rows,
        "negative_rows": negative_rows,
        "missing_rows": missing_merges,
        "patch_median_weeks": patch_median,
        "target_median_weeks": target_median,
        "paths": paths,
        "output_dir": output_dir,
    }


def _check_mo_population(descriptive, lag_rows, missing_merges, negative_rows):
    """Every global MO hunk must end up in exactly one of the three lists."""
    expected = sum(1 for row in descriptive.audits["hunk"]["global_rows"]
                   if normalize_class(row.get("hunk_classification", "NA")) == "MO")
    accounted = len(lag_rows) + len(missing_merges) + len(negative_rows)
    if accounted != expected:
        raise RuntimeError(
            "Figures 7/8 MO reconciliation failed: {} lag rows + {} missing merge dates + "
            "{} ignored negative lags != {} global MO hunk rows.".format(
                len(lag_rows), len(missing_merges), len(negative_rows), expected))


def _mean_lag_per_target(lag_rows):
    weeks_per_target = defaultdict(list)
    for row in lag_rows:
        weeks_per_target[row["Target_Repository"]].append(float(row["Technical_Lag_Weeks"]))

    rows = [{"Target_Repository": target,
             "MO_Patch_Observations": len(weeks),
             "Mean_Technical_Lag_Weeks": sum(weeks) / float(len(weeks))}
            for target, weeks in weeks_per_target.items()]
    rows.sort(key=lambda r: float(r["Mean_Technical_Lag_Weeks"]))
    for position, row in enumerate(rows, start=1):
        row["Target_Variant_Index"] = position
    return rows
