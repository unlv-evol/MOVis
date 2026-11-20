import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MOVis 1.0

// Side-by-side viewer for one result folder. The left pane holds the hunks of
// the classified file, the right pane the file it was compared against; both
// arrive from the C++ controller already highlighted.
ApplicationWindow {
    id: window

    required property string rootDirectory

    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 640
    visible: false
    title: "MOVis Results Viewer"
    color: Theme.background

    ResultsViewerController {
        id: viewer
        rootDirectory: window.rootDirectory
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        // --- What is currently selected --------------------------------------
        SectionCard {
            Layout.fillWidth: true
            title: "Context"

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLoose

                FieldLabel { text: "Classification" }
                AppComboBox {
                    Layout.preferredWidth: 150
                    model: viewer.classifications
                    currentIndex: viewer.classificationIndex
                    onActivated: function(index) { viewer.selectClassification(index) }
                }

                FieldLabel { text: "PR Number" }
                AppComboBox {
                    Layout.preferredWidth: 170
                    model: viewer.pullRequests
                    currentIndex: viewer.pullRequestIndex
                    onActivated: function(index) { viewer.selectPullRequest(index) }
                }

                FieldLabel { text: "Search PR" }
                SuggestField {
                    id: prSearch
                    Layout.preferredWidth: 200
                    placeholderText: "PR number"
                    options: viewer.allPullRequests
                    onAccepted: function(value) { viewer.searchPullRequest(value) }
                }

                AppButton {
                    text: "Open PR"
                    onClicked: viewer.searchPullRequest(prSearch.text)
                }

                AppButton {
                    text: "Clear"
                    onClicked: {
                        prSearch.clear()
                        viewer.clearPullRequestSearch()
                    }
                }

                Item { Layout.fillWidth: true }
            }

            ContextRow {
                Layout.fillWidth: true
                label: "PR Title"
                Text {
                    Layout.fillWidth: true
                    text: viewer.prTitle
                    color: Theme.text
                    font.pixelSize: Theme.fontSize
                    elide: Text.ElideRight
                    ToolTip.visible: titleMouse.containsMouse
                    ToolTip.text: text

                    MouseArea {
                        id: titleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }
            }

            ContextRow {
                Layout.fillWidth: true
                label: "PR Link"
                LinkText {
                    Layout.fillWidth: true
                    text: viewer.prUrl
                    target: viewer.prUrl
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLoose

                ContextRow {
                    Layout.fillWidth: true
                    label: "PR Results"
                    LinkText {
                        Layout.fillWidth: true
                        text: window.fileNameOf(viewer.prResultsPath)
                        target: viewer.prResultsPath.length > 0
                                ? viewer.localFileUrl(viewer.prResultsPath) : ""
                    }
                }

                ContextRow {
                    Layout.fillWidth: true
                    label: "File Results"
                    LinkText {
                        Layout.fillWidth: true
                        text: window.fileNameOf(viewer.fileResultsPath)
                        target: viewer.fileResultsPath.length > 0
                                ? viewer.localFileUrl(viewer.fileResultsPath) : ""
                    }
                }
            }

            ContextRow {
                Layout.fillWidth: true
                label: "Classification"

                ClassificationBadge { classification: viewer.fileClassification }

                Text {
                    visible: viewer.hunkSideClassificationsVisible
                    text: viewer.hunkSideClassifications
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSize
                }

                Item { Layout.fillWidth: true }
            }

            ContextRow {
                Layout.fillWidth: true
                visible: viewer.errorDetailsVisible
                label: "Error Details"

                Text {
                    Layout.fillWidth: true
                    text: viewer.errorDetails
                    color: Theme.crimsonBright
                    font.pixelSize: Theme.fontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        // --- The two code panes ------------------------------------------------
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Item {
                implicitWidth: Theme.spacing

                Rectangle {
                    anchors.centerIn: parent
                    width: SplitHandle.pressed || SplitHandle.hovered ? 3 : 1
                    height: parent.height - 2 * Theme.radius
                    radius: width / 2
                    color: SplitHandle.pressed || SplitHandle.hovered
                           ? Theme.gold : Theme.border
                    Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                }
            }

            SectionCard {
                SplitView.preferredWidth: window.width / 2
                SplitView.minimumWidth: 340
                title: "Left · Source"

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTight

                    FieldLabel {
                        Layout.preferredWidth: window.paneLabelWidth
                        text: "Hunks"
                    }
                    AppComboBox {
                        Layout.fillWidth: true
                        enabled: viewer.hunkChoicesEnabled
                        model: viewer.hunkChoices
                        currentIndex: viewer.hunkIndex
                        onActivated: function(index) { viewer.selectHunk(index) }
                        ToolTip.visible: hovered
                        ToolTip.text: "Show every hunk or one hunk number. A selected hunk always shows both additions and deletions when available."
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: -Theme.spacingTight
                    spacing: Theme.spacingTight

                    FieldLabel {
                        Layout.preferredWidth: window.paneLabelWidth
                        text: "Find"
                    }
                    AppTextField {
                        id: leftSearch
                        Layout.fillWidth: true
                        placeholderText: "Search text from left..."
                        onAccepted: viewer.findLeftTextInRight(leftPane.selectedText, text)
                    }

                    AppButton {
                        Layout.preferredWidth: window.findButtonWidth
                        text: "Find in right ›"
                        onClicked: viewer.findLeftTextInRight(leftPane.selectedText,
                                                              leftSearch.text)
                    }
                }

                CodePane {
                    id: leftPane
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    documentHtml: viewer.leftDocument
                    lineNumbers: viewer.leftLineNumbers
                    lineBands: viewer.leftLineBands
                    placeholderText: "Pick a PR above to load its hunks"
                    onCursorMoved: function(position) { viewer.leftCursorMoved(position) }
                }
            }

            SectionCard {
                SplitView.preferredWidth: window.width / 2
                SplitView.minimumWidth: 340
                title: "Right · Compared"

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTight

                    FieldLabel {
                        Layout.preferredWidth: window.paneLabelWidth
                        text: viewer.rightSelectorLabel
                    }
                    AppComboBox {
                        Layout.fillWidth: true
                        model: viewer.rightFileChoices
                        currentIndex: viewer.rightFileIndex
                        onActivated: function(index) { viewer.selectRightFile(index) }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: -Theme.spacingTight
                    spacing: Theme.spacingTight

                    FieldLabel {
                        Layout.preferredWidth: window.paneLabelWidth
                        text: "Find"
                    }
                    AppTextField {
                        id: rightSearch
                        Layout.fillWidth: true
                        placeholderText: "Search text from right..."
                        onAccepted: viewer.findRightTextInLeft(rightPane.selectedText, text)
                    }

                    AppButton {
                        Layout.preferredWidth: window.findButtonWidth
                        text: "‹ Find in left"
                        onClicked: viewer.findRightTextInLeft(rightPane.selectedText,
                                                              rightSearch.text)
                    }
                }

                CodePane {
                    id: rightPane
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    documentHtml: viewer.rightDocument
                    lineNumbers: viewer.rightLineNumbers
                    lineBands: viewer.rightLineBands
                    placeholderText: "The compared file appears here"
                }
            }
        }
    }

    // Both code panes share these so their header rows line up exactly.
    readonly property int paneLabelWidth: 40
    readonly property int findButtonWidth: 124

    // Last path segment of a file path, or "-" when there is no path.
    function fileNameOf(path) {
        return path.length > 0 ? path.split(/[\\/]/).pop() : "-"
    }

    AppDialog {
        id: messageDialog
        anchors.centerIn: parent
        width: Math.min(520, window.width - 60)
        accent: Theme.crimson

        property string messageText

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
                onClicked: messageDialog.close()
            }
        ]
    }

    Connections {
        target: viewer

        function onMessageRequested(title, message) {
            messageDialog.title = title
            messageDialog.messageText = message
            messageDialog.open()
        }

        function onLeftNavigationRequested(selectionStart, selectionEnd) {
            leftPane.navigate(selectionStart, selectionEnd)
        }

        function onRightNavigationRequested(selectionStart, selectionEnd) {
            rightPane.navigate(selectionStart, selectionEnd)
        }

        function onPanesReset() {
            leftPane.reset()
            rightPane.reset()
        }

        function onSearchFieldsCleared() {
            leftSearch.text = ""
            rightSearch.text = ""
        }
    }
}
