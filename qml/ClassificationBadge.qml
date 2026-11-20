import QtQuick

// Colour-coded pill for a GACPD classification code.
Rectangle {
    id: root

    property string classification: "-"

    readonly property color accent: {
        switch (root.classification) {
        case "MO": return Theme.gold          // modified
        case "ED": return Theme.goldBright    // exact duplicate
        case "NA": return Theme.textMuted     // not applicable
        case "CC": return Theme.crimson       // cannot compare
        case "NE": return Theme.textFaint     // not existing
        case "ERROR": return Theme.crimsonBright
        default: return Theme.textFaint
        }
    }

    implicitWidth: label.implicitWidth + 18
    implicitHeight: 22
    radius: height / 2
    color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.16)
    border.width: Theme.borderWidth
    border.color: root.accent

    Text {
        id: label
        anchors.centerIn: parent
        text: root.classification
        color: root.accent
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
        font.letterSpacing: 0.8
    }
}
