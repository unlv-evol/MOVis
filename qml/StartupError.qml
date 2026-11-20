import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shown instead of the main window when MOVis cannot create or write to its
// runtime directories. `startupTitle` and `startupMessage` are context
// properties set in main.cpp.
ApplicationWindow {
    width: 600
    height: 320
    minimumWidth: 440
    minimumHeight: 240
    visible: true
    title: startupTitle
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLoose
        spacing: Theme.spacing

        Text {
            Layout.fillWidth: true
            text: startupTitle
            color: Theme.crimsonBright
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            height: 2
            radius: 1
            color: Theme.crimson
        }

        AppTextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            text: startupMessage
        }

        AppButton {
            Layout.alignment: Qt.AlignRight
            text: "Quit"
            primary: true
            onClicked: Qt.quit()
        }
    }
}
