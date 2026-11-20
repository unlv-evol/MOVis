import QtQuick
import QtQuick.Layouts

// A titled panel. Put the contents straight inside it; they are laid out in a
// column and the card grows to fit.
//
//   SectionCard {
//       title: "Date Range"
//       AppTextField { Layout.fillWidth: true }
//   }
Rectangle {
    id: root

    property string title
    property string subtitle

    // Colour of the dash beside the title. Crimson marks anything sensitive
    // or failing.
    property color accent: Theme.gold

    default property alias content: contentColumn.data

    implicitHeight: layout.implicitHeight + 2 * Theme.spacing
    implicitWidth: layout.implicitWidth + 2 * Theme.spacing
    radius: Theme.radius
    color: Theme.surface
    border.width: Theme.borderWidth
    border.color: Theme.border

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacingTight

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 2
            visible: root.title.length > 0
            spacing: Theme.spacingTight

            Rectangle {
                width: 14
                height: 2
                radius: 1
                color: root.accent
            }

            Text {
                Layout.fillWidth: true
                text: root.title.toUpperCase()
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                font.letterSpacing: 1.2
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 2
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textFaint
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing
        }
    }
}
