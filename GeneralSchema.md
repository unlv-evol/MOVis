## Output Directory Structure and Report Format

![](https://i.imgur.com/5Pc2Pfo.png)

The Figure above illustrates the directory organization and report structure produced for each analyzed pull request. The output is organized under a root folder, where each pull request is stored in a directory named using the pull request number and its classification:

```text
<PR_Number>_{Classification}
```

Within each pull request directory, the output may contain one or more classification-specific subdirectories. These subdirectories correspond to the detected categories associated with the pull request, such as:

```text
{Classification_1}
{Classification_X}
MO
```

The `MO` directory contains the files and reports associated with missed opportunities. Inside this directory, the generated output preserves the relevant project file structure and includes a `reports` directory. The `reports` directory contains the main analysis report:

```text
report.json
```

An example output structure is shown below:
```text
Root Folder/
└── <PR_Number>_{Classification}/
    ├── {Classification_1}/
    ├── {Classification_X}/
    └── MO/
        └── <Files>/
            ├── src/
            ├── cmp/
            └── reports/
                └── report.json
```

The `report.json` file stores the metadata required to describe detected duplicated fragments and their corresponding source and target locations. The report contains a `statistics` object, which includes a `duplicates` array. Each entry in this array represents one detected duplicated code fragment.

A simplified version of the report format is shown below:

```json
{
  "statistics": {
    "duplicates": [
      {
        "format": "<language>",
        "lines": <number>,
        "fragment": "<code lines>",
        "firstFile": {
          "name": "src/<srcHunkFile>",
          "start": <duplicateStartLine>,
          "end": <duplicateEndLine>,
          "startLoc": {
            "line": <duplicateStartLine>,
            "column": <duplicateStartColumn>
          },
          "endLoc": {
            "line": <duplicateEndLine>,
            "column": <duplicateEndColumn>
          }
        },
        "secondFile": {
          "name": "src/<targetFile>",
          "start": <duplicateStartLine>,
          "end": <duplicateEndLine>,
          "startLoc": {
            "line": <duplicateStartLine>,
            "column": <duplicateStartColumn>
          },
          "endLoc": {
            "line": <duplicateEndLine>,
            "column": <duplicateEndColumn>
          }
        }
      }
    ]
  }
}
```

Each duplicate entry records the programming language or file format of the duplicated fragment, the number of duplicated lines, and the duplicated code fragment itself. The `firstFile` field describes the source location of the duplicate, including the file name, start line, end line, and exact start and end positions using line and column numbers.

The `secondFile` field identifies the corresponding target file where the duplicated or related fragment appears. It follows the same location structure as firstFile, allowing the report to represent both sides of the source-target mapping.

This structure enables `MOVis` to reconstruct the relationship between source and target code regions. The directory layout provides access to the relevant source, comparison, and report files, while `report.json` provides the metadata needed to identify, align, and visualize duplicated fragments across the source and target locations.
