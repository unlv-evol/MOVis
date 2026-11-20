"""
Normalisation of PaReco classification labels.

PaReco (and older versions of our export code) used a handful of spellings for
the same class. Everything that leaves this module is one of the six final
labels in config.CLASS_ORDER:

    MO  missing operation   (buggy code still present in the target)
    ED  effort duplication  (fixed code already present in the target)
    NA  not applicable
    CC  cannot classify     (file type PaReco does not process)
    NE  not existing        (file does not exist in the target)
    EE  error               (PaReco failed on the file)

The old split class SP is merged into MO, and NI into NA.
"""

from .config import CLASS_ORDER

VALID_CLASSES = frozenset(CLASS_ORDER)

_ALIASES = {
    "SP": "MO",
    "NI": "NA",
    "N/A": "NA",
    "NOT APPLICABLE": "NA",
    "NOT_APPLICABLE": "NA",
    "OTHER EXT": "CC",
    "OTHER EXTENSION": "CC",
    "OTHER_EXT": "CC",
    "OE": "CC",
    "NOT EXISTING": "NE",
    "NOT_EXISTING": "NE",
    "ERROR": "EE",
    "E": "EE",
}


def normalize_class(value):
    """Map any raw PaReco label onto one of the six final labels.

    Missing values and anything unrecognised become "NA".
    """
    label = "NA" if value is None else str(value).strip().upper()
    label = _ALIASES.get(label, label)
    return label if label in VALID_CLASSES else "NA"


def collapse_hunk_sides(buggy_class, patch_class):
    """Combine the buggy-side and patched-side results of one hunk.

    PaReco checks each hunk twice: once against the buggy lines and once
    against the patched lines. The final hunk label is:
      * MO if the buggy side matched (this also absorbs the old SP case)
      * otherwise ED if the patched side matched
      * otherwise NA
    """
    observed = {
        str(value).strip().upper()
        for value in (buggy_class, patch_class)
        if value is not None and str(value).strip()
    }
    if "MO" in observed:
        return "MO"
    if "ED" in observed:
        return "ED"
    return "NA"
