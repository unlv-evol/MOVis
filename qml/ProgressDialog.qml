import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Live view of a classification run: the stage MOVis is on, then one row per
// pull request. Rows that have finished sink to the bottom of the list.
//
// `model` is MainWindow's ClassificationProgressModel, so a file-count update
// repaints only the row it belongs to instead of rebuilding the whole list.
AppDialog {
    id: root

    property string stageText
    property alias model: rows.model

    function statusColor(finished, status) {
        if (finished)
            return Theme.gold
        return status === "running" ? Theme.crimsonBright : Theme.textMuted
    }

    title: "Running GACPD Classification"
    width: 640
    height: 420
    closePolicy: Popup.NoAutoClose

    Text {
        Layout.fillWidth: true
        text: root.stageText
        color: Theme.text
        font.pixelSize: Theme.fontSize
        wrapMode: Text.WordWrap
    }

    BusyBar {
        Layout.fillWidth: true
        visible: rows.count === 0
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        ColumnLayout {
            width: root.availableWidth
            spacing: Theme.spacingTight

            Repeater {
                id: rows

                Rectangle {
                    required property string prNumber
                    required property int completedFiles
                    required property int totalFiles
                    required property string status
                    required property bool finished

                    Layout.fillWidth: true
                    implicitHeight: rowLayout.implicitHeight + 2 * Theme.spacingTight
                    radius: Theme.radiusSmall
                    color: Theme.surfaceRaised
                    border.width: Theme.borderWidth
                    border.color: finished ? Theme.goldDim : Theme.border
                    opacity: finished ? 0.6 : 1

                    Behavior on opacity { NumberAnimation { duration: Theme.animation } }

                    ColumnLayout {
                        id: rowLayout
                        anchors.fill: parent
                        anchors.margins: Theme.spacingTight
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingTight

                            Text {
                                text: "PR " + prNumber
                                color: Theme.text
                                font.pixelSize: Theme.fontSize
                                font.bold: true
                            }

                            Text {
                                text: "\u2014"
                                color: Theme.textFaint
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Text {
                                text: status
                                color: root.statusColor(finished, status)
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                            }

                            Text {
                                text: "\u2014"
                                color: Theme.textFaint
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Text {
                                text: completedFiles + " / " + totalFiles + " files"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Item { Layout.fillWidth: true }
                        }

                        AppProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: Math.max(1, totalFiles)
                            value: completedFiles
                            indeterminate: totalFiles === 0 && status === "running"
                        }
                    }
                }
            }
        }
    }
}
