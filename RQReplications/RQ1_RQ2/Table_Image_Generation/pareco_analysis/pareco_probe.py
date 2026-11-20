"""
Optional use of PaReco itself to recover a pair's analysis period.

Legacy Results folders are named only <index>_<repo>, so they do not record
the date interval that was analysed. For the technical-lag figures we need
the cutoff date of each direction; if PaReco.py is available we ask it for
the divergence and cutoff dates and rebuild the six-month windows.

This is never used to decide result indexes or pair attribution.
"""

import os
import sys
from calendar import monthrange
from datetime import date, datetime, timedelta
from pathlib import Path

from . import config


def as_date(value):
    if isinstance(value, datetime):
        return value.date()
    if isinstance(value, date):
        return value
    text = str(value).strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    return datetime.fromisoformat(text).date()


def add_calendar_months(day, months):
    """Add months to a date, clamping the day to the end of the month."""
    month_index = day.month - 1 + int(months)
    year = day.year + month_index // 12
    month = month_index % 12 + 1
    return day.replace(year=year, month=month, day=min(day.day, monthrange(year, month)[1]))


def build_six_month_windows(diverge_date, cut_off_date):
    """Split [diverge_date, cutoff] into six-month windows.

    The cutoff is never later than config.HARD_VARIANT_END.
    """
    current = as_date(diverge_date)
    final = min(as_date(cut_off_date), config.HARD_VARIANT_END.date())
    if current > final:
        raise ValueError("The divergence date must not be later than the effective "
                         "cutoff date ({:%Y-%m-%d}).".format(final))

    windows = []
    while current <= final:
        end = min(add_calendar_months(current, 6) - timedelta(days=1), final)
        windows.append({
            "label": "{:%Y_%m_%d}_{:%Y_%m_%d}".format(current, end),
            "start": "{:%Y-%m-%d}T00:00:00Z".format(current),
            "end": "{:%Y-%m-%d}T23:59:59Z".format(end),
        })
        current = end + timedelta(days=1)
    return windows


def find_pareco_root():
    """PaReco project folder (holds Examples/PaReco.py and Methods/), or None."""
    candidates = [config.PARECO_ROOT] + list(config.PARECO_SEARCH_PATHS) + [Path.cwd(), Path.cwd().parent]
    for candidate in candidates:
        if (candidate / "Examples" / "PaReco.py").exists() and (candidate / "Methods").is_dir():
            return candidate.resolve()
    return None


def _load_pareco_class(pareco_root):
    # PaReco.py imports Methods.* and is itself imported as Examples.PaReco.
    if str(pareco_root) not in sys.path:
        sys.path.insert(0, str(pareco_root))
    from Examples.PaReco import PaReco
    return PaReco


def probe_pair_windows(source, target, tokens):
    """Ask PaReco for the divergence/cutoff dates of source -> target.

    PaReco resolves renamed repositories and falls back to repository
    metadata for pairs without a merge base, exactly as in the data run.
    """
    pareco_root = find_pareco_root()
    if pareco_root is None:
        raise FileNotFoundError("Could not locate PaReco (Examples/PaReco.py) for optional date probing.")

    PaReco = _load_pareco_class(pareco_root)

    # PaReco uses paths relative to its project folder, so run it from there.
    original_cwd = Path.cwd()
    try:
        os.chdir(str(pareco_root))
        probe = PaReco(("0", source, target, "", "", tokens))
        probe.getDates()
        return build_six_month_windows(probe.diverge_date, probe.cut_off_date)
    finally:
        os.chdir(str(original_cwd))
