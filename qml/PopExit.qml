import QtQuick
import TodoCpp

// How a small popup leaves: the fade of PopEnter in half the time.
// `exit: PopExit {}`.
Transition {
    NumberAnimation { property: "opacity"; to: 0; duration: Theme.durPopOut; easing.type: Theme.easeExit }
}
