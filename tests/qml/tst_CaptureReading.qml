// Quick capture reads the line before Enter, and says so (APP-221 batch B2):
// a repeat has its chip (A2-21), the end of a range is in the "when" chip, and
// "also a meeting" answers to Ctrl+M, the hint line naming it and Tab
// (A2-14).
import QtQuick
import QtQuick.Controls
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "CaptureReading"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    Item { id: host; anchors.fill: parent }

    function open() {
        const qc = createTemporaryQmlObject('import TodoCpp; QuickCapturePopup { }', host);
        verify(qc !== null);
        qc.open();
        tryVerify(() => qc.opened);
        const input = findChild(qc, "qc-input");
        input.forceActiveFocus();
        return qc;
    }
    function type(qc, text) {
        const input = findChild(qc, "qc-input");
        input.text = text;
        input.cursorPosition = text.length;
        qc._refreshPreview();
    }
    function hints(qc) {
        const out = [];
        (function walk(it) {
            if (!it) return;
            if (it.objectName === "qc-keys-hint") {
                for (const c of it.children) if (c.text !== undefined) out.push(c.text);
                return;
            }
            for (const k of it.children || []) walk(k);
        })(qc.contentItem);
        return out;
    }

    function test_a_repeat_has_its_chip_before_enter() {
        const qc = open();
        type(qc, "Отчёт каждый понедельник");
        const chip = findChild(qc.contentItem, "qc-repeat");
        verify(chip !== null);
        verify(chip.visible, "no repeat chip");
        verify(chip.value.length > 0);
        type(qc, "Отчёт в понедельник");
        verify(!chip.visible, "a one-off is not a repeat");
        qc.close();
    }

    function test_the_when_chip_keeps_the_end_of_a_range() {
        const qc = open();
        type(qc, "release tomorrow 23:00-01:00");
        const chip = findChild(qc.contentItem, "qc-when");
        verify(chip.visible);
        verify(chip.value.indexOf("–") >= 0, "the end is dropped: " + chip.value);
        qc.close();
    }

    function test_ctrl_m_makes_it_a_meeting() {
        const qc = open();
        type(qc, "call with anna capturereading tomorrow 9:00-10:00");
        const box = findChild(qc.contentItem, "qc-meeting");
        verify(box.visible);
        verify(!box.checked);
        verify(hints(qc).indexOf(I18n.t("capture.hint.meeting")) >= 0, "the hint line does not name Ctrl M");
        verify(hints(qc).indexOf(I18n.t("capture.hint.tab")) >= 0, "the hint line does not name Tab");
        keyClick(Qt.Key_M, Qt.ControlModifier);
        verify(box.checked, "Ctrl M did not tick the meeting");
        keyClick(Qt.Key_M, Qt.ControlModifier);
        verify(!box.checked, "Ctrl M did not untick it");
        compare(findChild(qc, "qc-input").text, "call with anna capturereading tomorrow 9:00-10:00", "Ctrl M typed into the line");
        // With no time there is no meeting: Ctrl M says why.
        type(qc, "capturereading no time");
        keyClick(Qt.Key_M, Qt.ControlModifier);
        verify(!box.visible);
        compare(qc._hint, I18n.t("capture.meeting.needTime"));
        qc.close();
    }

    function test_ctrl_m_then_enter_books_the_meeting() {
        const qc = open();
        const spy = createTemporaryQmlObject('import QtTest; SignalSpy { signalName: "captured" }', host);
        spy.target = qc;
        type(qc, "sync capturereading tomorrow 9:00-10:00");
        keyClick(Qt.Key_M, Qt.ControlModifier);
        keyClick(Qt.Key_Return);
        tryVerify(() => spy.count === 1);
        const title = String(spy.signalArguments[0][0]);
        const taskId = String(spy.signalArguments[0][2]);
        verify(title.indexOf(I18n.t("quick.done.meeting.sync")) === 0 || title.indexOf(I18n.t("quick.done.meeting.none")) === 0,
               "no meeting was booked: " + title);
        AppController.deleteTask(taskId);
    }
}
