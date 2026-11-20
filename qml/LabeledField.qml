import QtQuick
import QtQuick.Layouts

// A caption above a single-line input, which is how every repository and
// number field on the three pages is laid out.
ColumnLayout {
    id: root

    property alias label: caption.text
    property alias text: field.text
    property alias placeholderText: field.placeholderText

    signal edited(string value)

    spacing: 4

    FieldLabel {
        id: caption
        Layout.fillWidth: true
        elide: Text.ElideRight
    }

    AppTextField {
        id: field
        Layout.fillWidth: true

        // Ignore the assignment that happens when C++ pushes a new value in.
        onTextChanged: if (activeFocus) root.edited(text)
    }
}
