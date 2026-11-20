# RQ4: Viewing the results in MOVis

This folder holds the MOVis result used for RQ4: pull request **#12842**
(*KAFKA-14406: Fix double iteration of restoring records*), classified as
**MO** (missed opportunity).

```
RQ4/
└── 12842_MO/
    ├── pr_results.json                          PR metadata and overall classification
    └── MO/
        ├── 21beecd89a41_RocksDBTimestampedStore_java/
        └── 4309e807cf4e_RocksDBStore_java/
            ├── cmp/       target-variant file the patch was compared against
            ├── patch/     the patch for this file
            ├── src/       extracted hunks (context / deletions / additions)
            ├── reports/   jscpd clone reports (JSON + HTML)
            └── results.json
```

## How to view it

The results are meant to be viewed in **MOVis**' results viewer, with this `RQ4`
folder as the root folder.

1. Build and run MOVis (see the README in the MOVis repository).
2. In the main window, click **View Previous Results**.
3. In the **Select Root Directory** dialog, choose this **`RQ4`** folder. Do not
   choose `12842_MO` or a folder inside it.
4. In the results viewer, pick classification **MO**, then PR **12842**, then one of
   the two files (`RocksDBStore.java` or `RocksDBTimestampedStore.java`). The patch
   and the matching target-variant code are shown side by side, with the clone
   matches highlighted.

No GitHub tokens or network access are needed to view these results.
