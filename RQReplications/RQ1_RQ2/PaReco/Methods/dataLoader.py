from Methods.github_api import configure_tokens, request_json
from urllib.parse import quote
import os
import requests
import csv
import json
import sys
import re
import time
from collections import defaultdict
import bitarray
import pickle
import difflib
from pprint import pprint

import Methods.common as common
import Methods.commitLoader as commitloader

try:
    import argparse
    import magic
except ImportError as err:
    print(err)
    sys.exit(-1)


GITHUB_FILES_PER_PAGE = 100
GITHUB_MAX_PR_FILES = 3000

class PrFileCountMismatch(RuntimeError):
    pass


def _api_request(url, token_list, ct):
    configure_tokens(token_list)
    return request_json(url, token_list, ct)


def _fetch_pr_files(source, pr_number, expected_count, token_list, ct):
    """Fetch the PR's *net* changed-file list, following GitHub pagination.

    GitHub's pull-request files endpoint represents the same final PR diff shown
    in the Files changed tab.  This is intentionally different from unioning the
    files reported by every commit in the PR: intermediate/merge commits can
    touch many files that are not part of the final PR diff.
    """
    if expected_count > GITHUB_MAX_PR_FILES:
        raise RuntimeError(
            "GitHub's pull-request files API exposes at most 3000 files, but "
            f"PR {pr_number} reports {expected_count} changed files."
        )

    files = []
    page = 1

    while True:
        files_url = (
            f"https://api.github.com/repos/{source}/pulls/{pr_number}/files"
            f"?per_page={GITHUB_FILES_PER_PAGE}&page={page}"
        )
        page_data, ct = _api_request(files_url, token_list, ct)

        if not isinstance(page_data, list):
            raise RuntimeError(
                f"Unexpected response while fetching PR {pr_number} files: "
                f"{page_data!r}"
            )

        files.extend(page_data)

        if len(page_data) < GITHUB_FILES_PER_PAGE:
            break

        page += 1
        if page > (GITHUB_MAX_PR_FILES // GITHUB_FILES_PER_PAGE):
            break

    if len(files) != expected_count:
        raise PrFileCountMismatch(
            f"Incomplete PR file fetch for {source}#{pr_number}: GitHub "
            f"changed_files={expected_count}, fetched_files={len(files)}"
        )

    return files, ct


def _count_patch_hunks(patch_text):
    """Count unified-diff hunks exactly as represented by GitHub patch text."""
    if not patch_text:
        return 0
    return sum(1 for line in patch_text.splitlines() if line.startswith('@@'))


def fetchPrData(source, destination, prs, destination_sha, token_list, ct):
    print('Fetching PR information and net changed files from patches...')
    start = time.time()
    req = 0
    pr_data = {}

    for k in prs:
        try:
            # Fetch PR metadata.  changed_files and the base/head SHAs describe
            # the final PR diff, which is what GitHub/MOVis report to users.
            pr_request = f'https://api.github.com/repos/{source}/pulls/{k}'
            for consistency_attempt in range(3):
                pr, ct = _api_request(pr_request, token_list, ct)
                req += 1
                count = pr.get('changed_files') if isinstance(pr, dict) else None
                valid_count = type(count) is int and count >= 0
                if valid_count:
                    nr_files = count
                    base_sha = pr.get('base', {}).get('sha', '')
                    head_sha = pr.get('head', {}).get('sha', '')
                    if not base_sha or not head_sha:
                        raise RuntimeError(f'Missing base/head metadata for {source}#{k}')
                    commits_url = pr.get('commits_url', '')
                    try:
                        pr_files, ct = _fetch_pr_files(source, k, nr_files, token_list, ct)
                        req += len(pr_files) // GITHUB_FILES_PER_PAGE + 1
                        break
                    except PrFileCountMismatch:
                        if consistency_attempt == 2:
                            raise
                elif consistency_attempt == 2:
                    raise RuntimeError(f'Missing/invalid changed_files for {source}#{k}')
                time.sleep(0.2 * (consistency_attempt + 1))

            commits_data = {}
            files_data = []

            for file_info in pr_files:
                file_name = file_info.get('filename', '')
                if not file_name:
                    raise RuntimeError(
                        f"PR {source}#{k} returned a file entry without a filename"
                    )

                patch_text = file_info.get('patch')
                changes = int(file_info.get('changes', 0) or 0)
                patch_available = (
                    changes > 0 and isinstance(patch_text, str) and bool(patch_text)
                )
                if patch_available:
                    patch_status = 'available'
                elif changes == 0:
                    patch_status = 'not_provided_no_textual_changes'
                else:
                    patch_status = 'missing_from_github'

                file_summary = {
                    'filename': file_name,
                    'status': file_info.get('status', ''),
                    'additions': file_info.get('additions', 0),
                    'deletions': file_info.get('deletions', 0),
                    'changes': changes,
                    'previous_filename': file_info.get('previous_filename', ''),
                    'patch_available': patch_available,
                    'patch_status': patch_status,
                    'hunk_count': _count_patch_hunks(patch_text),
                }
                if patch_available:
                    file_summary['patch'] = patch_text
                files_data.append(file_summary)

                # Always create the key so files absent from the destination fork
                # are still represented and can be classified as NOT EXISTING.
                commits_data[file_name] = []

                file_url = (f'https://api.github.com/repos/{destination}/contents/'
                            f'{quote(file_name, safe="/")}?ref={quote(destination_sha, safe="")}')
                file_response, ct = request_json(file_url, token_list, ct, allow_not_found=True)
                req += 1
                exists_in_destination = file_response is not None
                if exists_in_destination and (not isinstance(file_response, dict) or 'path' not in file_response):
                    raise RuntimeError(f'Invalid file-existence response for {source}#{k}')

                if not exists_in_destination:
                    continue

                sub = dict(file_summary)
                sub.pop('filename', None)
                # For a PR-level net diff, the correct before/after states
                # are the PR base and head, not arbitrary first/last commits.
                sub.update({
                    'commit_url': pr_request,
                    'commit_sha': head_sha,
                    'commit_date': (
                        pr.get('merged_at')
                        or pr.get('updated_at')
                        or pr.get('created_at')
                        or ''
                    ),
                    'parent_sha': base_sha,
                })

                commits_data[file_name].append(sub)

            if len(commits_data) != nr_files:
                raise RuntimeError(
                    f"PR file-map size mismatch for {source}#{k}: "
                    f"changed_files={nr_files}, mapped_files={len(commits_data)}"
                )

            pr_data[k] = {}
            pr_data[k]['pr_url'] = pr_request
            pr_data[k]['commits_url'] = commits_url
            pr_data[k]['changed_files'] = nr_files
            pr_data[k]['fetched_files'] = len(commits_data)
            pr_data[k]['files_data'] = files_data
            pr_data[k]['commits_data'] = [commits_data]
            pr_data[k]['destination_sha'] = destination_sha
            pr_data[k]['base_sha'] = base_sha
            pr_data[k]['head_sha'] = head_sha

        except Exception as e:
            raise RuntimeError(f'PR fetch failed for {source}#{k}; run is incomplete') from e

    end = time.time()
    runtime = end - start
    print('Fetch Runtime: ', runtime)

    return ct, pr_data, req, runtime


def getDestinationSha(destination, cut_off_date, token_list, ct):
    destination_sha = ''
    lenTokens = len(token_list)
    if ct >= lenTokens:
        ct = 0
    cut_off_commits = commitloader.apiRequest(
        'https://api.github.com/repos/' + destination + '/commits?until=' + cut_off_date,
        token_list[ct]
    )
    ct += 1
    destination_sha = cut_off_commits[0]['sha']
    return destination_sha, ct
