import QtQuick
import TodoCpp

// How a popup arrives (APP-175). A dialog carries more than a menu, so it
// moves at move speed and grows from just under its size while it fades
// in; a picker or a suggestion list only fades in at pop speed. No
// overshoot. Put one inside the popup:
//
//   PopupArrival { popup: root }                  // a dialog
//   PopupArrival { popup: picker; dialog: false } // a small popup
//
// Not an `enter:` transition on purpose: Qt holds `opened` false until a
// transition ends, and the code asks `opened` everywhere (which popup is
// on top, whether a press fell beside it, which capture to switch away
// from). This plays on the popup while it is already open, so keys, focus
// and `opened` are live from the first frame. A dialog closes at once.
ParallelAnimation {
    id: arrival

    // A Popup or a Dialog, from either Controls import: `var`, as the two
    // namespaces type-check as different classes.
    required property var popup
    property bool dialog: true

    readonly property Connections _watch: Connections {
        target: arrival.popup
        function onAboutToShow() {
            arrival.stop();
            if (Theme.motion === 0) { arrival._rest(); return; }
            arrival.popup.opacity = 0;
            arrival.popup.scale = arrival.dialog ? Theme.dialogScaleFrom : 1;
            arrival.start();
        }
        function onAboutToHide() { arrival.stop(); arrival._rest(); }
    }
    function _rest() { arrival.popup.opacity = 1; arrival.popup.scale = 1; }

    NumberAnimation {
        target: arrival.popup; property: "opacity"; to: 1
        duration: arrival.dialog ? Theme.durMove : Theme.durPop
        easing.type: Theme.easeEnter
    }
    NumberAnimation {
        target: arrival.popup; property: "scale"; to: 1
        duration: arrival.dialog ? Theme.durMove : Theme.durPop
        easing.type: Theme.easeEnter
    }
}
