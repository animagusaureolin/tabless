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
//
// The growth is a Scale transform on the popup's own item, never
// `popup.scale`: Qt places a centred popup from its scaled size and does
// not place it again when the scale returns to 1, so a dialog that grew
// from 0.94 stayed 3 % of its size right and down of the centre and ran
// past the window edge (A6-13).
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
            arrival._install();
            arrival.popup.opacity = 0;
            arrival.grow = arrival.dialog ? Theme.dialogScaleFrom : 1;
            arrival.start();
        }
        function onAboutToHide() { arrival.stop(); arrival._rest(); }
    }
    function _rest() { arrival.popup.opacity = 1; arrival.grow = 1; }

    // The size the dialog grows through, 1 at rest.
    property real grow: 1
    readonly property Scale _scale: Scale {
        origin.x: arrival.popup ? arrival.popup.width / 2 : 0
        origin.y: arrival.popup ? arrival.popup.height / 2 : 0
        xScale: arrival.grow
        yScale: arrival.grow
    }
    // The popup's item is the parent of its content item; the transform
    // goes on it once, next to any it already carries.
    function _install() {
        const content = arrival.popup ? arrival.popup.contentItem : null;
        const item = content ? content.parent : null;
        if (!item)
            return;
        const list = [];
        for (let i = 0; i < item.transform.length; ++i) {
            if (item.transform[i] === arrival._scale)
                return;
            list.push(item.transform[i]);
        }
        list.push(arrival._scale);
        item.transform = list;
    }

    NumberAnimation {
        target: arrival.popup; property: "opacity"; to: 1
        duration: arrival.dialog ? Theme.durMove : Theme.durPop
        easing.type: Theme.easeEnter
    }
    NumberAnimation {
        target: arrival; property: "grow"; to: 1
        duration: arrival.dialog ? Theme.durMove : Theme.durPop
        easing.type: Theme.easeEnter
    }
}
