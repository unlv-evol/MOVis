import QtQuick
import QtQuick.Layouts

// One "Caption  value" pair in the results viewer's context panel. Put the
// value items inside; they sit immediately after the caption.
//
// Captions are left-aligned and share a fixed width so the values line up in a
// column without being pushed far to the right.
RowLayout {
    id: root

    property string label
    default property alias content: holder.data

    spacing: Theme.spacing

    FieldLabel {
        Layout.preferredWidth: 92
        Layout.alignment: Qt.AlignVCenter
        text: root.label
        elide: Text.ElideRight
    }

    RowLayout {
        id: holder
        Layout.fillWidth: true
        spacing: Theme.spacing
    }
}
