// The crash card after an unclean exit is the only layer until it is answered
// (A7-2): global keys stay off (Ctrl+2 switched the hidden view, Ctrl+N opened
// a task over the card), Tab walks its two answers, Esc answers "Не
// отправлять" even after the view under it was built.
//
// Drives the real Main.qml with real key events.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "CrashCardKeys"
    when: windowShown

    property var win: null

    function find(root, pred) {
        if (!root) return null;
        if (pred(root)) return root;
        const kids = root.children || [];
        for (let i = 0; i < kids.length; i++) {
            const r = find(kids[i], pred);
            if (r) return r;
        }
        return null;
    }
    function byName(root, name) { return find(root, function (it) { return it.objectName === name; }); }
    function inside(item, root) {
        for (let p = item; p; p = p.parent) if (p === root) return true;
        return false;
    }

    function initTestCase() {
        AppController.simulateUncleanExitForTest(new Date(2026, 9, 9, 15, 58));
        AppController.currentView = "today";
        const comp = Qt.createComponent("qrc:/qt/qml/TodoCpp/qml/Main.qml");
        tryCompare(comp, "status", Component.Ready, 5000);
        verify(comp.status === Component.Ready, comp.errorString());
        tc.win = comp.createObject(null);
        verify(tc.win !== null);
        tc.win.width = 1200;
        tc.win.height = 800;
        tc.win.requestActivate();
        wait(800);
    }

    function cleanupTestCase() {
        AppController.dismissUncleanExit();
        if (tc.win) tc.win.destroy();
        tc.win = null;
    }

    function test_the_card_is_the_only_layer() {
        const splash = byName(tc.win.contentItem, "splash");
        verify(splash !== null);
        tryVerify(function () { return splash.ready; }, 3000);
        verify(splash.waiting, "the crash card waits for an answer");
        tryVerify(function () { return inside(tc.win.activeFocusItem, splash); }, 2000,
                  "the card holds the keyboard after the view under it was built");

        keyClick(Qt.Key_2, Qt.ControlModifier);
        wait(50);
        compare(AppController.currentView, "today", "Ctrl+2 did not switch the hidden view");
        keyClick(Qt.Key_N, Qt.ControlModifier);
        wait(100);
        verify(!tc.win._overlayOpen, "Ctrl+N opened nothing over the card");
        keyClick(Qt.Key_J);
        keyClick(Qt.Key_Slash);
        wait(50);
        verify(inside(tc.win.activeFocusItem, splash), "letters stay with the card");

        keyClick(Qt.Key_Tab);
        verify(inside(tc.win.activeFocusItem, splash), "Tab stays on the card");
        const first = tc.win.activeFocusItem;
        keyClick(Qt.Key_Tab);
        verify(inside(tc.win.activeFocusItem, splash));
        verify(tc.win.activeFocusItem !== first, "Tab walks between the two answers");
        keyClick(Qt.Key_Backtab);
        compare(tc.win.activeFocusItem, first);

        // Something else takes the keyboard: the card takes it back.
        tc.win.focusActiveView();
        tryVerify(function () { return inside(tc.win.activeFocusItem, splash); }, 1000);

        keyClick(Qt.Key_Escape);
        tryVerify(function () { return !splash.waiting; }, 1000, "Esc answers \"Не отправлять\"");
        verify(!AppController.lastExitUnclean);
        // Global keys are back.
        keyClick(Qt.Key_2, Qt.ControlModifier);
        tryVerify(function () { return AppController.currentSection === "tasks"; }, 1000);
    }
}
