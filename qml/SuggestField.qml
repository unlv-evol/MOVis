import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A text field that offers matching entries from `options` while you type.
//
// It is not a ComboBox: the user types a value, sees the entries that contain
// what they typed, and either picks one or presses Return to accept what is in
// the field. `accepted` carries the final text either way.
Item {
    id: root

    property var options: []
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property int maximumSuggestions: 12
    // Open the list as soon as the field takes focus, with the field's text
    // selected so typing replaces it. For short fixed lists such as months.
    property bool suggestOnFocus: false
    readonly property bool editing: field.activeFocus

    signal accepted(string value)
    signal textEdited(string value)

    implicitWidth: 200
    implicitHeight: Theme.controlHeight

    function clear() {
        field.clear()
        suggestions.close()
    }

    // Rebuilds the suggestion list from what is currently typed and shows it
    // when there is anything to offer.
    function refreshSuggestions() {
        root.showSuggestions(root.matchesFor(field.text))
    }

    function showSuggestions(matches) {
        suggestionList.model = matches
        suggestionList.currentIndex = 0
        suggestionList.navigated = false

        if (suggestionList.model.length > 0)
            suggestions.open()
        else
            suggestions.close()
    }

    // Entries containing the typed text, longest-shot matches last. An empty
    // field offers the first few entries so the list is discoverable.
    function matchesFor(typed) {
        const needle = typed.trim().toLowerCase()
        const all = root.options || []
        let starts = []
        let contains = []

        for (let i = 0; i < all.length; ++i) {
            const option = String(all[i])
            const haystack = option.toLowerCase()

            if (needle.length === 0 || haystack.startsWith(needle))
                starts.push(option)
            else if (haystack.indexOf(needle) >= 0)
                contains.push(option)

            if (starts.length >= root.maximumSuggestions)
                break
        }

        return starts.concat(contains).slice(0, root.maximumSuggestions)
    }

    AppTextField {
        id: field
        anchors.fill: parent

        onTextEdited: {
            root.refreshSuggestions()
            root.textEdited(text)
        }

        onActiveFocusChanged: {
            if (activeFocus && root.suggestOnFocus) {
                // Deferred so the click that gave focus does not clear it.
                Qt.callLater(selectAll)
                root.showSuggestions(root.matchesFor(""))
            }
        }

        // Return takes the entry picked with the arrow keys, if any;
        // otherwise exactly what was typed.
        onAccepted: {
            if (suggestions.opened && suggestionList.navigated
                    && suggestionList.currentIndex >= 0
                    && suggestionList.currentIndex < suggestionList.count) {
                text = suggestionList.model[suggestionList.currentIndex]
            }
            suggestions.close()
            root.accepted(text)
        }

        Keys.onDownPressed: {
            if (suggestions.opened && suggestionList.count > 0) {
                suggestionList.navigated = true
                suggestionList.currentIndex =
                    Math.min(suggestionList.currentIndex + 1, suggestionList.count - 1)
            }
        }

        Keys.onUpPressed: {
            if (suggestions.opened && suggestionList.count > 0) {
                suggestionList.navigated = true
                suggestionList.currentIndex =
                    Math.max(suggestionList.currentIndex - 1, 0)
            }
        }
    }

    Popup {
        id: suggestions
        y: root.height + 2
        width: root.width
        implicitHeight: Math.min(240, suggestionList.contentHeight + 2 * padding)
        padding: 4
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.width: Theme.borderWidth
            border.color: Theme.borderStrong
        }

        ListView {
            id: suggestionList
            anchors.fill: parent
            clip: true
            currentIndex: 0
            model: []

            // True once the arrow keys have moved through the list.
            property bool navigated: false

            ScrollIndicator.vertical: ScrollIndicator {}

            delegate: ItemDelegate {
                required property string modelData
                required property int index

                width: suggestionList.width
                height: 26
                highlighted: suggestionList.currentIndex === index

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

                onClicked: {
                    field.text = modelData
                    suggestions.close()
                    root.accepted(modelData)
                }
            }
        }
    }
}
