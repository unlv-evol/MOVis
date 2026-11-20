"""
Project-wide settings for the PaReco results analysis.

Everything that a person might reasonably want to change between runs lives
here: input/output locations, the list of studied variant pairs and the hard
date limit used by the technical-lag figures.

All default paths are resolved relative to this project folder so that the
analysis keeps working when the folder is copied to another machine.
"""

from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

# Folder that contains run_analysis.py / analysis.ipynb.
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Where all per-folder audits, global audits, tables and figures are written.
RESULTS_ROOT = PROJECT_ROOT / "Results_"

# The PaReco project (contains Examples/PaReco.py and Methods/).
PARECO_ROOT = PROJECT_ROOT.parent

# Raw PaReco result pickles, written by PaReco to Examples/Repos_results.
PICKLE_DIR = PARECO_ROOT / "Examples" / "Repos_results"

# Optional interval plans saved by the PaReco notebook (its interval_plan
# DataFrame) next to the pickles. When present they map every pickle to its
# pair and dates exactly; see data_division.
INTERVAL_PLAN_GLOB = "interval_plan*.csv"

# Folders put on sys.path so the pickles (which reference PaReco's Methods
# classes) can be loaded and PaReco can be asked for dates in Figures 7/8.
PARECO_SEARCH_PATHS = [
    PARECO_ROOT,
    PROJECT_ROOT,
    PROJECT_ROOT.parent,
]

# Optional file with one GitHub token per line (used by Figures 7/8).
TOKEN_FILE_NAME = "tokens.txt"

# ---------------------------------------------------------------------------
# Classification labels
# ---------------------------------------------------------------------------

# Final labels that may appear in any exported audit file, in the order they
# are presented in tables and figures.
CLASS_ORDER = ["MO", "ED", "NA", "CC", "NE", "EE"]

# Classes that mean "this patch is relevant for the target variant".
INTERESTING_CLASSES = {"MO", "ED"}

# File-level classes for files that were never split into hunks. Each such
# file counts as one patch observation in Figure 6.
SPECIAL_FILE_CLASSES = {"CC", "NE", "EE"}

# ---------------------------------------------------------------------------
# Date limit for the technical-lag analysis
# ---------------------------------------------------------------------------

# No target-variant commit after this moment is ever used.
HARD_VARIANT_END = datetime(2026, 9, 3, 23, 59, 59, tzinfo=timezone.utc)
HARD_VARIANT_END_TIMESTAMP = "2026-09-03T23:59:59Z"

# ---------------------------------------------------------------------------
# Studied variant pairs (upstream / variant_1, fork / variant_2)
# ---------------------------------------------------------------------------
#
# The ORDER of this list matters. PaReco runs every pair in this order, once
# upstream -> fork and once fork -> upstream (the two passes may share one
# numbering or each have their own, as GACPD's dsep3_/dsep3R_ runs do). The
# data-division step uses the order to tell apart pairs that share a source
# repository (spesmilo/electrum, qmk/qmk_firmware); that is intentional.

VARIANT_PAIRS = [
    ("dlindahl/omniauth-cas", "tduehr/omniauth-cas3"),
    ("ekohl/metadata_json_deps", "puppetlabs/dependency_checker"),
    ("lemurheavy/coveralls-ruby", "tagliala/coveralls-ruby-reborn"),
    ("openssh/openssh-portable", "PowerShell/openssh-portable"),
    ("openssl/openssl", "open-quantum-safe/openssl"),
    ("cweiske/jsonmapper", "apimatic/jsonmapper"),
    ("paramiko/paramiko", "ploxiln/paramiko-ng"),
    ("jagrosh/MusicBot", "Cosgy-Dev/JMusicBot-JP"),
    ("bitcoinj/bitcoinj", "langerhans/dogecoinj-new"),
    ("micke/valid_email2", "wolfemm/email_assessor"),
    ("NickWaterton/Roomba980-Python", "pschmitt/roombapy"),
    ("jekyll/jekyll-admin", "ashmaroli/jekyll-manager"),
    ("ledermann/rails-settings", "huacnlee/rails-settings-cached"),
    ("getredash/redash", "mozilla/redash"),
    ("DMTF/python-redfish-library", "HewlettPackard/python-ilorest-library"),
    ("coderholic/django-cities", "yourlabs/django-cities-light"),
    ("spesmilo/electrum", "Electron-Cash/Electron-Cash"),
    ("apache/roller", "gmazza/tightblog"),
    ("the-control-group/voyager", "cotiga/voyager"),
    ("google/styleguide", "cpplint/cpplint"),
    ("datastax/python-driver", "scylladb/python-driver"),
    ("javacc/javacc", "phax/ParserGeneratorCC"),
    ("NCAR/ParallelIO", "E3SM-Project/scorpio"),
    ("python-pillow/Pillow", "uploadcare/pillow-simd"),
    ("curl/curl", "Unity-Technologies/curl"),
    ("prestodb/presto", "twitter-forks/presto"),
    ("xerial/sqlite-jdbc", "Willena/sqlite-jdbc-crypt"),
    ("apache/mynewt-nimble", "espressif/esp-nimble"),
    ("easybuilders/easybuild-easyconfigs", "ComputeCanada/easybuild-easyconfigs"),
    ("spesmilo/electrum", "exoeconomy/EXOS-Electrum"),
    ("spesmilo/electrum", "Groestlcoin/electrum-grs"),
    ("PyMySQL/PyMySQL", "nakagami/CyMySQL"),
    ("DMOJ/online-judge", "mcpt/wlmoj"),
    ("DSpace/DSpace", "ufal/clarin-dspace"),
    ("DataDog/dd-trace-rb", "lightstep/ls-trace-rb"),  # fork has since been removed
    ("edx/configuration", "appsembler/configuration"),
    ("collectd/collectd", "Stackdriver/collectd"),
    ("qmk/qmk_firmware", "zsa/qmk_firmware"),
    ("microsoft/azure-tools-for-java", "JetBrains/azure-tools-for-intellij"),
    ("qmk/qmk_firmware", "germ/qmk_firmware"),
    ("spack/spack", "BlueBrain/spack"),
    ("apache/kafka", "linkedin/kafka"),
]

# ---------------------------------------------------------------------------
# Renamed / transferred repositories
# ---------------------------------------------------------------------------
#
# PaReco (like GACPD) runs on the canonical full_name GitHub reports, so result
# pickles of these repositories carry the new name. VARIANT_PAIRS keeps the
# names used in the paper; this map lets both names identify the same repo.
# Add an entry here if data_division reports a folder it cannot match.

RENAMED_REPOS = {
    "ekohl/metadata_json_deps": "voxpupuli/metadata_json_deps",
    "the-control-group/voyager": "thedevdojo/voyager",
    "datastax/python-driver": "apache/cassandra-python-driver",
}


def canonical_repo(repo):
    """Current GitHub name of a repository listed in VARIANT_PAIRS."""
    return RENAMED_REPOS.get(repo, repo)
