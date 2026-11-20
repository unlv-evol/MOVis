import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The segmented control that flips between the three input pages. It replaces
// the old menu bar, so it also carries the reset each menu entry performed.
//
// `model` is a list of { label, hint } objects; the selected index is reported
// through `selected` rather than bound directly, so the caller can clear its
// fields at the same time.
Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0

    signal selected(int index)

    implicitHeight: Theme.controlHeight + 10
    implicitWidth: row.implicitWidth + 10
    radius: Theme.radius
    color: Theme.surfaceSunken
    border.width: Theme.borderWidth
    border.color: Theme.border

    // The gold pill slides to whichever segment is active.
    Rectangle {
        id: highlight
        y: 5
        height: parent.height - 10
        width: row.children.length > root.currentIndex
               ? row.children[root.currentIndex].width : 0
        x: row.children.length > root.currentIndex
           ? row.x + row.children[root.currentIndex].x : 0
        radius: Theme.radiusSmall
        color: Theme.goldWash
        border.width: Theme.borderWidth
        border.color: Theme.gold

        Behavior on x { NumberAnimation { duration: Theme.animation; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Theme.animation; easing.type: Easing.OutCubic } }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            model: root.model

            Item {
                id: segment
                required property int index
                required property var modelData

                readonly property bool current: root.currentIndex === segment.index

                Layout.preferredWidth: Math.max(110, caption.implicitWidth + 30)
                Layout.preferredHeight: Theme.controlHeight

                Text {
                    id: caption
                    anchors.centerIn: parent
                    text: segment.modelData.label
                    color: segment.current ? Theme.goldBright
                                           : (mouse.containsMouse ? Theme.text
                                                                  : Theme.textMuted)
                    font.pixelSize: Theme.fontSize
                    font.bold: segment.current

                    Behavior on color { ColorAnimation { duration: Theme.animationFast } }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selected(segment.index)

                    ToolTip.visible: containsMouse
                                     && segment.modelData.hint !== undefined
                    ToolTip.delay: 600
                    ToolTip.text: segment.modelData.hint !== undefined
                                  ? segment.modelData.hint : ""
                }
            }
        }
    }
}
