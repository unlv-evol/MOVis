import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Classify every pull request one variant repository merged from its source,
// across a date range MOVis works out from the divergence point.
ColumnLayout {
    id: page

    // MainWindow, passed in from Main.qml.
    required property var controller

    spacing: Theme.spacing

    SectionCard {
        Layout.fillWidth: true
        title: "Repositories"

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            LabeledField {
                Layout.fillWidth: true
                label: "Source Repository"
                placeholderText: "owner/repository"
                text: page.controller.sourceRepository
                onEdited: function(value) { page.controller.sourceRepository = value }
            }

            LabeledField {
                Layout.fillWidth: true
                label: "Variant Repository"
                placeholderText: "owner/repository"
                text: page.controller.variantRepository
                onEdited: function(value) { page.controller.variantRepository = value }
            }
        }

        RowLayout {
            spacing: Theme.spacing

            AppButton {
                text: "Get Divergence Date"
                onClicked: page.controller.checkDivergenceDate()
            }

            AppButton {
                text: "Override (Detached Pair)"
                onClicked: page.controller.overrideDivergenceDate()

                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: "For repositories outside the same fork network "
                              + "(e.g. Red5/red5-server -> ant-media/Ant-Media-Server). "
                              + "Skips the merge-base lookup and pre-fills the dates "
                              + "from repository metadata so you can edit them."
            }
        }
    }

    // Only shown once a divergence date has been resolved.
    SectionCard {
        Layout.fillWidth: true
        visible: page.controller.dateRangeVisible
        title: "Date Range"
        subtitle: "runs over six months are split into consecutive result folders"

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingLoose

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                FieldLabel { text: "Divergence Date" }
                DateField {
                    Layout.fillWidth: true
                    value: page.controller.divergenceDate
                    onEdited: function(value) { page.controller.divergenceDate = value }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                FieldLabel { text: "Cut-Off Date" }
                DateField {
                    Layout.fillWidth: true
                    value: page.controller.cutoffDate
                    onEdited: function(value) { page.controller.cutoffDate = value }
                }
            }
        }
    }

    SectionCard {
        Layout.fillWidth: true
        title: "Options"

        AppCheckBox {
            text: "Generate API JSON file"
            checked: page.controller.generateApiJsonNormal
            onToggled: page.controller.generateApiJsonNormal = checked
        }

        AppCheckBox {
            text: "Fetch PR commit lists"
            checked: page.controller.fetchPrCommitListNormal
            onToggled: page.controller.fetchPrCommitListNormal = checked
            ToolTip.visible: hovered
            ToolTip.text: "Fetch each PR commit list. PRs over 250 commits automatically use the GraphQL path."
        }
    }

    Item { Layout.fillHeight: true }

    RowLayout {
        Layout.fillWidth: true

        Item { Layout.fillWidth: true }

        AppButton {
            text: "Run Classification"
            primary: true
            enabled: page.controller.normalSubmitVisible
            onClicked: page.controller.submitNormal()

            ToolTip.visible: hovered && !enabled
            ToolTip.text: "Get the divergence date first."
        }
    }
}
