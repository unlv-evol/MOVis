import QtQuick

// Indeterminate progress: a gold sliver sweeping a dark track. Used when there
// is no count to report, such as while npm installs.
Rectangle {
    id: root

    implicitHeight: 4
    radius: 2
    color: Theme.surfaceSunken
    clip: true

    Rectangle {
        id: sliver
        width: parent.width * 0.3
        height: parent.height
        radius: parent.radius
        color: Theme.gold

        SequentialAnimation on x {
            running: root.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: -sliver.width
                to: root.width
                duration: 1100
                easing.type: Easing.InOutQuad
            }
        }
    }
}
