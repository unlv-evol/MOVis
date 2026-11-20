import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    hoverEnabled: true
    font.pixelSize: Theme.fontSize
    spacing: Theme.spacingTight

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: Theme.radiusSmall
        color: control.checked ? Theme.gold : Theme.surfaceSunken
        border.width: Theme.borderWidth
        border.color: control.checked ? Theme.gold
                                      : (control.hovered ? Theme.borderStrong
                                                         : Theme.border)

        Behavior on color { ColorAnimation { duration: Theme.animationFast } }

        // Checkmark.
        Text {
            anchors.centerIn: parent
            text: "✓"
            font.pixelSize: 13
            font.bold: true
            color: Theme.background
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.hovered ? Theme.text : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
