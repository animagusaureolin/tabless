import QtQuick
import TodoCpp

// The dim layer behind every modal (APP-182). Use as
// `Overlay.modal: ModalScrim {}`; a modal without one fell back to the
// control style's grey, so dialogs dimmed the page by different amounts.
// The popup's enter/exit drives this layer's opacity; the Behavior lets the
// page dim and clear instead of blinking.
Rectangle {
    color: Theme.scrim
    Behavior on opacity { NumberAnimation { duration: Theme.durPop; easing.type: Theme.easeEnter } }
}
