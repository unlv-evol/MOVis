import QtQuick
import QtQuick.Controls

// Scrollable multi-line input used for the token list and the dataset repo
// lists. Reads `text` and reports edits through `edited` so the caller decides
// what to do with them.
Item {
    id: root

    property alias text: editor.text
    property alias placeholderText: editor.placeholderText
    property alias readOnly: editor.readOnly
    property bool monospace: false

    signal edited(string value)

    implicitHeight: 120

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: Theme.surfaceSunken
        border.width: Theme.borderWidth
        border.color: editor.activeFocus ? Theme.gold : Theme.border

        Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: Theme.borderWidth
        clip: true

        TextArea {
            id: editor
            color: Theme.text
            placeholderTextColor: Theme.textFaint
            selectByMouse: true
            selectionColor: Theme.goldWash
            selectedTextColor: Theme.text
            wrapMode: TextEdit.NoWrap
            padding: 8
            background: null
            font.pixelSize: Theme.fontSize
            font.family: root.monospace ? Theme.monoFontFamily : font.family

            // Only report what the user typed. Assignments coming back from
            // C++ must not be echoed straight back as a fresh edit.
            onTextChanged: if (activeFocus) root.edited(text)
        }
    }
}
