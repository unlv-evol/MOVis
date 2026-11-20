# RQ1 & RQ2: Running PaReco

This folder has everything needed to run PaReco on the studied variant pairs and
to analyse its results.

```
RQ1_RQ2/
├── .python-version          Python version used (3.8.10)
├── .venv/                   virtual environment (you create this, see step 2)
├── PaReco/
│   ├── requirements.txt     dependencies
│   ├── Examples/            the notebooks that run PaReco (step 4)
│   │   ├── Examples_variant_lists_hard_limit.ipynb
│   │   ├── Examples.ipynb
│   │   ├── PaReco.py
│   │   ├── Repos_files/     files downloaded during a run
│   │   └── Repos_results/   result pickles written by a run
│   ├── Methods/             PaReco helper modules
│   ├── src/                 analysis notebook to RUN (step 5)
│   └── *.txt                per-run summaries (PRs, files, hunks)
└── Table_Image_Generation/  read-only copy of the analysis, for viewing only
```

---

## 1. Install Python 3.8.10

PaReco was run with **Python 3.8.10** (the version in `.python-version`). Other
versions may resolve different dependency versions, so use 3.8.10 if you can.

With [pyenv](https://github.com/pyenv/pyenv) or
[pyenv-win](https://github.com/pyenv-win/pyenv-win), `.python-version` selects it
automatically from inside this folder:

```bash
pyenv install 3.8.10
cd RQ1_RQ2
python --version        # should print Python 3.8.10
```

Without pyenv, install 3.8.10 from <https://www.python.org/downloads/release/python-3810/>
and call that interpreter directly in the next step.

## 2. Create and activate the virtual environment

Create the venv in the `RQ1_RQ2` folder, next to `PaReco/`:

```bash
cd RQ1_RQ2
python -m venv .venv
```

Activate it. Do this in every new terminal before running anything else:

| Shell | Command |
|---|---|
| Windows PowerShell | `.\.venv\Scripts\Activate.ps1` |
| Windows cmd | `.venv\Scripts\activate.bat` |
| Git Bash | `source .venv/Scripts/activate` |
| Linux / macOS | `source .venv/bin/activate` |

If PowerShell blocks the script, run
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once and try again.

## 3. Install the requirements

With the venv active:

```bash
python -m pip install --upgrade pip
pip install -r PaReco/requirements.txt
pip install notebook ipykernel
```

### The `magic` package: use exactly `python-magic-bin==0.4.14`

PaReco uses `magic` to detect file types during classification. `requirements.txt`
pins **`python-magic-bin==0.4.14`**, which includes the `libmagic` library, so
nothing else has to be installed on Windows.

- **Do not** also install `python-magic` or `python-magic-win64`. They install
  the same `magic` module, and the last one installed replaces the others.
  This usually fails with `failed to find libmagic` or
  `AttributeError: module 'magic' has no attribute 'Magic'`.
- If one of them is already installed, fix the venv with:

  ```bash
  pip uninstall -y python-magic python-magic-win64 python-magic-bin
  pip install python-magic-bin==0.4.14
  ```

- Check that it works:

  ```bash
  python -c "import magic; print(magic.Magic(mime=True).from_buffer(b'print(1)'))"
  ```

- **Linux:** `python-magic-bin` has no Linux wheel. Install the system library
  (`sudo apt-get install libmagic1`), then replace the `python-magic-bin==0.4.14`
  line in `requirements.txt` with `python-magic` before running `pip install -r`.
  **macOS:** if the bundled library fails to load, run `brew install libmagic` and do
  the same replacement.

### Tested versions

These versions are installed in the venv used for the paper. Pin them if a newer
release causes problems:

```
requests==2.32.4   numpy==1.24.4    pandas==2.0.3     matplotlib==3.7.5
bitarray==3.12.4   unidiff==0.7.5   pydriller==2.12   openai==2.2.0
python-magic-bin==0.4.14
notebook==7.3.3    jupyterlab==4.3.8  ipykernel==6.29.5
```

### Register the venv as a Jupyter kernel (optional, recommended)

This makes the kernel easy to pick in VS Code or JupyterLab:

```bash
python -m ipykernel install --user --name pareco-venv --display-name "PaReco (.venv, Python 3.8.10)"
```

## 4. Run PaReco

### 4.1 Start Jupyter from the venv

```bash
# venv active, inside RQ1_RQ2
jupyter notebook
```

In VS Code, open the notebook and select the `.venv` interpreter, or the
`PaReco (.venv, Python 3.8.10)` kernel, as the kernel.

Open **`PaReco/Examples/Examples_variant_lists_hard_limit.ipynb`**. It runs every
variant pair in six-month windows, capped at `2026-09-03T23:59:59Z`.
`Examples.ipynb` runs a single pair, which is useful for trying one pair or one
date window.

The first code cell runs `os.chdir('./..')`, so after it the working directory is
`PaReco/`. Paths below are relative to `PaReco/`. Run that cell **only once** per
kernel session; running it again moves you up another folder.

### 4.2 Add your GitHub tokens

PaReco makes many GitHub API requests and needs personal access tokens. Use
**at least 5** so that no single token hits the rate limit. Choose one of these
options in the **"Add GitHub tokens"** cell:

- **Option A: in the notebook.** Fill in `token_list`:

  ```python
  token_list = [
      'ghp_token1',
      'ghp_token2',
      # ...
  ]
  ```

- **Option B: from a file.** Create `PaReco/tokens.txt` (note: `PaReco/`, not
  `PaReco/Examples/`, because of the `chdir` above). Put every token on **one line,
  separated only by commas**, with no spaces:

  ```
  ghp_token1,ghp_token2,ghp_token3
  ```

  Then uncomment the "Read from list" block and comment out the manual
  `token_list` in that cell.

Remove your tokens from the notebook before you share or commit it.

### 4.3 Run the cells

1. **Magic cookie** cell: initialises `magic`. If it fails, see step 3.
2. **Create directories** cell: creates `Examples/Repos_files` and
   `Examples/Repos_results`. If they already exist, skip this cell; running it
   raises `FileExistsError`.
3. **Variant lists**: `variant1_list[i]` is paired with `variant2_list[i]`. Set
   `starting_data_number` to the first data ID you want to use.
4. Run the remaining cells in order: date retrieval and interval plan, then
   extraction and classification for every interval, then the run summary.

Each interval writes `Examples/Repos_results/<id>_<owner>_<repo>_results.pkl` and
a summary `<id>_<owner>_<repo>.txt` in `PaReco/`. These pickles are the input of
the analysis.

## 5. Run the analysis

After PaReco has finished, run the analysis notebook **with the same `.venv`**:

> **Run `PaReco/src/analysis.ipynb`.**
> `Table_Image_Generation/analysis.ipynb` is only a copy for viewing the tables and
> figures easily. It is **not** meant to be executed.

```bash
# venv active
cd PaReco/src
jupyter notebook analysis.ipynb
```

- Settings (paths, the list of variant pairs, the date limit) are in
  `PaReco/src/pareco_analysis/config.py`.
- It reads the pickles from `PaReco/Examples/Repos_results` and writes the audits,
  tables and figures to `PaReco/src/Results_/`.
- The technical-lag step (Figures 7 and 8) also calls GitHub. It reads
  `PaReco/src/tokens.txt`. This file uses **one token per line**, which is a
  different format from PaReco's comma-separated `tokens.txt`.
- The run order is the order of the notebook cells: export pickles, then combine
  audits, data division, Table 1 / Figures 4–5, Figure 6, and Figures 7–8.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `failed to find libmagic` / `module 'magic' has no attribute ...` | Reinstall only `python-magic-bin==0.4.14` (step 3). |
| `ModuleNotFoundError: Examples` / `Methods` | The `os.chdir('./..')` cell was skipped or run twice. Restart the kernel and run the cells from the top. |
| `FileExistsError` on `Repos_files` / `Repos_results` | The folders already exist. Skip that cell. |
| `403` / rate-limit errors from GitHub | Add more tokens. |
| Notebook runs on the wrong Python | Select the `.venv` kernel. `import sys; sys.executable` should point inside `RQ1_RQ2/.venv`. |
