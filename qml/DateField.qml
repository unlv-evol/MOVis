import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// yyyy-MM-dd text entry with a calendar popup. Reports the chosen date through
// `edited`; it never writes back to `value` itself.
Item {
    id: root

    property string value

    signal edited(string value)

    implicitWidth: 170
    implicitHeight: Theme.controlHeight

    function isoDate(date) {
        const month = String(date.getMonth() + 1).padStart(2, "0")
        const day = String(date.getDate()).padStart(2, "0")
        return date.getFullYear() + "-" + month + "-" + day
    }

    function monthNames() {
        let names = []
        for (let month = 0; month < 12; ++month)
            names.push(Qt.locale().monthName(month))
        return names
    }

    // Newest first, so typing "20" offers the recent years at the top.
    function yearOptions() {
        let years = []
        for (let year = new Date().getFullYear() + 1; year >= 1990; --year)
            years.push(String(year))
        return years
    }

    // Month index for typed text: a number from 1 to 12, or the start of a
    // month name ("oct", "ju" finds June). -1 when nothing matches yet.
    function findMonth(text) {
        const typed = text.trim().toLowerCase()
        if (typed.length === 0)
            return -1
        if (/^\d{1,2}$/.test(typed)) {
            const number = parseInt(typed, 10)
            return number >= 1 && number <= 12 ? number - 1 : -1
        }
        const names = root.monthNames()
        for (let month = 0; month < 12; ++month) {
            if (names[month].toLowerCase().startsWith(typed))
                return month
        }
        return -1
    }

    // A complete four-digit year, or -1 while one is still being typed.
    function findYear(text) {
        const typed = text.trim()
        if (!/^\d{4}$/.test(typed))
            return -1
        const year = parseInt(typed, 10)
        return year >= 1900 && year <= 2100 ? year : -1
    }

    // Shows the grid's month and year in the header fields, leaving alone a
    // field the user is typing in.
    function syncHeader() {
        if (!monthField.editing)
            monthField.text = Qt.locale().monthName(calendar.month)
        if (!yearField.editing)
            yearField.text = String(calendar.year)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 4

        AppTextField {
            id: field
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.value
            placeholderText: "yyyy-MM-dd"
            validator: RegularExpressionValidator {
                regularExpression: /\d{4}-\d{2}-\d{2}/
            }
            onEditingFinished: root.edited(text)
        }

        AppButton {
            Layout.preferredWidth: Theme.controlHeight
            Layout.fillHeight: true
            text: "▾"
            onClicked: {
                // Open the popup on the month currently in the field.
                const parsed = new Date(root.value + "T12:00:00")
                if (!isNaN(parsed.getTime())) {
                    calendar.month = parsed.getMonth()
                    calendar.year = parsed.getFullYear()
                }
                calendarPopup.open()
            }
        }
    }

    Popup {
        id: calendarPopup
        y: root.height + 4
        width: 300
        height: calendarColumn.implicitHeight + 2 * padding
        padding: Theme.spacing
        // Non-negative margins keep the popup inside the window instead of
        // letting it run off the bottom edge.
        margins: Theme.spacingTight
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        // Open above the field when there is not enough room below it, e.g.
        // for the Hard End Date field at the bottom of the Dataset page.
        onAboutToShow: {
            const overlay = Overlay.overlay
            if (!overlay)
                return
            const fieldBottom = root.mapToItem(overlay, 0, root.height).y
            const fitsBelow = fieldBottom + 4 + height + margins <= overlay.height
            y = fitsBelow ? root.height + 4 : -height - 4
        }

        onOpened: root.syncHeader()

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.width: Theme.borderWidth
            border.color: Theme.borderStrong
        }

        ColumnLayout {
            id: calendarColumn
            anchors.fill: parent
            spacing: Theme.spacingTight

            // Month and year can be stepped with the arrows, picked from their
            // lists, or typed: the grid follows as soon as what is typed
            // identifies a month ("oc", "10") or a four-digit year.
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                AppButton {
                    Layout.preferredWidth: 32
                    text: "‹"
                    onClicked: calendar.shiftMonth(-1)
                }

                SuggestField {
                    id: monthField
                    Layout.fillWidth: true
                    placeholderText: "Month"
                    options: root.monthNames()
                    suggestOnFocus: true

                    onTextEdited: function(value) {
                        const month = root.findMonth(value)
                        if (month >= 0) calendar.month = month
                    }
                    onAccepted: function(value) {
                        const month = root.findMonth(value)
                        if (month >= 0) calendar.month = month
                        text = Qt.locale().monthName(calendar.month)
                    }
                    onEditingChanged: if (!editing) root.syncHeader()
                }

                SuggestField {
                    id: yearField
                    Layout.preferredWidth: 72
                    placeholderText: "Year"
                    options: root.yearOptions()

                    onTextEdited: function(value) {
                        const year = root.findYear(value)
                        if (year > 0) calendar.year = year
                    }
                    onAccepted: function(value) {
                        const year = root.findYear(value)
                        if (year > 0) calendar.year = year
                        text = String(calendar.year)
                    }
                    onEditingChanged: if (!editing) root.syncHeader()
                }

                AppButton {
                    Layout.preferredWidth: 32
                    text: "›"
                    onClicked: calendar.shiftMonth(1)
                }
            }

            DayOfWeekRow {
                Layout.fillWidth: true
                locale: calendar.locale
                delegate: Text {
                    required property string shortName
                    text: shortName
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            MonthGrid {
                id: calendar
                Layout.fillWidth: true
                // MonthGrid sizes its cells from its own height, so the grid
                // needs one: six rows of day cells.
                Layout.preferredHeight: 6 * 30
                month: new Date().getMonth()
                year: new Date().getFullYear()

                function shiftMonth(delta) {
                    const shifted = new Date(year, month + delta, 1)
                    month = shifted.getMonth()
                    year = shifted.getFullYear()
                }

                onMonthChanged: root.syncHeader()
                onYearChanged: root.syncHeader()

                // The date in the field is filled gold; today is in gold text.
                delegate: Rectangle {
                    id: dayCell
                    required property var model
                    readonly property bool selected: root.isoDate(model.date) === root.value
                    readonly property bool inMonth: model.month === calendar.month

                    implicitHeight: 28
                    radius: Theme.radiusSmall
                    color: selected ? Theme.gold
                                    : (dayHover.hovered ? Theme.whiteWash : "transparent")

                    HoverHandler { id: dayHover }

                    Text {
                        anchors.fill: parent
                        text: dayCell.model.day
                        color: dayCell.selected ? Theme.background
                               : !dayCell.inMonth ? Theme.textFaint
                               : dayCell.model.today ? Theme.goldBright : Theme.text
                        font.pixelSize: Theme.fontSize
                        font.bold: dayCell.selected || dayCell.model.today
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                onClicked: function(date) {
                    root.edited(root.isoDate(date))
                    calendarPopup.close()
                }
            }
        }
    }
}
