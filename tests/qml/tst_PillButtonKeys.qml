// A focused PillButton answers Return and the keypad Enter as well as Space
// (A1-9, A7-1, A5-16): a dialog that opens with a pill focused ("Копировать
// Markdown", "Оставить") did nothing on Enter.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "PillButtonKeys"
    when: windowShown
    visible: true
    width: 300
    height: 200

    PillButton {
        id: pill
        property int hits: 0
        text: "Keep"
        onClicked: pill.hits++
    }
    PillButton {
        id: toggle
        y: 60
        text: "Toggle"
        checkable: true
    }
    PillButton {
        id: off
        y: 120
        text: "Off"
        enabled: false
        property int hits: 0
        onClicked: off.hits++
    }

    function init() {
        pill.hits = 0;
        off.hits = 0;
        toggle.checked = false;
    }

    function test_return_enter_and_space_press_it() {
        pill.forceActiveFocus(Qt.TabFocusReason);
        keyClick(Qt.Key_Return);
        compare(pill.hits, 1, "Return");
        keyClick(Qt.Key_Enter);
        compare(pill.hits, 2, "keypad Enter");
        keyClick(Qt.Key_Space);
        compare(pill.hits, 3, "Space, as before");
    }

    function test_a_checkable_pill_toggles_on_return() {
        toggle.forceActiveFocus(Qt.TabFocusReason);
        keyClick(Qt.Key_Return);
        verify(toggle.checked);
        keyClick(Qt.Key_Return);
        verify(!toggle.checked);
    }

    function test_a_disabled_pill_takes_nothing() {
        off.forceActiveFocus(Qt.TabFocusReason);
        keyClick(Qt.Key_Return);
        compare(off.hits, 0);
    }
}
