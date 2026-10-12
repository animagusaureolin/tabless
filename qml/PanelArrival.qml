import QtQuick
import TodoCpp

// A side panel arriving (APP-175): the task document, the meeting editor.
// It slides the last stretch in from the right edge it lives on and fades
// in at move speed; keys work at once, the move only paints. Leaving is
// instant: the panel empties the moment it closes, so there is nothing
// left to animate. Wire it as
//
//   transform: [arrival.shift]
//   PanelArrival { id: arrival; target: root; shown: root.opened }
//
// `replay()` plays it again in place (no slide), for a panel that changes
// size or parent while open.
Item {
    id: arrival
    visible: false

    required property Item target
    property bool shown: false
    readonly property Translate shift: Translate { x: 0 }

    function play(from) {
        anim.stop();
        arrival.shift.x = from;
        arrival.target.opacity = 0;
        anim.start();
    }
    function replay() { if (arrival.shown) arrival.play(0); }

    onShownChanged: {
        if (arrival.shown) { arrival.play(Theme.panelShift); return; }
        anim.stop();
        arrival.shift.x = 0;
        arrival.target.opacity = 1;
    }

    ParallelAnimation {
        id: anim
        NumberAnimation { target: arrival.shift; property: "x"; to: 0; duration: Theme.durMove; easing.type: Theme.easeEnter }
        NumberAnimation { target: arrival.target; property: "opacity"; to: 1; duration: Theme.durMove; easing.type: Theme.easeEnter }
    }
}
