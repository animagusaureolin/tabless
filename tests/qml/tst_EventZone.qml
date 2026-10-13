// A6-7: the event editor's "Time zone" row is a picker. Local time by
// default, recent zones first, typing searches; the zone picked keeps the
// meeting on that zone's clock, and the row says the start there. The When
// field reads a zone typed with the time.
import QtQuick
import QtQuick.Controls
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "EventZone"
    when: windowShown
    visible: true
    width: 600
    height: 700

    Item { id: host; anchors.fill: parent }

    readonly property string ny: "America/New_York"

    // A day far ahead, emptied of what earlier runs left (the test profile
    // persists).
    function probeDay() {
        const day = new Date();
        day.setDate(day.getDate() + 523);
        day.setHours(0, 0, 0, 0);
        const evs = AppController.events;
        for (let i = evs.rowCount() - 1; i >= 0; i--) {
            const id = String(evs.data(evs.index(i, 0), Qt.UserRole + 1));
            const t = String(AppController.eventById(id).title || "");
            if (t.indexOf("zone probe") === 0) AppController.deleteEvent(id);
        }
        return day;
    }
    function editor() {
        const ed = createTemporaryQmlObject('import TodoCpp; EventEditor { width: 460; height: 640 }', host);
        verify(ed !== null);
        return ed;
    }
    function addMeeting(day, title, start) {
        const d = AppController.newEventDraft(start, day);
        d.title = title;
        d.date = day;
        d.start = start;
        d.end = start + 1;
        AppController.saveEvent(d);
        return d.id;
    }

    function test_a_meeting_is_local_until_a_zone_is_picked() {
        const day = probeDay();
        const id = addMeeting(day, "zone probe local", 15);
        const ed = editor();
        ed.showForId(id);
        const row = findChild(ed, "event-zone");
        verify(row !== null);
        verify(row.visible, "the zone row is always there for a timed meeting");
        compare(row.text, I18n.t("event.zone.local"));

        // Enter on the row opens the picker; local time is first.
        row.forceActiveFocus();
        keyClick(Qt.Key_Return);
        const picker = findChild(ed, "zone-picker");
        tryVerify(() => picker.opened, 1000);
        compare(picker.rows[0].id, "", "local time first");
        // Typing searches.
        const input = findChild(picker, "zone-input");
        tryVerify(() => input.activeFocus, 1000);
        keyClick("n"); keyClick("e"); keyClick("w"); keyClick(Qt.Key_Space); keyClick("y");
        tryVerify(() => picker.rows.length > 0 && picker.rows[0].id === tc.ny, 1000, JSON.stringify(picker.rows[0]));
        keyClick(Qt.Key_Return);
        tryVerify(() => !picker.opened, 1000);

        // Kept in New York; the moment did not move.
        compare(ed.tz, tc.ny);
        const shown = AppController.eventById(id);
        compare(shown.tz, tc.ny);
        compare(Number(shown.start), 15, "the same moment on the viewer's clock");
        const stored = AppController.eventSeriesMaster(id);
        compare(stored.tz, tc.ny);
        fuzzyCompare(Number(stored.start), AppController.zoneHour(day, 15, tc.ny), 1e-6);
        // The row says the start there, unless New York is the viewer's zone.
        if (!AppController.isLocalZone(tc.ny))
            verify(row.text.indexOf("New York") >= 0 && row.text.indexOf(Theme.fmtHour(Number(stored.start))) === 0, row.text);

        // Recent zones come first next time.
        row.forceActiveFocus();
        keyClick(Qt.Key_Return);
        tryVerify(() => picker.opened, 1000);
        compare(picker.rows[0].id, "");
        compare(picker.rows[1].id, tc.ny, "the zone a meeting is kept in is offered");
        compare(picker.index, 1, "the zone set now is under the cursor");
        // Esc leaves it as it was.
        keyClick(Qt.Key_Escape);
        tryVerify(() => !picker.opened, 1000);
        compare(AppController.eventSeriesMaster(id).tz, tc.ny);
        verify(ed.opened, "Esc closes the picker, not the meeting");
        tryVerify(() => row.activeFocus, 1000, "focus is back on the zone row");

        // Back to local time: floats at the hour on screen.
        ed.setZone("");
        compare(AppController.eventSeriesMaster(id).tz, "");
        compare(Number(AppController.eventSeriesMaster(id).start), 15);
        ed.close();
        AppController.deleteEvent(id);
    }

    function test_a_search_with_no_zone_says_so() {
        const ed = editor();
        const day = probeDay();
        const id = addMeeting(day, "zone probe none", 10);
        ed.showForId(id);
        ed.openZonePicker();
        const picker = findChild(ed, "zone-picker");
        tryVerify(() => picker.opened, 1000);
        picker.query = "qwzx";
        compare(picker.rows.length, 0);
        verify(findChild(picker, "zone-none").visible);
        picker.query = "UTC+5";
        verify(picker.rows.length > 0);
        compare(picker.rows[0].offset, "UTC+5");
        picker.close();
        ed.close();
        AppController.deleteEvent(id);
    }

    // "10:00 New York" in When: the hour is New York's, the meeting is kept
    // there.
    function test_when_reads_a_zone_said_with_the_time() {
        const ed = editor();
        const day = probeDay();
        const id = addMeeting(day, "zone probe when", 9);
        ed.showForId(id);
        const err = ed.applyWhen("10:00 New York");
        compare(err, "");
        compare(ed.tz, tc.ny);
        const stored = AppController.eventSeriesMaster(id);
        compare(stored.tz, tc.ny);
        fuzzyCompare(Number(stored.start), 10, 1e-6);
        ed.close();
        AppController.deleteEvent(id);
    }
}
