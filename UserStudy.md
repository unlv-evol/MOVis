## Formative User Study (Full Description)

This section provides the complete design, procedure, and analysis details of the formative user study summarized in the main paper. The study evaluates whether `MOVis` improves developers’ ability to localize missed opportunities compared to manual interpretation of `PaReco` output.

We employed a within-subjects design with six participants: two PhD students and four MSc students in computer science. The study was conducted with Institutional Review Board (IRB) approval. Each participant completed localization tasks using both `PaReco` and `MOVis`, with counterbalancing applied to mitigate ordering effects. Tasks required participants to identify the corresponding buggy region associated with a missed opportunity under two conditions:

1. Manual inspection using textual `PaReco` output
2. Visual inspection using `MOVis`

We collected both quantitative and qualitative data. Quantitative measures included task completion time and correctness of localization. Qualitative data were collected through post-task interviews, focusing on search effort, confidence, usability, and interface feedback. Interview responses were documented and analyzed to identify recurring themes explaining observed performance differences.

Table 1 summarizes the observed results. Participants achieved higher accuracy using `MOVis` at 100% compared to `PaReco` at 50%, and completed tasks substantially faster with `MOVis`, with a median time of 95.75 seconds compared to 602.5 seconds for `PaReco`. These results indicate that the visual alignment provided by `MOVis` improves both the efficiency and reliability of missed opportunity localization.

**Table 1. Summary of user study results.**

| Tool | Accuracy | Median Time |
|---|---:|---:|
| `MOVis` | 100% | 95.75s |
| `PaReco` | 50% | 602.5s |

Qualitative feedback further supports these findings. Participants consistently reported that the manual approach required extensive searching, scrolling, and mental alignment across files, whereas `MOVis` enabled immediate identification of corresponding regions through its side-by-side visualization. Participants also indicated higher confidence when using `MOVis`, attributing this to the explicit mapping between source and target code regions.

## Experiment

During the experiment, participants were screen-recorded only; no audio or camera recording was collected. The screen recordings were used solely to measure task completion time and to support analysis of participants’ interaction patterns during the localization task.

Participants were asked to identify the first missed-opportunity (MO) patch in a given pull request (PR). Their task was to locate the corresponding MO using the assigned tool, allowing us to evaluate whether the information presented by each tool was sufficient for successful localization and how long the localization process took.

The pull requests used for each tool were:

| Tool | Pull Request |
|---|---:|
| `MOVis` | PR 12842 |
| `PaReco` | PR 12709 |

After completing the experiment, participants were invited to take part in an optional post-experiment interview. The interview was designed to collect qualitative feedback about participants’ task experience, perceived effort, confidence, and preferences regarding the two systems. The interview questions were organized as follows.

### Task Experience

1. Which system made it easiest to locate the corresponding buggy region in the target, and why?
2. Can you describe the steps you followed to locate the buggy region when using each system?

### Effort and Workflow

3. Which system required more manual effort, such as searching, scrolling, or matching code fragments? Why?

### Confidence

4. With which system did you feel most confident that you had found the correct location? What contributed to that confidence?
5. At any point, did you feel uncertain about whether you had found the correct location? If so, when and why?

### MOVis-Specific Insights

6. What aspects of `MOVis` helped you understand the relationship between the source patch and the target code?
7. Were there any parts of `MOVis` that were confusing, unclear, or difficult to use?

### Design-Specific Questions

8. How did the side-by-side view of source and target code in `MOVis` affect your ability to locate and understand the corresponding buggy region?
9. `MOVis` displays all hunks at once instead of using a combo box for selection. How did this design affect your ability to navigate and inspect the changes? Which approach did you prefer, and why?

### Overall Reflection and Improvement

10. Overall, which system would you prefer to use for this task, and why?
11. What would you change or improve in `MOVis`?
