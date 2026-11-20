import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Classify many source/variant pairs in one batch. The two lists are matched
// line by line, so they must hold the same number of repositories.
ColumnLayout {
    id: page

    required property var controller

    spacing: Theme.spacing

    SectionCard {
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: "Repository Pairs"
        subtitle: "matched line by line, so both lists must be the same length"

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                FieldLabel { text: "Source Repositories" }
                AppTextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    monospace: true
                    placeholderText: "One owner/repository per line"
                    text: page.controller.datasetSources
                    onEdited: function(value) { page.controller.datasetSources = value }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                FieldLabel { text: "Variant Repositories" }
                AppTextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    monospace: true
                    placeholderText: "One owner/repository per line"
                    text: page.controller.datasetVariants
                    onEdited: function(value) { page.controller.datasetVariants = value }
                }
            }
        }
    }

    SectionCard {
        Layout.fillWidth: true
        title: "Options"

        AppCheckBox {
            text: "Run again with source and variants flipped"
            checked: page.controller.runDatasetReversed
            onToggled: page.controller.runDatasetReversed = checked
        }

        AppCheckBox {
            text: "Generate API JSON file"
            checked: page.controller.generateApiJsonDataset
            onToggled: page.controller.generateApiJsonDataset = checked
        }

        AppCheckBox {
            text: "Fetch PR commit lists"
            checked: page.controller.fetchPrCommitListDataset
            onToggled: page.controller.fetchPrCommitListDataset = checked
            ToolTip.visible: hovered
            ToolTip.text: "Fetch each PR commit list. PRs over 250 commits automatically use the GraphQL path."
        }

        AppCheckBox {
            text: "Skip repository deletion"
            checked: page.controller.skipRepoDeletionDataset
            onToggled: page.controller.skipRepoDeletionDataset = checked
            ToolTip.visible: hovered
            ToolTip.text: "Keep cloned repositories after each run and reuse them for later "
                          + "windows instead of cloning again. Uses more disk space; the next "
                          + "run without this option clears them."
        }
    }

    // Keeps a whole dataset inside one fixed period, so a re-run covers
    // exactly the same ground as the last one.
    SectionCard {
        Layout.fillWidth: true
        title: "Dataset - Debug"
        accent: Theme.crimson

        AppCheckBox {
            id: endDateLimit
            text: "Stop every pair at a fixed end date"
            checked: page.controller.datasetEndDateLimited
            onToggled: page.controller.datasetEndDateLimited = checked
            ToolTip.visible: hovered
            ToolTip.text: "Each pair still starts at its own divergence date, but nothing past this date is fetched."
        }

        RowLayout {
            Layout.fillWidth: true
            visible: endDateLimit.checked
            spacing: Theme.spacing

            FieldLabel {
                Layout.alignment: Qt.AlignVCenter
                text: "Hard End Date"
            }

            DateField {
                value: page.controller.datasetEndDateLimit
                onEdited: function(value) { page.controller.datasetEndDateLimit = value }
            }

            Item { Layout.fillWidth: true }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        Item { Layout.fillWidth: true }

        AppButton {
            text: "Run Batch"
            primary: true
            onClicked: page.controller.submitDataset()
        }
    }
}
