import QtQuick

// A path or URL rendered as a clickable link. Shows "-" and stays inert when
// there is nothing to open.
Text {
    id: root

    property url target

    readonly property bool openable: target.toString().length > 0 && text !== "-"

    color: openable ? (mouse.containsMouse ? Theme.goldBright : Theme.gold)
                    : Theme.textFaint
    font.pixelSize: Theme.fontSize
    font.underline: openable && mouse.containsMouse
    elide: Text.ElideMiddle
    verticalAlignment: Text.AlignVCenter

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.openable
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally(root.target)
    }
}
