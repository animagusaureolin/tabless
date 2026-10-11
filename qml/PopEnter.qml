import QtQuick
import TodoCpp

// How a small popup arrives (APP-175): a menu, a picker, a suggestion list
// fades in at pop speed. Use as `enter: PopEnter {}` on a Popup; the
// animations act on the popup itself. Instant with "Reduce motion" on.
Transition {
    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durPop; easing.type: Theme.easeEnter }
}
