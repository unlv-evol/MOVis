"""
Helpers for the layout of the Results/ folder.

Result folders are named

    <index>_<source_repo>                              (legacy runs)
    <index>_<source_repo>_YYYY_MM_DD_YYYY_MM_DD        (dated runs)

where <source_repo> is owner/repo with "/" replaced by "_". The index is the
order in which PaReco produced the result; nothing else about the numbering
(start, end, gaps) is assumed anywhere in this project. The upstream -> fork
and fork -> upstream passes may each have their own numbering, so an index is
only unique within one direction: folders are identified by their name.
"""

import re
from dataclasses import dataclass
from pathlib import Path

from . import config
from .csv_io import AUDIT_FILE_NAMES, AUDIT_SPECS, read_audit_csv

_FOLDER_PATTERN = re.compile(r"^(\d+)_(.+)$")
_DATED_SUFFIX = re.compile(r"^(.*)_(\d{4}_\d{2}_\d{2})_(\d{4}_\d{2}_\d{2})$")


def repo_key(repo_name):
    """'owner/repo' -> 'owner_repo', the form used in folder names."""
    return str(repo_name).strip().replace("/", "_")


def match_key(text):
    """Comparison form of a repo name or folder source key.

    GitHub names are case-insensitive, and GACPD writes '-' as '_' in its
    folder names, so both are folded.
    """
    return repo_key(text).lower().replace("-", "_")


def repo_match_keys(repo_name):
    """Keys under which a VARIANT_PAIRS repository can appear in folder names:
    its listed name and, if it was renamed, its canonical name."""
    return {match_key(repo_name), match_key(config.canonical_repo(repo_name))}


@dataclass(frozen=True)
class ResultFolder:
    index: int
    source_key: str
    path: Path
    label: str = ""  # "YYYY_MM_DD_YYYY_MM_DD" for dated folders
    start: str = ""  # ISO timestamps of the interval, if known
    end: str = ""

    @property
    def name(self):
        return self.path.name

    def has_audits(self):
        return any((self.path / name).exists() for name in AUDIT_FILE_NAMES)

    def audit_rows(self, kind):
        """Rows of one of this folder's audit files ('classification', 'file', 'hunk')."""
        spec = AUDIT_SPECS[kind]
        return read_audit_csv(self.path / spec.local_name, spec.columns)


def parse_result_folder(path):
    """Build a ResultFolder from a folder path, or return None if it is not one."""
    path = Path(path)
    match = _FOLDER_PATTERN.match(path.name)
    if not match:
        return None

    index = int(match.group(1))
    remainder = match.group(2)

    dated = _DATED_SUFFIX.match(remainder)
    if not dated:
        return ResultFolder(index=index, source_key=remainder, path=path)

    first_day, last_day = dated.group(2), dated.group(3)
    return ResultFolder(
        index=index,
        source_key=dated.group(1),
        path=path,
        label="{}_{}".format(first_day, last_day),
        start=first_day.replace("_", "-") + "T00:00:00Z",
        end=last_day.replace("_", "-") + "T23:59:59Z",
    )


def list_result_folders(results_root=config.RESULTS_ROOT, audited_only=False):
    """All indexed result folders, sorted by index (then name)."""
    results_root = Path(results_root)
    if not results_root.exists():
        return []

    folders = []
    for path in results_root.iterdir():
        if not path.is_dir():
            continue
        folder = parse_result_folder(path)
        if folder is None:
            continue
        if audited_only and not folder.has_audits():
            continue
        folders.append(folder)
    return sorted(folders, key=lambda folder: (folder.index, folder.name))


def folders_by_name(results_root=config.RESULTS_ROOT):
    return {folder.name: folder for folder in list_result_folders(results_root)}
