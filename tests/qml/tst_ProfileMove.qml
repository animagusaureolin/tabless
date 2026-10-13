// Profiles in 0.9 (A5-14, A5-21, A1-17), through the real Main.qml: a task
// moved to another profile from the picker, with one undo that works from
// either profile; the picker reached from the selection bar and the command
// key; "+Profile name" in quick capture and in the Ctrl N editor; Today with
// every profile's tasks, another profile's row opening in its own profile.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "ProfileMove"
    when: windowShown

    property var win: null
    property string pA: ""
    property var made: []
    property string settingsBefore: ""

    function initTestCase() {
        const comp = Qt.createComponent("qrc:/qt/qml/TodoCpp/qml/Main.qml");
        tryCompare(comp, "status", Component.Ready, 5000);
        tc.win = comp.createObject(null);
        verify(tc.win !== null);
        tc.win.width = 1400;
        tc.win.height = 900;
        tc.win.show();
        wait(1200);
        tc.pA = AppController.activeProfileId;
        tc.settingsBefore = AppController.appSettingsJson;
    }
    function cleanupTestCase() {
        if (tc.win) tc.win.destroy();
    }
    function cleanup() {
        const dlg = findChild(tc.win, "profile-move-dialog");
        if (dlg && dlg.opened) dlg.close();
        const ed = findChild(tc.win, "task-editor");
        if (ed && ed.opened) ed.close();
        const doc = findChild(tc.win, "task-doc");
        if (doc && doc.opened) doc.close();
        AppController.appSettingsJson = tc.settingsBefore;
        AppController.activeProfileId = tc.pA;
        AppController.clearSelection();
        for (let i = 0; i < tc.made.length; i++) AppController.deleteProfile(tc.made[i]);
        tc.made = [];
        AppController.clearPendingUndo();
    }

    function mk(title) {
        const d = AppController.newTaskDraft("todo");
        d._isNew = true;
        d.title = title;
        verify(AppController.saveTask(d), "seed save");
        return d.id;
    }
    // A second profile, the window back on the first.
    function other(name) {
        const id = AppController.createProfile(name + " " + Date.now(), "");
        verify(id.length > 0);
        tc.made.push(id);
        AppController.activeProfileId = tc.pA;
        return id;
    }
    function nameOf(id) {
        return AppController.profileById(id).name;
    }
    function inProfile(profileId, taskId) {
        const keep = AppController.activeProfileId;
        AppController.activeProfileId = profileId;
        const found = !!AppController.taskById(taskId).id;
        AppController.activeProfileId = keep;
        return found;
    }

    function test_picker_moves_and_one_undo_from_the_target_brings_it_back() {
        const pB = tc.other("Move target");
        const id = tc.mk("move me " + Date.now());
        tc.win.moveToProfile([id]);
        const dlg = findChild(tc.win, "profile-move-dialog");
        tryVerify(() => dlg.opened);
        // Type to filter, Enter moves.
        const field = findChild(dlg, "profile-move-filter");
        field.forceActiveFocus();
        keyClick(Qt.Key_M);
        keyClick(Qt.Key_O);
        keyClick(Qt.Key_V);
        compare(dlg.choices.length >= 1, true);
        compare(dlg.choices[dlg.current].id, pB);
        keyClick(Qt.Key_Return);
        tryVerify(() => !dlg.opened);
        verify(!AppController.taskById(id).id, "the task left the source");
        verify(tc.inProfile(pB, id), "the task is in the target");
        // u in the target takes it back.
        AppController.activeProfileId = pB;
        verify(AppController.hasPendingUndo);
        AppController.undo();
        verify(!AppController.taskById(id).id);
        AppController.activeProfileId = tc.pA;
        verify(!!AppController.taskById(id).id, "back in the source");
        AppController.deleteTask(id);
    }

    function test_the_selection_bar_and_the_key_open_the_picker_for_the_selection() {
        tc.other("Bar target");
        const a = tc.mk("bar one " + Date.now());
        const b = tc.mk("bar two " + Date.now());
        AppController.openSection("tasks");
        AppController.toggleTaskSelection(a);
        AppController.toggleTaskSelection(b);
        const action = findChild(tc.win, "sel-profile");
        verify(action !== null);
        tryVerify(() => action.visible);
        tc.win.runCommand("task.moveProfile");
        const dlg = findChild(tc.win, "profile-move-dialog");
        tryVerify(() => dlg.opened);
        compare(dlg.taskIds.length, 2);
        verify(dlg.fact.indexOf("2") >= 0, dlg.fact);
        dlg.close();
        tryVerify(() => !dlg.opened);
        // Shift M, the same.
        tc.win.runShortcut("task.moveProfile");
        tryVerify(() => dlg.opened);
        compare(dlg.taskIds.length, 2);
        dlg.close();
        AppController.clearSelection();
        AppController.deleteTask(a);
        AppController.deleteTask(b);
    }

    function test_one_profile_says_why() {
        // Only meaningful with a single profile: then the picker never opens.
        if ((AppController.profiles || []).length > 1) skip("the test profile has several profiles");
        const id = tc.mk("lonely " + Date.now());
        tc.win.moveToProfile([id]);
        verify(!findChild(tc.win, "profile-move-dialog").opened);
        AppController.deleteTask(id);
    }

    function test_quick_capture_plus_name_saves_into_that_profile() {
        const pB = tc.other("Side project");
        const title = "sigil capture " + Date.now();
        tc.win.runCommand("quick-capture");
        const input = findChild(tc.win, "qc-input");
        tryVerify(() => input && input.visible);
        input.forceActiveFocus();
        input.text = title + " +" + tc.nameOf(pB).toLowerCase();
        input.cursorPosition = input.text.length;
        wait(150);
        const chip = findChild(tc.win, "qc-profile-chip");
        verify(chip.visible, "the profile shows as a chip");
        compare(chip.value, tc.nameOf(pB));
        keyClick(Qt.Key_Return, Qt.ControlModifier);
        tryCompare(AppController, "activeProfileId", pB);
        let found = "";
        const m = AppController.tasks;
        for (let i = 0; i < m.rowCount(); i++) {
            const t = AppController.taskById(String(m.data(m.index(i, 0), Qt.UserRole + 1)));
            if (t.title === title) found = t.id;
        }
        verify(found.length > 0, "the task is in the named profile, its title without the sigil");
    }

    function test_the_editor_names_its_profile_and_takes_another() {
        const pB = tc.other("Editor target");
        tc.win.newTask("");
        const ed = findChild(tc.win, "task-editor");
        tryVerify(() => ed.opened);
        const label = findChild(ed, "te-profile-name");
        verify(label.text.indexOf(tc.nameOf(tc.pA)) >= 0, label.text);
        const title = "editor sigil " + Date.now();
        findChild(ed, "te-title").text = title + " +" + tc.nameOf(pB);
        ed._refreshTitleChips();
        verify(label.text.indexOf(tc.nameOf(pB)) >= 0, "the header follows the +name: " + label.text);
        verify(ed._save(), "saved");
        tryCompare(AppController, "activeProfileId", pB);
        let found = false;
        const m = AppController.tasks;
        for (let i = 0; i < m.rowCount(); i++) {
            const t = AppController.taskById(String(m.data(m.index(i, 0), Qt.UserRole + 1)));
            if (t.title === title) found = true;
        }
        verify(found, "saved in the named profile under the title without the sigil");
    }

    function test_today_with_every_profile_labels_and_opens_in_place() {
        const pB = tc.other("Today other");
        AppController.activeProfileId = pB;
        const d = AppController.newTaskDraft("todo");
        d._isNew = true;
        d.title = "other profile today " + Date.now();
        const now = new Date();
        d.scheduledAt = new Date(now.getFullYear(), now.getMonth(), now.getDate());
        d.scheduledHasTime = false;
        verify(AppController.saveTask(d));
        AppController.activeProfileId = tc.pA;

        const s = JSON.parse(AppController.appSettingsJson || "{}");
        s.calendar = Object.assign({}, s.calendar || {}, { todayScope: "all" });
        AppController.appSettingsJson = JSON.stringify(s);
        AppController.selectedDate = new Date();
        AppController.openSection("today");
        const today = findChild(tc.win, "today-view");
        tryVerify(() => today && today.visible);
        tryVerify(() => today.allProfiles);
        const rows = (today.dayData.dayOnly || []).filter(r => r.id === d.id);
        compare(rows.length, 1, "another profile's task is on Today");
        compare(rows[0].profileId, pB);
        compare(rows[0].profileName, tc.nameOf(pB));
        verify(today._sub("task", rows[0]).indexOf(tc.nameOf(pB)) >= 0, "the row names its profile");
        // Read-only here: the task keys do not reach it.
        today.cursorVisible = true;
        today.cursorKey = "task:" + d.id;
        compare(today.cursorTaskId, "");
        compare(today._actionCardId(), "");
        // Enter opens it in its own profile.
        today.openCursor();
        tryCompare(AppController, "activeProfileId", pB);
        const doc = findChild(tc.win, "task-doc");
        tryVerify(() => doc.opened && doc.taskId === d.id);
    }
    function collect(item, name, out) {
        if (!item) return out;
        if (item.objectName === name) out.push(item);
        const kids = item.children || [];
        for (let i = 0; i < kids.length; i++) collect(kids[i], name, out);
        return out;
    }
    function test_today_in_progress_names_another_profiles_task() {
        const pB = tc.other("Progress other");
        AppController.activeProfileId = pB;
        const d = AppController.newTaskDraft("prog");
        d._isNew = true;
        d.title = "other profile in progress " + Date.now();
        verify(AppController.saveTask(d));
        AppController.activeProfileId = tc.pA;

        const s = JSON.parse(AppController.appSettingsJson || "{}");
        s.calendar = Object.assign({}, s.calendar || {}, { todayScope: "all" });
        AppController.appSettingsJson = JSON.stringify(s);
        AppController.selectedDate = new Date();
        AppController.openSection("today");
        const today = findChild(tc.win, "today-view");
        tryVerify(() => today && today.visible && today.allProfiles);
        const rows = (today.dayData.inProgress || []).filter(r => r.id === d.id);
        compare(rows.length, 1, "another profile's task is in progress on Today");
        const name = tc.nameOf(pB);
        tryVerify(() => {
            const labels = tc.collect(today, "today-inprogress-profile", []).concat(tc.collect(today, "today-lead-profile", []));
            return labels.some(l => l.visible && l.text === name);
        }, 3000, "the in-progress row names its profile");
        // This profile's rows carry no name.
        const own = tc.collect(today, "today-inprogress-profile", []).filter(l => l.visible && l.text.length === 0);
        compare(own.length, 0);
    }
}
