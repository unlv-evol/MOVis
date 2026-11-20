"""Read-only GitHub transport: validate status, rotate tokens, fail explicitly."""
import hashlib
import threading
import time
from urllib.parse import parse_qsl, urlencode, urlsplit, urlunsplit
import requests

_lock = threading.Lock()
_rejected = set()
_configured = []

class GitHubAPIError(RuntimeError):
    def __init__(self, status, url):
        self.status = status
        parts = urlsplit(url)
        query = [(k, v) for k, v in parse_qsl(parts.query) if k.lower() not in ('access_token', 'token')]
        safe_url = urlunsplit((parts.scheme, parts.netloc, parts.path, urlencode(query), ''))
        super().__init__(f'GitHub request failed (HTTP {status}): {safe_url}')

def _clean(token):
    token = token.strip()
    for prefix in ('bearer ', 'token '):
        if token.lower().startswith(prefix):
            return token[len(prefix):].strip()
    return token

def configure_tokens(tokens):
    global _configured
    with _lock:
        _configured = list(dict.fromkeys(_clean(t) for t in tokens if _clean(t)))

def token_pool(preferred):
    preferred = _clean(preferred)
    with _lock:
        return list(dict.fromkeys([preferred] + _configured))

def request_json(url, tokens, ct=0, allow_not_found=False, max_retries=3, timeout=30):
    if not tokens:
        raise ValueError('token_list cannot be empty')
    last_status = 401
    for _ in range(len(tokens)):
        index = ct % len(tokens)
        ct = (index + 1) % len(tokens)
        token = _clean(tokens[index])
        fingerprint = hashlib.sha256(token.encode()).digest()
        with _lock:
            rejected = fingerprint in _rejected
        if not token or rejected:
            continue
        for attempt in range(max(1, max_retries)):
            try:
                response = requests.get(url, headers={
                    'Authorization': f'Bearer {token}', 'Accept': 'application/vnd.github+json',
                    'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'PaReco',
                    'Cache-Control': 'no-cache'}, timeout=timeout)
            except requests.RequestException:
                if attempt + 1 == max(1, max_retries):
                    raise GitHubAPIError(0, url) from None
                time.sleep(0.2 * (2 ** attempt))
                continue
            last_status = response.status_code
            if last_status == 401:
                with _lock:
                    _rejected.add(fingerprint)
                break
            if last_status in (403, 429):
                break
            if last_status == 404 and allow_not_found:
                return None, ct
            if last_status == 200:
                try:
                    data = response.json()
                    if isinstance(data, (dict, list)):
                        return data, ct
                except ValueError:
                    pass
            elif last_status < 500:
                raise GitHubAPIError(last_status, url)
            if attempt + 1 == max(1, max_retries):
                raise GitHubAPIError(last_status, url)
            time.sleep(0.2 * (2 ** attempt))
    raise GitHubAPIError(last_status, url)
