import QtQuick
import QtQuick.Controls

// Drop-down list of choices.
//
// This is deliberately a pick-from-a-list control only: its content item is a
// plain Text, so setting `editable` would leave the user with nothing to type
// into. Use SuggestField when the value should be typed.
ComboBox {
    id: control

    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.fontSize
    hoverEnabled: true

    contentItem: Text {
        leftPadding: 10
        rightPadding: control.indicator.width + 6
        text: control.editable ? control.editText : control.displayText
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "▾"
        color: control.pressed ? Theme.gold : Theme.textMuted
        font.pixelSize: Theme.fontSize
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surfaceSunken
        border.width: Theme.borderWidth
        border.color: control.activeFocus || control.hovered ? Theme.gold
                                                             : Theme.border

        Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(280, contentItem.implicitHeight + 2 * padding)
        padding: 4

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.width: Theme.borderWidth
            border.color: Theme.borderStrong
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    delegate: ItemDelegate {
        required property var modelData
        required property int index

        width: control.width - 8
        height: Theme.controlHeight
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: modelData
            color: highlighted ? Theme.goldBright : Theme.text
            font.pixelSize: Theme.fontSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: highlighted ? Theme.goldWash : "transparent"
        }
    }
}
