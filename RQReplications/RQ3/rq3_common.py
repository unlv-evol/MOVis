"""Shared loading code for the RQ3 scripts.

Two tools classified the same pull requests:

  GACPD   Results/Repos_results/<run>_<#>_<owner>_<name>_<YYYY_MM_DD>_<YYYY_MM_DD>/
          classification_audit.csv  (PR level)   file_audit.csv  (file level)
  PaReco  Results/PaReco_Results/<i>_<owner>_<name>_results.pkl
          data[0] = {pr: {file: {"result": {...}}}}   data[1] = {pr: {"class": ...}}

PaReco totals always come from the pickles in PaReco_Results; every pickle is
counted. To compare the tools, each pickle is also paired with the GACPD folder
that covers the same repo and date window:
  1. RQ3/pareco_windows.csv (built by rq3_build_pareco_map.py) gives each pickle's
     repo and start/end dates; the pickle pairs with the GACPD folder that has the
     same repo + dates, whatever the run is called or numbered.
  2. Otherwise: the GACPD folder of the same repo that contains all its PRs.
  3. Otherwise the pickle stays on its own (run "PaReco_only") and is still counted.
A GACPD window without a pickle means PaReco produced no result for it.

Dataset rule (same as scripts/aggregate_repo_results.py in MOVis):
  A long repository history is split into consecutive date windows, one folder
  each. A *dataset* is one unbroken series of those windows for the same
  run + repo: the next folder's # is the previous # + 1 and its start date is
  the day after the previous end date. A repo listed twice in a run becomes two
  datasets that are never merged. A PR is counted once per dataset (in the
  first window it appears in).

Label rule: SP is counted as MO, and "NA ADDED" as NA.
"""

from __future__ import annotations

import csv
import datetime as dt
import json
import pickle
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
PACKAGE_ROOT = HERE.parent
DEFAULT_RESULTS = PACKAGE_ROOT / "Results"
DEFAULT_OUT = HERE / "output"

GACPD = "GACPD"
PARECO = "PaReco"
TOOLS = (GACPD, PARECO)

GACPD_DIR = "Repos_results"
PARECO_DIR = "PaReco_Results"

# pickle -> repo + date window, so pickles pair with GACPD folders by dates, not folder numbers.
PARECO_MAP = HERE / "pareco_windows.csv"
PARECO_MAP_COLUMNS = ["pickle", "pareco_index", "repo", "start", "end", "source_folder", "prs_match"]
PARECO_ONLY_RUN = "PaReco_only"

PR_CLASS_ORDER = ["MO", "ED", "CC", "NE", "NA", "ERROR"]

FOLDER_RE = re.compile(
    r"^(?P<run>.+?)_(?P<idx>\d+)_(?P<repo>.+)_"
    r"(?P<start>\d{4}_\d{2}_\d{2})_(?P<end>\d{4}_\d{2}_\d{2})$"
)
PR_DIR_RE = re.compile(r"^(?P<pr>\d+)_(?P<cls>.+)$")
PKL_RE = re.compile(r"^(?P<idx>\d+)_(?P<repo>.+)_results\.pkl$")

_ALIASES = {
    "SP": "MO",          # split patch counts as missing operation
    "NA ADDED": "NA",    # MOVis counts "NA ADDED" as NA
    "EE": "ERROR",
    "E": "ERROR",
    "OTHER EXT": "CC",
    "OE": "CC",
    "NOT EXISTING": "NE",
}


def normalize(label) -> str:
    """PR-level label with SP -> MO applied. Blank stays visible as '(blank)'."""
    label = "" if label is None else str(label).strip().upper()
    if not label:
        return "(blank)"
    return _ALIASES.get(label, label)


def order_labels(labels) -> list[str]:
    labels = set(labels)
    return [c for c in PR_CLASS_ORDER if c in labels] + sorted(labels - set(PR_CLASS_ORDER))


def parse_date(s: str) -> dt.date:
    return dt.date(*map(int, s.split("_")))


def file_extension(name: str) -> str:
    suffix = Path(name).suffix.lower()
    return suffix if suffix else "(no extension)"


# ---------------------------------------------------------------------------
# PaReco pickles
# ---------------------------------------------------------------------------

class _Stub:
    """Stand-in for PaReco classes (SourceLoader, PatchInfo, bitarray) we never read."""

    def __init__(self, *args, **kwargs):
        pass

    def __setstate__(self, state):
        pass


def _stub_function(*args, **kwargs):
    return None


class _PaRecoUnpickler(pickle.Unpickler):
    """Loads PaReco pickles without needing PaReco's Methods package or bitarray."""

    STUBBED = ("Methods", "bitarray", "SourceLoader", "PatchInfo", "__main__")

    def find_class(self, module, name):
        if module.split(".")[0] in self.STUBBED:
            if name.startswith("_"):
                return _stub_function
            return type(name, (_Stub,), {})
        return super().find_class(module, name)


def load_pareco_pickle(path: Path):
    with path.open("rb") as fh:
        return _PaRecoUnpickler(fh).load()


def pareco_pr_classes(data) -> dict[str, str]:
    """PR -> raw PaReco class, for every PR in the pickle."""
    raw = data[0] if data and isinstance(data[0], dict) else {}
    summary = data[1] if len(data) > 1 and isinstance(data[1], dict) else {}
    out = {}
    for pr in list(raw.keys()) + [k for k in summary.keys() if k not in raw]:
        entry = summary.get(pr, {})
        out[str(pr)] = entry.get("class", "NA") if isinstance(entry, dict) else "NA"
    return out


def pareco_file_rows(data) -> list[tuple[str, str, str]]:
    """(pr, file name, raw file class) for every file PaReco listed."""
    raw = data[0] if data and isinstance(data[0], dict) else {}
    rows = []
    for pr, files in raw.items():
        if not isinstance(files, dict):
            continue
        for name, payload in files.items():
            result = payload.get("result", {}) if isinstance(payload, dict) else {}
            if isinstance(result, dict):
                cls = result.get("patchClass", "NA")
            elif isinstance(result, str):
                cls = result
            else:
                cls = "ERROR"
            rows.append((str(pr), str(name), str(cls)))
    return rows


def pareco_repos(data) -> tuple[str | None, str | None]:
    """(source owner/name, target owner/name) read from the raw GitHub URLs."""
    raw = data[0] if data and isinstance(data[0], dict) else {}
    for files in raw.values():
        if not isinstance(files, dict):
            continue
        for payload in files.values():
            result = payload.get("result") if isinstance(payload, dict) else None
            if not isinstance(result, dict):
                continue
            src = result.get("fileBeforePatchUrl") or result.get("fileAfterPatchUrl")
            dst = result.get("destUrl")
            if src:
                to_repo = lambda url: "/".join(url.split("/")[3:5]) if url else None
                return to_repo(src), to_repo(dst)
    return None, None


def repo_key(name: str) -> str:
    """Compare repo names ignoring case, '-', '_' and '.' (folders use '_', pickles keep '-')."""
    return re.sub(r"[^a-z0-9]", "", name.lower())


def load_pareco_map(path: Path = PARECO_MAP) -> dict[str, dict]:
    """pickle file name -> {repo, start, end, ...} from pareco_windows.csv (empty if missing)."""
    if not Path(path).is_file():
        return {}
    with Path(path).open(newline="", encoding="utf-8-sig") as fh:
        return {row["pickle"]: row for row in csv.DictReader(fh)}


# ---------------------------------------------------------------------------
# Windows and datasets
# ---------------------------------------------------------------------------

@dataclass
class Window:
    path: Path
    run: str
    idx: int
    repo: str
    start: dt.date
    end: dt.date
    divergent: str | None = None
    # tool -> {pr: raw class}; None when that tool has no result for this window
    prs: dict[str, dict[str, str] | None] = field(default_factory=dict)
    # tool -> [(pr, file name, raw class)]
    files: dict[str, list[tuple[str, str, str]]] = field(default_factory=dict)
    pareco_pickle: Path | None = None
    source_repo: str | None = None  # owner/name the PRs belong to, from the PaReco pickle
    target_repo: str | None = None  # owner/name they were compared against

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def days(self) -> int:
        return (self.end - self.start).days + 1


@dataclass
class Dataset:
    run: str
    repo: str
    windows: list[Window] = field(default_factory=list)

    @property
    def start(self) -> dt.date:
        return self.windows[0].start

    @property
    def end(self) -> dt.date:
        return self.windows[-1].end

    @property
    def divergent(self) -> str:
        return ";".join(sorted({w.divergent for w in self.windows if w.divergent}))

    @property
    def source_repo(self) -> str | None:
        """owner/name the PRs belong to: from a PaReco pickle, else GACPD's results.json 'mainline'."""
        found = next((w.source_repo for w in self.windows if w.source_repo), None)
        if found:
            return found
        for w in self.windows:
            if w.run != PARECO_ONLY_RUN:
                found = _mainline(w.path)
                if found:
                    return found
        return None

    @property
    def target_repo(self) -> str | None:
        return next((w.target_repo for w in self.windows if w.target_repo), None)

    @property
    def name(self) -> str:
        return f"{self.run}_{self.repo}_{self.start:%Y_%m_%d}_{self.end:%Y_%m_%d}"

    def pr_classes(self, tool: str, windows: list[Window] | None = None) -> dict[str, tuple[str, str]]:
        """PR -> (normalized class, window name), each PR counted once (first window wins)."""
        out: dict[str, tuple[str, str]] = {}
        for w in windows if windows is not None else self.windows:
            for pr, cls in (w.prs.get(tool) or {}).items():
                if pr not in out:
                    out[pr] = (normalize(cls), w.name)
        return out

    def duplicate_prs(self, tool: str) -> set[str]:
        seen, dup = set(), set()
        for w in self.windows:
            for pr in w.prs.get(tool) or {}:
                (dup if pr in seen else seen).add(pr)
        return dup

    def file_rows(self, tool: str) -> list[tuple[str, str, str]]:
        """File rows of PRs counted for this dataset, skipping repeats of already-counted PRs."""
        seen: set[str] = set()
        rows = []
        for w in self.windows:
            window_prs = set((w.prs.get(tool) or {}).keys())
            for pr, name, cls in w.files.get(tool, []):
                if pr in seen:  # PR already counted in an earlier window
                    continue
                rows.append((pr, name, cls))
            seen |= window_prs
        return rows


def _read_rows(csv_path: Path) -> list[list[str]]:
    with csv_path.open(newline="", encoding="utf-8-sig") as fh:
        rows = list(csv.reader(fh))
    return [r for r in rows[1:] if r]


_MAINLINE_CACHE: dict[Path, str | None] = {}


def _mainline(folder: Path) -> str | None:
    """'mainline' (owner/name of the source repo) from the first GACPD results.json in a window."""
    if folder not in _MAINLINE_CACHE:
        value = None
        if folder.is_dir():
            for results in folder.glob("*/*/*/results.json"):
                try:
                    value = json.loads(results.read_text(encoding="utf-8")).get("mainline")
                except (OSError, ValueError, AttributeError):
                    continue
                if value:
                    break
        _MAINLINE_CACHE[folder] = value or None
    return _MAINLINE_CACHE[folder]


def _find_divergent(folder: Path) -> str | None:
    for results in folder.glob("*/*/*/results.json"):
        try:
            value = json.loads(results.read_text(encoding="utf-8")).get("divergentRepo")
        except (OSError, ValueError, AttributeError):
            continue
        if value:
            return value
    return None


def _load_gacpd(w: Window, with_files: bool, warnings: list[str]) -> None:
    pr_csv = w.path / "classification_audit.csv"
    if pr_csv.is_file():
        w.prs[GACPD] = {r[0]: r[1] for r in _read_rows(pr_csv) if len(r) >= 2}
    else:
        from_dirs = {}
        for sub in w.path.iterdir():
            m = PR_DIR_RE.match(sub.name) if sub.is_dir() else None
            if m:
                from_dirs[m["pr"]] = m["cls"]
        w.prs[GACPD] = from_dirs or None
        if from_dirs:
            warnings.append(f"{w.name}: no classification_audit.csv, used PR folder names")
    if with_files:
        f_csv = w.path / "file_audit.csv"
        rows = []
        if f_csv.is_file():
            for r in _read_rows(f_csv):
                if len(r) >= 4:  # file_name, file_classification, file_extensions, pr_of_file
                    rows.append((r[-1], ",".join(r[:-3]), r[-3]))
        w.files[GACPD] = rows


def _attach_pareco(windows: dict[tuple[str, int], Window], pareco_root: Path, with_files: bool,
                   quiet: bool, warnings: list[str]) -> None:
    """Read every pickle in pareco_root and attach it to a GACPD window (or its own window)."""
    pmap = load_pareco_map()
    if not pmap:
        warnings.append(f"{PARECO_MAP.name} not found: pairing PaReco pickles by repo name + PRs only "
                        f"(run rq3_build_pareco_map.py to create it)")
    by_dates: dict[tuple[str, dt.date, dt.date], list[Window]] = defaultdict(list)
    by_repo: dict[str, list[Window]] = defaultdict(list)
    for w in windows.values():
        by_dates[(repo_key(w.repo), w.start, w.end)].append(w)
        by_repo[repo_key(w.repo)].append(w)

    pickles = sorted(pareco_root.glob("*.pkl"),
                     key=lambda p: (int(p.name.split("_")[0]) if p.name.split("_")[0].isdigit() else 10**9, p.name))
    live = not quiet and sys.stdout.isatty()
    if not quiet and not live:
        print(f"Reading {len(pickles)} PaReco pickles from {pareco_root} ...", flush=True)
    how = Counter()
    raw_prs = 0
    for i, pkl in enumerate(pickles, 1):
        if live:
            print(f"\rReading PaReco pickles {i}/{len(pickles)}", end="", flush=True)
        try:
            data = load_pareco_pickle(pkl)
        except Exception as exc:  # corrupt pickle: report, keep going
            warnings.append(f"{pkl.name}: could not be read ({exc})")
            continue
        prs = pareco_pr_classes(data)
        raw_prs += len(prs)
        m = PKL_RE.match(pkl.name)
        pkl_idx = int(m["idx"]) if m else 10**6 + i
        info = pmap.get(pkl.name)
        repo = info["repo"] if info else (m["repo"] if m else pkl.stem)

        # 1. same repo + same date window
        target = None
        if info:
            start, end = parse_date(info["start"].replace("-", "_")), parse_date(info["end"].replace("-", "_"))
            cands = by_dates.get((repo_key(repo), start, end), [])
            if cands:
                target = max(cands, key=lambda w: len(set(w.prs.get(GACPD) or {}) & set(prs)))
                how["repo + dates"] += 1
        # 2. same repo, GACPD window that holds every PR of the pickle
        if target is None and prs:
            names = {repo_key(repo)} | ({repo_key(m["repo"])} if m else set())
            cands = [w for n in names for w in by_repo.get(n, [])
                     if set(prs) <= set(w.prs.get(GACPD) or {})]
            if cands:
                target = min(cands, key=lambda w: len(w.prs.get(GACPD) or {}))
                how["repo + PRs"] += 1
                warnings.append(f"{pkl.name}: no GACPD folder with its repo + dates; "
                                f"paired with {target.name} (holds all its PRs)")
        # 3. on its own, still counted
        if target is None:
            start = end = dt.date(1900, 1, 1)
            if info:
                start, end = parse_date(info["start"].replace("-", "_")), parse_date(info["end"].replace("-", "_"))
            target = Window(pkl, PARECO_ONLY_RUN, pkl_idx, re.sub(r"[^A-Za-z0-9]", "_", repo), start, end)
            windows[(PARECO_ONLY_RUN, pkl_idx)] = target
            how["not paired"] += 1
            warnings.append(f"{pkl.name}: no matching GACPD folder; counted on its own ({len(prs)} PR(s))")

        if target.prs.get(PARECO):
            warnings.append(f"{pkl.name}: {target.name} already has PaReco results from "
                            f"{target.pareco_pickle.name}; PRs merged")
            target.prs[PARECO].update(prs)
            if with_files:
                target.files.setdefault(PARECO, []).extend(pareco_file_rows(data))
            continue
        target.pareco_pickle = pkl
        target.prs[PARECO] = prs
        target.source_repo, target.target_repo = pareco_repos(data)
        if with_files:
            target.files[PARECO] = pareco_file_rows(data)
    if live:
        print()
    if not quiet:
        print(f"PaReco: {len(pickles)} pickles, {raw_prs} PRs (before de-duplication) - paired with GACPD by "
              + ", ".join(f"{k}: {v}" for k, v in how.items()))


def scan(results_root: Path = DEFAULT_RESULTS, with_files: bool = False,
         tools=TOOLS, find_divergent: bool = False, quiet: bool = False):
    """Load every window of both tools. Returns (windows, warnings)."""
    results_root = Path(results_root)
    gacpd_root = results_root / GACPD_DIR
    pareco_root = results_root / PARECO_DIR
    warnings: list[str] = []

    windows: dict[tuple[str, int], Window] = {}
    for entry in sorted(gacpd_root.iterdir()):
        if not entry.is_dir():
            continue
        m = FOLDER_RE.match(entry.name)
        if not m:
            warnings.append(f"skipped folder not matching the naming pattern: {entry.name}")
            continue
        w = Window(entry, m["run"], int(m["idx"]), m["repo"],
                   parse_date(m["start"]), parse_date(m["end"]))
        if find_divergent:
            w.divergent = _find_divergent(entry)
        windows[(w.run, w.idx)] = w

    if GACPD in tools:
        for w in windows.values():
            _load_gacpd(w, with_files, warnings)

    if PARECO in tools and pareco_root.is_dir():
        _attach_pareco(windows, pareco_root, with_files, quiet, warnings)

    if GACPD in tools and PARECO in tools:
        only_gacpd: dict[tuple[str, str], list[int]] = defaultdict(list)
        for w in sorted(windows.values(), key=lambda w: (w.run, w.idx)):
            g, p = w.prs.get(GACPD), w.prs.get(PARECO)
            if g and not p:
                only_gacpd[(w.run, w.repo)].append(len(g))
            elif p and not g and w.run != PARECO_ONLY_RUN:
                warnings.append(f"{w.name}: {len(p)} PR(s) in {PARECO}, no {GACPD} classification")
        for (run, repo), sizes in only_gacpd.items():
            warnings.append(f"{run} {repo}: {sum(sizes)} PR(s) in {len(sizes)} window(s) with a "
                            f"{GACPD} result but no {PARECO} pickle")

    return list(windows.values()), warnings


def _continues(prev: Window, nxt: Window) -> bool:
    if nxt.idx != prev.idx + 1:
        return False
    if nxt.start != prev.end + dt.timedelta(days=1):
        return False
    if prev.divergent and nxt.divergent and prev.divergent != nxt.divergent:
        return False
    return True


def group_datasets(windows: list[Window]) -> list[Dataset]:
    by_repo: dict[tuple[str, str], list[Window]] = defaultdict(list)
    for w in windows:
        by_repo[(w.run, w.repo)].append(w)

    datasets: list[Dataset] = []
    for (run, repo), ws in by_repo.items():
        ws.sort(key=lambda w: w.idx)
        current = Dataset(run, repo, [ws[0]])
        for w in ws[1:]:
            if _continues(current.windows[-1], w):
                current.windows.append(w)
            else:
                datasets.append(current)
                current = Dataset(run, repo, [w])
        datasets.append(current)

    datasets.sort(key=lambda d: (d.run, d.windows[0].idx))
    return datasets


def load_datasets(results_root: Path = DEFAULT_RESULTS, **kwargs):
    windows, warnings = scan(results_root, **kwargs)
    return group_datasets(windows), warnings


# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

def write_csv(path: Path, header: list, rows: list[list]) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        wr = csv.writer(fh)
        wr.writerow(header)
        wr.writerows(rows)
    return path


def format_table(header: list, rows: list[list], first_left: bool = True) -> str:
    cells = [[str(c) for c in header]] + [[str(c) for c in r] for r in rows]
    widths = [max(len(r[i]) for r in cells) for i in range(len(header))]
    fmt = lambda r: "  ".join(
        v.ljust(widths[i]) if (i == 0 and first_left) else v.rjust(widths[i]) for i, v in enumerate(r))
    lines = [fmt(cells[0]), "  ".join("-" * w for w in widths)]
    lines += [fmt(r) for r in cells[1:]]
    return "\n".join(lines)


def markdown_table(header: list, rows: list[list]) -> str:
    out = ["| " + " | ".join(str(h) for h in header) + " |",
           "|" + "|".join(["---"] + ["---:"] * (len(header) - 1)) + "|"]
    out += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    return "\n".join(out)


def pct(part: int, whole: int) -> str:
    return f"{100 * part / whole:.2f}" if whole else "0.00"


def print_warnings(warnings: list[str], limit: int = 40) -> None:
    if not warnings:
        return
    print(f"\nWarnings ({len(warnings)}):")
    for w in warnings[:limit]:
        print(f"  - {w}")
    if len(warnings) > limit:
        print(f"  ... {len(warnings) - limit} more")


def counter_of(prs: dict[str, tuple[str, str]]) -> Counter:
    return Counter(cls for cls, _ in prs.values())
