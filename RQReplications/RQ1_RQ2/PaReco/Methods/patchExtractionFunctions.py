from Methods.github_api import configure_tokens, request_json
from Methods.bugfix_keywords import BUGFIX_KEYWORDS
# Replaced the library urllib.request with requests and 
# changed the code to suit the new library
import requests
import json

from dateutil import parser


class GetOutOfLoop(Exception):
    pass


import json
import time
import requests


def get_response(url, token_list, ct, max_retries=5, timeout=30):
    configure_tokens(token_list)
    return request_json(url, token_list, ct, max_retries=max_retries, timeout=timeout)


def repo_commit_date(repo, date, token_list, ct):
    url2 = 'https://api.github.com/repos/' + repo + '/commits?until=' + date
    content_arrays, ct = get_response(url2, token_list, ct)
    sha = ''
    sha = content_arrays[0]['sha']
    commit_date = content_arrays[0]['commit']['committer']['date']
    return sha, ct


def repo_dates(repo, token_list, ct):
    created_at = ''
    updated_at = ''
    url = 'https://api.github.com/repos/' + repo
    # print(url)
    content_arrays, ct = get_response(url, token_list, ct)
    if content_arrays is None:
        raise RuntimeError(
            f'GitHub API request returned no data for repository: {repo}\n'
            f'URL: {url}\n'
            f'Token index: {ct}'
        )

    if not isinstance(content_arrays, dict):
        raise TypeError(
            f'Unexpected response type for {repo}: '
            f'{type(content_arrays).__name__}: {content_arrays!r}'
        )

    if 'created_at' not in content_arrays or 'updated_at' not in content_arrays:
        raise RuntimeError(
            f'GitHub response for {repo} does not contain repository dates.\n'
            f'Response: {content_arrays!r}'
        )
    
    if content_arrays is not None:
        created_at = content_arrays['created_at']
        updated_at = content_arrays['updated_at']
    return created_at, updated_at, ct

def repo_info(repo, token_list, ct):
    url = 'https://api.github.com/repos/' + repo
    content, ct = get_response(url, token_list, ct)

    if content is None:
        raise RuntimeError(
            f'GitHub API request returned no data for repository: {repo}'
        )

    return {
        'full_name': content.get('full_name', repo),
        'created_at': content['created_at'],
        'updated_at': content['updated_at'],
        'pushed_at': content['pushed_at'],
        'default_branch': content['default_branch'],
    }, ct

def repo_commit_date_new(repo, date, token_list, ct, branch=None):
    url = f'https://api.github.com/repos/{repo}/commits?until={date}&per_page=1'

    if branch:
        url += f'&sha={branch}'

    content_arrays, ct = get_response(url, token_list, ct)

    if not content_arrays:
        raise RuntimeError(
            f'No commit found for {repo} at or before {date}'
        )

    sha = content_arrays[0]['sha']
    commit_date = content_arrays[0]['commit']['committer']['date']

    return sha, commit_date, ct

def divergence_date(
    mainline,
    variant,
    token_list,
    ct,
    least_date='',
    diverge_date=''
):
    ml_info, ct = repo_info(mainline, token_list, ct)
    vr_info, ct = repo_info(variant, token_list, ct)

    resolved_mainline = ml_info['full_name']
    resolved_variant = vr_info['full_name']

    ml_branch = ml_info['default_branch']
    vr_branch = vr_info['default_branch']

    # If no cutoff was supplied, preserve your previous behavior
    # of choosing a date based on repository metadata.
    if least_date == '':
        date_ml = parser.parse(ml_info['updated_at'])
        date_vr = parser.parse(vr_info['updated_at'])

        if date_ml < date_vr:
            least_date = ml_info['updated_at']
        else:
            least_date = vr_info['updated_at']

    # Get each branch as it existed at/before the cutoff.
    sha_ml, date_ml_commit, ct = repo_commit_date_new(
        resolved_mainline,
        least_date,
        token_list,
        ct,
        branch=ml_branch
    )

    sha_vr, date_vr_commit, ct = repo_commit_date_new(
        resolved_variant,
        least_date,
        token_list,
        ct,
        branch=vr_branch
    )

    vr_owner = resolved_variant.split('/')[0]

    url = (
        f'https://api.github.com/repos/{resolved_mainline}'
        f'/compare/{sha_ml}...{vr_owner}:{sha_vr}'
    )

    content, ct = get_response(url, token_list, ct)

    if content is None:
        raise RuntimeError(
            f'GitHub comparison returned no data:\n{url}'
        )

    merge_base = content['merge_base_commit']

    divergence_sha = merge_base['sha']

    if diverge_date == '':
        diverge_date = merge_base['commit']['committer']['date']

    ahead = content['ahead_by']
    behind = content['behind_by']

    return '', diverge_date, least_date, ahead, behind, ct, resolved_mainline, resolved_variant


def detached_pair_dates(mainline, variant, token_list, ct):
    """Mirror of GACPD getDetachedPairDates.

    Pairs outside one fork network have no merge base, so the dates are
    estimated from repository metadata: the divergence date is the later
    created_at and the cutoff is the earlier updated_at.
    """
    ml_info, ct = repo_info(mainline, token_list, ct)
    vr_info, ct = repo_info(variant, token_list, ct)

    ml_created = parser.parse(ml_info['created_at'])
    vr_created = parser.parse(vr_info['created_at'])
    ml_updated = parser.parse(ml_info['updated_at'])
    vr_updated = parser.parse(vr_info['updated_at'])

    diverge_date = ml_info['created_at'] if vr_created < ml_created else vr_info['created_at']
    least_date = ml_info['updated_at'] if ml_updated < vr_updated else vr_info['updated_at']

    return '', diverge_date, least_date, 0, 0, ct, ml_info['full_name'], vr_info['full_name']



def pr_patches(repo, diverge_date, least_date, token_list, ct):
    pr = []
    pr_all_merged = []
    title = []



    divergence_dt = parser.parse(diverge_date)
    least_dt = parser.parse(least_date)

    p = 1
    seen = set()
    reached_cutoff = False

    # Same window rule as MOVis getPrPatches: eligibility is decided by
    # merged_at, so a PR created long before the window but merged inside it
    # still belongs to it.
    #
    # Pages run oldest-created first. A PR merged inside the window must have
    # been created no later than the cutoff, so the walk stops at the first PR
    # created after it instead of paging through every closed PR in the
    # repository. The PRs returned are identical; only the number of pages
    # read changes. A PR merged while this runs has merged_at after the cutoff
    # and cannot change the result.
    while not reached_cutoff:
        url = (
            f'https://api.github.com/repos/{repo}/pulls'
            f'?page={p}'
            f'&per_page=100'
            f'&state=closed'
            f'&sort=created'
            f'&direction=asc'
        )

        p += 1

        pulls_arrays, ct = get_response(
            url,
            token_list,
            ct
        )

        print(url)

        if not pulls_arrays:
            break

        for pull_obj in pulls_arrays:

            if parser.parse(pull_obj['created_at']) > least_dt:
                reached_cutoff = True
                break

            merged_at = pull_obj.get('merged_at')

            # We only care about merged PRs.
            if merged_at is None:
                continue

            merged_dt = parser.parse(merged_at)

            # Same exact condition used by GACPD.
            if merged_dt < divergence_dt:
                continue

            if merged_dt > least_dt:
                continue

            pr_number = pull_obj['number']
            if pr_number in seen:
                continue
            seen.add(pr_number)

            pr_all_merged.append(pr_number)

            pull_title = (
                pull_obj['title']
                .lower()
                .replace(';', '-')
                .replace(',', '-')
            )

            for bug in BUGFIX_KEYWORDS:
                if bug in pull_title:
                    pr.append(pr_number)
                    title.append(pull_title)
                    break

    return pr, title, pr_all_merged, ct