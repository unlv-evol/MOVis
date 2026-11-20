import QtQuick
import QtQuick.Controls

// Single-line input. The gold underline marks the field that has focus.
TextField {
    id: control

    implicitHeight: Theme.controlHeight
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectByMouse: true
    selectionColor: Theme.goldWash
    selectedTextColor: Theme.text
    font.pixelSize: Theme.fontSize
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surfaceSunken
        border.width: Theme.borderWidth
        border.color: control.activeFocus ? Theme.gold : Theme.border

        Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 1
            height: 2
            radius: 1
            color: Theme.gold
            opacity: control.activeFocus ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }
        }
    }
}
