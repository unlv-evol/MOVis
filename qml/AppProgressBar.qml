import QtQuick
import QtQuick.Controls

// Determinate bar that turns gold on completion, and sweeps when the total is
// not known yet.
ProgressBar {
    id: control

    implicitHeight: 6

    background: Rectangle {
        radius: 3
        color: Theme.surfaceSunken
        border.width: Theme.borderWidth
        border.color: Theme.border
    }

    contentItem: Item {
        clip: true

        Rectangle {
            id: fill
            width: control.indeterminate ? parent.width * 0.3
                                         : control.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: control.indeterminate ? Theme.textFaint
                                         : (control.value >= control.to ? Theme.gold
                                                                        : Theme.crimson)

            Behavior on width {
                enabled: !control.indeterminate
                NumberAnimation { duration: Theme.animation }
            }

            SequentialAnimation on x {
                running: control.indeterminate && control.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: -fill.width
                    to: control.width
                    duration: 1100
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
