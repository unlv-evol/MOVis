# RQ5: User study

```
RQ5/
├── Participant <N> ... - Responses + Timings.pdf    user responses (12 participants)
├── make_wordcloud.py                                word-cloud generator
├── output/
│   ├── responses.csv                                all answers, one row per answer
│   ├── word_frequencies.csv                         word counts over all answers
│   ├── word_frequencies_by_question.csv             word counts per question
│   └── wordclouds/                                  generated figures (PNG + PDF)
└── User Study/                                      MOVis version and PRs used in the study
```

## User responses

The answers and task timings of every participant are in the **root of this
folder**, one PDF per participant (`Participant 1 - Responses + Timings.pdf` to
`Participant 12 (6_2) - Responses + Timings.pdf`). The transcribed answers are in
`output/responses.csv`.

## User Study folder

The **`User Study`** folder contains:

- the **MOVis version** used to run the user study, and
- the **pull requests (PRs) used for each tool**: the PRs participants analysed
  with MOVis, PaReco and GACPD.

## Example run (video)

A video of an example run of the user study:

**Video:** <!-- TODO: add video link here --> _link coming soon_

## Generating the word clouds

`make_wordcloud.py` reads the word frequencies in `output/`
(`word_frequencies_by_question.csv` and `word_frequencies.csv`) and creates the
word clouds in `output/wordclouds/`.

### Requirements

Python 3.8+ with `matplotlib` and `wordcloud`:

```bash
pip install matplotlib wordcloud
```

(Tested with matplotlib 3.11.2 and wordcloud 1.9.6.)

### Run

From the `RQ5` folder (it can be run from any folder, because paths are relative
to the script):

```bash
python make_wordcloud.py                     # all words
python make_wordcloud.py --exclude-tools     # without MOVis / PaReco / GACPD
python make_wordcloud.py --top 50            # only the 50 most frequent words per cloud
```

| Option | Effect |
|---|---|
| `--exclude-tools` | Removes the tool names (MOVis, PaReco, GACPD). Output files get a `_no_tools` suffix. |
| `--top N` | Keeps only the N most frequent words per cloud (default: all). |

### Output (`output/wordclouds/`)

| File | Content |
|---|---|
| `Q01` … `Q11` (`.png` / `.pdf`) | One cloud per interview question, without a title (as used in the paper) |
| `all_questions` (`.png` / `.pdf`) | All question clouds in one grid, with titles |
| `overall` (`.png` / `.pdf`) | All answers combined |

Running with `--exclude-tools` writes the same set of files with a `_no_tools`
suffix (for example `Q01_no_tools.png`), so it does not overwrite the full clouds.
