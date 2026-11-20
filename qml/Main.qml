import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore

// MOVis main window: pick a mode, fill in the repositories, run GACPD.
//
// `appController` is the MainWindow instance set as a context property in
// main.cpp; every field on the pages reads and writes it directly.
ApplicationWindow {
    id: window

    // Index into `modes` below, and into the page StackLayout.
    property int currentMode: 0

    // Results viewers are separate windows. Keep a reference to each so it is
    // not garbage collected while the user still has it open.
    property var viewerWindows: []

    readonly property var modes: [
        { label: "Normal", hint: "Every PR between two repositories over a date range" },
        { label: "One PR", hint: "A single pull request, by number" },
        { label: "Datasets", hint: "Many repository pairs in one batch" }
    ]

    width: settings.savedWidth
    height: settings.savedHeight
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "MOVis"
    color: Theme.background

    Settings {
        id: settings
        category: "MainWindow"
        property int savedWidth: 1040
        property int savedHeight: 680
    }

    onClosing: function(close) {
        settings.savedWidth = width
        settings.savedHeight = height
        appController.saveSettings()
    }

    // Switching modes starts that page from a clean slate, which is what the
    // old "Load Page" menu entries did.
    function selectMode(index) {
        window.currentMode = index
        appController.projectName = ""

        if (index === 0) {
            appController.sourceRepository = ""
            appController.variantRepository = ""
        } else if (index === 1) {
            appController.pullRequestNumber = ""
            appController.onePrSourceRepository = ""
            appController.onePrVariantRepository = ""
        } else {
            appController.datasetSources = ""
            appController.datasetVariants = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLoose
        spacing: Theme.spacing

        // --- Title row -----------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            ColumnLayout {
                spacing: 0

                Text {
                    text: "MOVis"
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                    font.letterSpacing: 1.5
                }

                Text {
                    text: window.modes[window.currentMode].hint
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Item { Layout.fillWidth: true }

            ModeSelector {
                model: window.modes
                currentIndex: window.currentMode
                onSelected: function(index) { window.selectMode(index) }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // --- Shared run settings -------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            LabeledField {
                // A nested layout fills by default; this one should not.
                Layout.fillWidth: false
                Layout.preferredWidth: 260
                label: "Root Folder Name"
                placeholderText: "Where results are saved"
                text: appController.projectName
                onEdited: function(value) { appController.projectName = value }
            }

            Item { Layout.fillWidth: true }

            AppButton {
                Layout.alignment: Qt.AlignBottom
                text: "Install Dependencies"
                onClicked: appController.installDependencies()
            }

            AppButton {
                Layout.alignment: Qt.AlignBottom
                text: "View Previous Results"
                onClicked: resultsFolderDialog.open()
            }

            AppButton {
                Layout.alignment: Qt.AlignBottom
                text: tokenPanel.visible ? "Hide Tokens" : "Show Tokens"
                onClicked: tokenPanel.visible = !tokenPanel.visible
            }
        }

        // --- Page + tokens --------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            // Each page supplies its own cards, so there is nothing to wrap
            // them in here.
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 440
                currentIndex: window.currentMode

                NormalPage { controller: appController }
                OnePrPage { controller: appController }
                DatasetPage { controller: appController }
            }

            SectionCard {
                id: tokenPanel
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                title: "GitHub Tokens"
                subtitle: "One personal access token per line."
                accent: Theme.crimson

                AppTextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    monospace: true
                    placeholderText: "ghp_..."
                    text: appController.tokens
                    onEdited: function(value) { appController.tokens = value }
                }
            }
        }

        // --- Footer ----------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            visible: appController.advancedResultsVisible

            Item { Layout.fillWidth: true }

            AppButton {
                text: "Advanced Results"
                onClicked: appController.openAdvancedResults()
            }
        }
    }

    FolderDialog {
        id: resultsFolderDialog
        title: "Select Root Directory"
        onAccepted: appController.openResultsDirectory(selectedFolder.toString())
        onRejected: messageDialog.showMessage(
                        "No Directory Selected",
                        "You must select a directory before continuing.", true)
    }

    AppDialog {
        id: messageDialog
        anchors.centerIn: parent
        width: Math.min(520, window.width - 60)

        property string messageText
        property bool acknowledgeable: true

        // `acknowledged` false means the message disappears on its own; the
        // classification code uses that for purely informational updates.
        function showMessage(titleText, bodyText, acknowledged) {
            title = titleText
            messageText = bodyText
            acknowledgeable = acknowledged
            open()
        }

        Text {
            Layout.fillWidth: true
            text: messageDialog.messageText
            color: Theme.text
            font.pixelSize: Theme.fontSize
            wrapMode: Text.WordWrap
        }

        actions: [
            AppButton {
                text: "OK"
                primary: true
                visible: messageDialog.acknowledgeable
                onClicked: messageDialog.close()
            }
        ]
    }

    AppDialog {
        id: resultChoiceDialog
        anchors.centerIn: parent
        width: Math.min(480, window.width - 60)
        title: "Select 6-Month Result Window"

        property var choices: []

        AppComboBox {
            id: resultChoice
            Layout.fillWidth: true
            model: resultChoiceDialog.choices
        }

        actions: [
            AppButton {
                text: "Cancel"
                onClicked: {
                    appController.chooseResultsWindow(-1)
                    resultChoiceDialog.close()
                }
            },
            AppButton {
                text: "Open"
                primary: true
                onClicked: {
                    appController.chooseResultsWindow(resultChoice.currentIndex)
                    resultChoiceDialog.close()
                }
            }
        ]
    }

    AppDialog {
        id: dependencyDialog
        anchors.centerIn: parent
        width: Math.min(500, window.width - 60)
        title: appController.dependencyProgressTitle
        visible: appController.dependencyInstallInProgress
        closePolicy: Popup.NoAutoClose

        Text {
            Layout.fillWidth: true
            text: appController.dependencyProgressMessage
            color: Theme.textMuted
            font.pixelSize: Theme.fontSize
            wrapMode: Text.WordWrap
        }

        BusyBar { Layout.fillWidth: true }
    }

    ProgressDialog {
        id: classificationDialog
        anchors.centerIn: parent
        title: appController.classificationProgressTitle
        stageText: appController.classificationStage
        model: appController.classificationProgress
        visible: appController.classificationProgressVisible
    }

    Component {
        id: resultsViewerComponent
        ResultsViewer {}
    }

    Connections {
        target: appController

        function onMessageRequested(title, message, requiresAcknowledgement) {
            messageDialog.showMessage(title, message, requiresAcknowledgement)
        }

        function onResultsWindowChoicesRequested(choices) {
            resultChoiceDialog.choices = choices
            resultChoice.currentIndex = 0
            resultChoiceDialog.open()
        }

        function onResultsViewerRequested(rootDirectory) {
            const viewer = resultsViewerComponent.createObject(null, {
                "rootDirectory": rootDirectory,
                "visible": true
            })
            if (viewer) {
                window.viewerWindows.push(viewer)
            } else {
                messageDialog.showMessage(
                            "Results Viewer Error",
                            "The results viewer could not be opened.", true)
            }
        }
    }
}
