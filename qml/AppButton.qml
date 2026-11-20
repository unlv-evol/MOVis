import QtQuick
import QtQuick.Controls

// Standard button. `primary` marks the one action that starts real work on a
// page and paints it crimson; everything else stays a quiet outlined button.
Button {
    id: control

    property bool primary: false

    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(96, label.implicitWidth + 32)
    hoverEnabled: true
    font.pixelSize: Theme.fontSize
    font.bold: control.primary

    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.enabled ? (control.primary ? Theme.text : Theme.gold)
                               : Theme.textFaint
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        border.width: Theme.borderWidth
        color: {
            if (!control.enabled)
                return "transparent"
            if (control.primary)
                return control.down ? Theme.crimsonDim
                                    : (control.hovered ? Theme.crimsonBright
                                                       : Theme.crimson)
            return control.down ? Theme.goldWash
                                : (control.hovered ? Theme.whiteWash
                                                   : "transparent")
        }
        border.color: {
            if (!control.enabled)
                return Theme.border
            if (control.primary)
                return control.hovered ? Theme.crimsonBright : Theme.crimson
            return control.hovered ? Theme.gold : Theme.borderStrong
        }

        Behavior on color { ColorAnimation { duration: Theme.animationFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
    }
}
