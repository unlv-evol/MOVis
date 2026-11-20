"""
Minimal GitHub REST client with token rotation, plus a JSON cache.

The cache matters for reproducibility: once a PR merge date or a target HEAD
at a cutoff has been fetched it is stored, so re-running the analysis later
does not move the historical boundary.
"""

import json
from pathlib import Path

from . import config

API_ROOT = "https://api.github.com"


def load_tokens(extra_dirs=()):
    """Read GitHub tokens (one per line) from the first tokens.txt found."""
    candidates = [config.PROJECT_ROOT, Path.cwd(), Path.cwd().parent]
    candidates.extend(Path(d) for d in extra_dirs)

    for folder in candidates:
        token_file = folder / config.TOKEN_FILE_NAME
        if token_file.exists():
            tokens = [line.strip() for line in token_file.read_text(encoding="utf-8").splitlines()]
            tokens = [token for token in tokens if token]
            if tokens:
                return tokens
    return []


class GitHubClient:
    """Tiny wrapper around requests.get that rotates tokens on rate limits."""

    def __init__(self, tokens=None):
        self.tokens = list(tokens or [])
        self._current = 0

    def get_json(self, endpoint, params=None, allow_404=False):
        try:
            import requests
        except ImportError:
            raise RuntimeError("The requests package is needed to talk to GitHub.")

        attempts = max(2, len(self.tokens) * 2)
        last_error = None
        for _ in range(attempts):
            headers = {"Accept": "application/vnd.github+json"}
            if self.tokens:
                headers["Authorization"] = "Bearer {}".format(self.tokens[self._current % len(self.tokens)])

            response = requests.get(API_ROOT + endpoint, headers=headers, params=params, timeout=30)
            if response.status_code == 200:
                return response.json()
            if response.status_code == 404 and allow_404:
                return None
            if response.status_code in (403, 429) and self.tokens:
                # Rate limited: try the next token.
                self._current = (self._current + 1) % len(self.tokens)
                last_error = "{} {}".format(response.status_code, response.text[:300])
                continue
            raise RuntimeError("GitHub API {} failed with {}: {}".format(
                endpoint, response.status_code, response.text[:500]))

        raise RuntimeError("GitHub API rate-limit/authentication failure: {}".format(last_error))


class ApiCache:
    """JSON file holding fetched PR merge dates and target HEAD commits."""

    def __init__(self, path):
        self.path = Path(path)
        self.data = {"pull_requests": {}, "target_heads": {}}
        if self.path.exists():
            with self.path.open("r", encoding="utf-8") as handle:
                self.data.update(json.load(handle))
        self.data.setdefault("pull_requests", {})
        self.data.setdefault("target_heads", {})

    @property
    def pull_requests(self):
        return self.data["pull_requests"]

    @property
    def target_heads(self):
        return self.data["target_heads"]

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("w", encoding="utf-8") as handle:
            json.dump(self.data, handle, indent=2, sort_keys=True)
