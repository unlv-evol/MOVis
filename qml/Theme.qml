pragma Singleton

import QtQuick

// Every colour, size and duration the interface uses. Change a value here and
// the whole application follows; nothing else should hard-code a colour.
//
// The palette is white on black with gold for anything selected or focused,
// and crimson reserved for the buttons that actually start work.
QtObject {
    // --- Surfaces, darkest first --------------------------------------
    readonly property color background: "#0C0C0F"
    readonly property color surface: "#1A1A20"
    readonly property color surfaceRaised: "#24242C"
    readonly property color surfaceSunken: "#101014"
    readonly property color border: "#31313C"
    readonly property color borderStrong: "#43434F"

    // --- Text ----------------------------------------------------------
    readonly property color text: "#F6F5F1"
    readonly property color textMuted: "#9A9AA6"
    readonly property color textFaint: "#6B6B76"

    // --- Accents ---------------------------------------------------------
    readonly property color gold: "#D4AF37"
    readonly property color goldBright: "#EBC85A"
    readonly property color goldDim: "#8A7226"
    readonly property color crimson: "#B3123A"
    readonly property color crimsonBright: "#D4204E"
    readonly property color crimsonDim: "#7A0C28"

    // Translucent accents for hover and selection fills.
    readonly property color goldWash: Qt.rgba(0.83, 0.69, 0.22, 0.14)
    readonly property color crimsonWash: Qt.rgba(0.70, 0.07, 0.23, 0.18)
    readonly property color whiteWash: Qt.rgba(1, 1, 1, 0.06)

    // --- Code panes ------------------------------------------------------
    // The results viewer paints its similarity highlights as pastel
    // backgrounds with black text, so the two code panes stay a light "paper"
    // surface inset in the dark chrome. Do not darken these.
    readonly property color paper: "#FBFAF7"
    readonly property color paperText: "#16161A"
    readonly property color paperGutter: "#EDEBE4"
    readonly property color paperGutterText: "#94908A"
    readonly property color paperBorder: "#C9C5BB"
    // The line holding the cursor, and the outline that flashes on the line a
    // jump lands on.
    readonly property color paperCurrentLine: Qt.rgba(0.83, 0.69, 0.22, 0.12)
    readonly property color paperFlash: "#B8902A"

    // --- Metrics ---------------------------------------------------------
    readonly property int radiusSmall: 4
    readonly property int radius: 8
    readonly property int radiusLarge: 12

    readonly property int spacingTight: 6
    readonly property int spacing: 12
    readonly property int spacingLoose: 20

    readonly property int controlHeight: 32
    readonly property int borderWidth: 1

    readonly property int animationFast: 110
    readonly property int animation: 180

    // --- Type ------------------------------------------------------------
    readonly property int fontSizeSmall: 11
    readonly property int fontSize: 13
    readonly property int fontSizeLarge: 15
    readonly property int fontSizeTitle: 20

    readonly property string monoFontFamily:
        Qt.platform.os === "windows" ? "Consolas" : "Menlo"
}
