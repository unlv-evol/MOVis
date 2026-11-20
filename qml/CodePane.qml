import QtQuick
import QtQuick.Controls

// Read-only source view with a gutter of line numbers.
//
// The document arrives as HTML because the viewer paints its similarity
// highlights into it, and the gutter numbers arrive separately because a hunk
// view renumbers its lines to match the original file. `lineBands` repeats the
// highlights as whole-line runs so they can be painted across the full width
// of the pane, the way the old QPlainTextEdit viewer showed them.
//
// navigate() is how the controller points this pane at a position: it selects
// what was asked for, glides it to the middle of the view and briefly outlines
// the line it landed on. reset() puts the pane back at its first line when a
// different file is loaded.
Item {
    id: root

    property string documentHtml
    property string lineNumbers
    property var lineBands: []
    property string placeholderText: "Nothing loaded"

    readonly property string selectedText: editor.selectedText
    readonly property int cursorPosition: editor.cursorPosition

    // Every line has the same height: one monospace font, no wrapping.
    readonly property real lineHeight: editor.lineCount > 0
                                       ? editor.contentHeight / editor.lineCount
                                       : Theme.fontSize + 4

    signal cursorMoved(int position)

    function navigate(selectionStart, selectionEnd) {
        // Select exactly the range the controller asked for. Assigning
        // cursorPosition afterwards would collapse that selection, so it is
        // only done when there is no range to keep.
        editor.select(selectionStart, selectionEnd)

        if (selectionStart === selectionEnd)
            editor.cursorPosition = selectionEnd

        root.scrollTo(selectionStart)
        root.flash(selectionStart, selectionEnd)
    }

    function reset() {
        scrollAnimation.stop()
        flashAnimation.stop()
        jumpFlash.opacity = 0
        editor.deselect()
        editor.cursorPosition = 0
        view.contentY = 0
        view.contentX = 0
    }

    function lineAt(position) {
        return Math.floor(editor.positionToRectangle(position).y / root.lineHeight)
    }

    // Glides `position` to the middle of the view, as far as the content
    // allows.
    function scrollTo(position) {
        const target = editor.positionToRectangle(position)
        const middle = target.y - (view.height - root.lineHeight) / 2
        const destination = Math.max(0, Math.min(middle, view.contentHeight - view.height))

        view.contentX = 0
        scrollAnimation.stop()
        if (Math.abs(destination - view.contentY) < 1)
            return
        scrollAnimation.to = destination
        scrollAnimation.start()
    }

    // Outlines the lines from `start` to `end` and fades the outline out, so
    // the eye can find where a jump landed.
    function flash(start, end) {
        const first = root.lineAt(start)
        const last = Math.max(first, root.lineAt(end))
        jumpFlash.y = first * root.lineHeight
        jumpFlash.height = (last - first + 1) * root.lineHeight
        flashAnimation.restart()
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: Theme.paper
        border.width: Theme.borderWidth
        border.color: Theme.paperBorder
    }

    Flickable {
        id: view
        anchors.fill: parent
        anchors.margins: Theme.borderWidth
        contentWidth: content.width
        contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds
        // Dragging with the mouse selects text, as in a normal editor; only the
        // wheel and the scroll bars move the view.
        acceptedButtons: Qt.NoButton
        clip: true

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        NumberAnimation {
            id: scrollAnimation
            target: view
            property: "contentY"
            duration: 260
            easing.type: Easing.OutCubic
        }

        Row {
            id: content

            Rectangle {
                id: gutter
                width: Math.max(48, numbers.implicitWidth + 22)
                height: Math.max(view.height, editorArea.height)
                color: Theme.paperGutter

                // A thin strip of each highlight colour beside its line numbers.
                Repeater {
                    model: root.lineBands
                    delegate: Rectangle {
                        required property var modelData
                        x: gutter.width - 4
                        y: modelData.line * root.lineHeight
                        width: 3
                        height: modelData.count * root.lineHeight
                        color: Qt.darker(modelData.color, 1.25)
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    width: Theme.borderWidth
                    height: parent.height
                    color: Theme.paperBorder
                }

                Text {
                    id: numbers
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    text: root.lineNumbers
                    color: Theme.paperGutterText
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSize
                    horizontalAlignment: Text.AlignRight
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.lineHeight
                }
            }

            Item {
                id: editorArea
                width: Math.max(view.width - gutter.width, editor.implicitWidth + 20)
                height: Math.max(view.height, editor.implicitHeight)

                // Full-width similarity highlights, behind the text.
                Repeater {
                    model: root.lineBands
                    delegate: Rectangle {
                        required property var modelData
                        y: modelData.line * root.lineHeight
                        width: editorArea.width
                        height: modelData.count * root.lineHeight
                        color: modelData.color
                    }
                }

                Rectangle {
                    id: currentLine
                    visible: editor.activeFocus && editor.length > 0
                    y: Math.floor(editor.cursorRectangle.y / root.lineHeight) * root.lineHeight
                    width: editorArea.width
                    height: root.lineHeight
                    color: Theme.paperCurrentLine
                }

                Rectangle {
                    id: jumpFlash
                    width: editorArea.width
                    opacity: 0
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.paperFlash
                    radius: 2

                    SequentialAnimation {
                        id: flashAnimation
                        NumberAnimation {
                            target: jumpFlash; property: "opacity"
                            to: 1; duration: 90
                        }
                        PauseAnimation { duration: 450 }
                        NumberAnimation {
                            target: jumpFlash; property: "opacity"
                            to: 0; duration: 650; easing.type: Easing.InQuad
                        }
                    }
                }

                TextArea {
                    id: editor

                    // Set while the mouse is down so a drag-select reports
                    // where it ended rather than every step along the way.
                    property bool mouseSelecting: false

                    x: 10
                    width: editorArea.width - x
                    readOnly: true
                    selectByMouse: true
                    selectionColor: Theme.gold
                    selectedTextColor: Theme.paperText
                    wrapMode: TextEdit.NoWrap
                    textFormat: TextEdit.RichText
                    text: root.documentHtml
                    color: Theme.paperText
                    padding: 0
                    background: null
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSize

                    onPressed: mouseSelecting = true
                    onReleased: {
                        mouseSelecting = false
                        root.cursorMoved(cursorPosition)
                    }

                    onCursorPositionChanged: {
                        if (activeFocus && !mouseSelecting)
                            root.cursorMoved(cursorPosition)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: editor.length === 0
                    text: root.placeholderText
                    color: Theme.paperGutterText
                    font.pixelSize: Theme.fontSize
                }
            }
        }
    }
}
