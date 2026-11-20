import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shared frame for every modal in the application: dark card, gold rule under
// the title, and a right-aligned row of buttons.
//
//   AppDialog {
//       title: "Delete everything?"
//       Text { text: "Really?" }
//       actions: [ AppButton { text: "Cancel" }, AppButton { primary: true } ]
//   }
Dialog {
    id: root

    // Colour of the rule under the title. Crimson marks a warning or failure.
    property color accent: Theme.gold

    default property alias body: bodyColumn.data
    property alias actions: actionRow.data

    modal: true
    padding: Theme.spacingLoose
    horizontalPadding: Theme.spacingLoose
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surface
        border.width: Theme.borderWidth
        border.color: Theme.borderStrong
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.6)
    }

    header: ColumnLayout {
        spacing: 0
        visible: root.title.length > 0

        Text {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLoose
            Layout.bottomMargin: Theme.spacing
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLoose
            Layout.rightMargin: Theme.spacingLoose
            height: 2
            radius: 1
            color: root.accent
        }
    }

    contentItem: ColumnLayout {
        id: bodyColumn
        spacing: Theme.spacing
    }

    footer: Item {
        implicitHeight: actionRow.children.length > 0
                        ? actionRow.implicitHeight + 2 * Theme.spacingLoose : 0

        RowLayout {
            id: actionRow
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.spacingLoose
            spacing: Theme.spacingTight
        }
    }
}
