# MOVis: Replication Package

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](./LICENSE)
[![Conference](https://img.shields.io/badge/FSE-2026-blue.svg)](#)
[![Artifact Type](https://img.shields.io/badge/Artifact-Tool%20Demo-orange.svg)](#)

This repository contains the **replication package** for the FSE 2026 Tool Demonstration paper:

> **MOVis: A Visual Analytics Tool for Surfacing Missed Patches Across Software Variants**

**MOVis** is a visual analytics system that helps developers explore and reason about *missed patches* across related software variants by combining program analysis, mining-based techniques, and interactive visualization.

---

## Table of Contents

- [MOVis: Replication Package](#movis-replication-package)
  - [Table of Contents](#table-of-contents)
  - [Artifact Overview](#artifact-overview)
  - [Repository Structure](#repository-structure)
  - [System Requirements](#system-requirements)
    - [Hardware](#hardware)
    - [Operating Systems](#operating-systems)
    - [Software Dependencies](#software-dependencies)
    - [GitHub Tokens](#github-tokens)
  - [Installation and Running MOvis](#installation-and-running-movis)
  - [Reproducing the Paper Demonstration](#reproducing-the-paper-demonstration)
  - [Expected Output](#expected-output)
  - [Development Environment Setup](#development-environment-setup)
    - [Windows](#windows)
    - [Linux / macOS](#linux--macos)
  - [Extending MOVis](#extending-movis)
  - [License](#license)
  - [Contact](#contact)

---

## Artifact Overview

This replication package supports:

* Reproducing the **tool demonstration** presented in the paper
* Re-running the **analysis pipeline** used to generate the visualization data
* Inspecting, modifying, or extending the MOVis implementation

The artifact is designed to be **self-contained**, **lightweight**, and runnable on a standard developer machine.

---

## Repository Structure

```
MOVis/
├── MOVis.pro               # Qt project configuration file
├── LICENSE                 # Project license
├── README.md               # Project overview and usage instructions
├── Pareco/                 # Analysis and data-processing backend
│   ├── Methods/            # Core analysis modules (patch extraction, loading, metrics)
│   ├── NewPaReco/          # Refactored and extended analysis components
│   └── ...                 # Additional backend utilities and modules
├── src/                    # Qt/C++ source files for the MOVis GUI
├── forms/                  # Qt Designer (.ui) files
├── assets/                 # Icons, images, and UI assets
├── headers/                # C/C++ header files
├── include/                # Shared include files and third-party headers
├── logs/                   # Runtime and analysis logs
├── scripts/                # Helper scripts (build, analysis, data preparation)
├── release/                # Compiled binaries or intermediate build artifacts
├── docker/                 # Dockerfiles and container configuration
└── .gitignore              # Git ignore rules
```

**Key components**:

* **Qt-based GUI (C++)**: Interactive visualization of software variants and missed patches
* **PaReco backend (Python)**: Patch extraction, commit loading, and analysis logic

---

## System Requirements

### Hardware
- **Processor:** 1.18 GHz CPU or faster  
- **RAM:** Minimum of 16 GB  
- **Disk Space:** At least 15 GB of free storage  

### Operating Systems

* Linux (x86_64)
* macOS (Intel or Apple Silicon)
* Windows 11

### Software Dependencies

* **Qt 6.10.1**
* **Python 3.12**
* `pip` (Python dependency management)
* Git
* NPM
* Docker
* Linux only: `mesa-common-dev`, `libgl1-mesa-dev`

### GitHub Tokens

MOVis relies on the GitHub API for extracting pull request patches through **PaReco's** backend. To avoid rate limiting, we recommend configuring **at least two GitHub access tokens**:

* Tokens can be created at: [https://github.com/settings/tokens](https://github.com/settings/tokens)
* Using multiple tokens enables safe execution of large-scale patch extraction without exceeding API limits

---

## Installation and Running MOvis

This section will get you through installation and execution of the RePatch tool. **If you want to quickly reproduce the results in the paper without running the tool, kindly refer to "[reproducing-the-paper-demonstration](#reproducing-the-paper-demonstratio)" section**.

You can run the **RePatch** tool using one of two approaches:

1. **Locally on your machine**, or
2. **Using Docker**.

The instructions below explain how to run the tool locally. **For most users, we recommend using the containerized setup provided in the [docker](docker/dev-container-movis/) directory, which includes a dedicated [README](docker/dev-container-repatch/README.md) with step-by-step guidance. This will automatically install and configure all neccessary tools**.

---

## Reproducing the Paper Demonstration

The tool demonstration described in the paper can be reproduced as follows:

1. **Install Base Packages**
   - If you are using Windows, install **Python 3.12** and **Node.js** with **npm**. Install python packages required for PaReco to work.
    ```
    cd Pareco/
    pip3 install pip --upgrade
    pip3 install -r requirements.txt
    ```
    Incase some of the packages are failing, try ```pip3 install -r requirements.txt --break-system-packages```
   - For Linux users, 

2. **Missed Patch Identification**
   Candidate missed patches across software variants are identified using `analysis.py` and related analysis modules.

3. **Visualization**
   The MOVis GUI renders the analysis results, allowing users to:

   * Explore relationships between software variants
   * Compare applied versus missed patches
   * Drill down into commit-, file-, and patch-level details

All steps can be reproduced using the provided scripts and the interactive UI workflows.

---

## Expected Output

When running the demo, users should observe:

* A list of analyzed software variants
* Visual indicators highlighting missed patches
* Interactive navigation across patches, files, and commits

---

## Development Environment Setup

### Windows

*(Instructions to be added)*

### Linux / macOS

*(Instructions to be added)*

> **Note**: Docker support is provided for users who prefer a containerized setup.

---

## Extending MOVis

MOVis is designed to be extensible. Researchers and practitioners can:

* Plug in new software variant datasets
* Extend patch similarity, classification, or ranking logic
* Reuse the PaReco backend independently of the GUI

The modular structure of `Pareco/Methods` enables straightforward experimentation and extension.

---

## License

This project is released under the **MIT License**.
See the [`LICENSE`](./LICENSE) file for details.

---

## Contact

For questions or issues related to this replication package, please contact the authors listed in the paper.
