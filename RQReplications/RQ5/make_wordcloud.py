"""Render one word cloud per interview question from the data in output/.

Reads output/word_frequencies_by_question.csv and writes, under output/wordclouds/:
  Q01.png/.pdf ... Q11.png/.pdf   one cloud per question (no title, ready for the paper)
  all_questions.png/.pdf          the same clouds as a titled overview grid
  overall.png/.pdf                all answers combined (from output/word_frequencies.csv)

Usage: python make_wordcloud.py [--exclude-tools] [--top N]
  --exclude-tools  drop the tool names (MOVis, PaReco, GACPD); files get a _no_tools suffix
  --top N          keep only the N most frequent words per cloud (default: all)
"""
import argparse
import csv
import math
import textwrap
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from wordcloud import WordCloud

import re
from pathlib import Path

RQ5_DIR = Path(__file__).resolve().parent
OUT_DIR = RQ5_DIR / "output"

TOOL_NAMES = {"MOVis", "PaReco", "GACPD"}
CLOUD_DIR = OUT_DIR / "wordclouds"


def filter_freqs(freqs, args):
    if args.exclude_tools:
        freqs = {w: c for w, c in freqs.items() if w not in TOOL_NAMES}
    if args.top:
        freqs = dict(sorted(freqs.items(), key=lambda kv: -kv[1])[: args.top])
    return freqs


def make_cloud(freqs):
    wc = WordCloud(width=1600, height=900, background_color="white", colormap="viridis",
                   prefer_horizontal=0.9, random_state=42, collocations=False)
    return wc.generate_from_frequencies(freqs)


def save_single(cloud, stem):
    fig = plt.figure(figsize=(16, 9))
    plt.imshow(cloud, interpolation="bilinear")
    plt.axis("off")
    plt.tight_layout(pad=0)
    for ext in ("png", "pdf"):
        fig.savefig(CLOUD_DIR / f"{stem}.{ext}", dpi=200, bbox_inches="tight")
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exclude-tools", action="store_true")
    ap.add_argument("--top", type=int, default=0)
    args = ap.parse_args()
    suffix = "_no_tools" if args.exclude_tools else ""
    CLOUD_DIR.mkdir(parents=True, exist_ok=True)

    by_q, titles = defaultdict(dict), {}
    with open(OUT_DIR / "word_frequencies_by_question.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            q = int(r["question_no"])
            by_q[q][r["word"]] = int(r["count"])
            titles[q] = r["question"]

    clouds = {}
    for q in sorted(by_q):
        freqs = filter_freqs(by_q[q], args)
        if not freqs:
            print(f"Q{q}: no words left, skipped")
            continue
        clouds[q] = make_cloud(freqs)
        save_single(clouds[q], f"Q{q:02d}{suffix}")
        print(f"Q{q}: {len(freqs)} words")

    # Overview grid with question titles.
    cols = 3
    rows = math.ceil(len(clouds) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(cols * 8, rows * 5.6))
    for ax in axes.flat:
        ax.axis("off")
    for ax, (q, cloud) in zip(axes.flat, clouds.items()):
        ax.imshow(cloud, interpolation="bilinear")
        ax.set_title(f"Q{q}. " + "\n".join(textwrap.wrap(titles[q], 70)), fontsize=11, loc="left")
    fig.tight_layout()
    for ext in ("png", "pdf"):
        fig.savefig(CLOUD_DIR / f"all_questions{suffix}.{ext}", dpi=150, bbox_inches="tight")
    plt.close(fig)

    with open(OUT_DIR / "word_frequencies.csv", encoding="utf-8") as f:
        overall = filter_freqs({r["word"]: int(r["count"]) for r in csv.DictReader(f)}, args)
    save_single(make_cloud(overall), f"overall{suffix}")
    print(f"Wrote {len(clouds)} question clouds + overview + overall to {CLOUD_DIR}")


if __name__ == "__main__":
    main()
