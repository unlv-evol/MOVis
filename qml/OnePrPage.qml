import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Classify a single pull request by number. MOVis resolves the date range for
// that PR itself, so there is nothing to pick here.
ColumnLayout {
    id: page

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
                text: page.controller.onePrSourceRepository
                onEdited: function(value) { page.controller.onePrSourceRepository = value }
            }

            LabeledField {
                Layout.fillWidth: true
                label: "Variant Repository"
                placeholderText: "owner/repository"
                text: page.controller.onePrVariantRepository
                onEdited: function(value) { page.controller.onePrVariantRepository = value }
            }
        }

        LabeledField {
            Layout.fillWidth: false
            Layout.preferredWidth: 200
            label: "Source PR Number"
            placeholderText: "e.g. 1423"
            text: page.controller.pullRequestNumber
            onEdited: function(value) { page.controller.pullRequestNumber = value }
        }
    }

    SectionCard {
        Layout.fillWidth: true
        title: "Options"

        AppCheckBox {
            text: "Generate API JSON file"
            checked: page.controller.generateApiJsonOnePr
            onToggled: page.controller.generateApiJsonOnePr = checked
        }

        AppCheckBox {
            text: "Fetch PR commit lists"
            checked: page.controller.fetchPrCommitListOnePr
            onToggled: page.controller.fetchPrCommitListOnePr = checked
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
            onClicked: page.controller.submitOnePullRequest()
        }
    }
}
