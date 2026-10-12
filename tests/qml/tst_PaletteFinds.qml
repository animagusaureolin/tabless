// What the command line finds (0.9 acceptance, APP-221 B4): the time machine
// by what a panicked user types, a command by its own name even when the words
// read as a date, a word that is only inside notes, text typed in the wrong
// keyboard layout, and another profile's tasks under a header of their own.
//
// Drives the real Main.qml with real key events.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "PaletteFinds"
    when: windowShown

    property var win: null
    property var seeded: []
    property var notes: []

    function initTestCase() {
        const comp = Qt.createComponent("qrc:/qt/qml/TodoCpp/qml/Main.qml");
        tryCompare(comp, "status", Component.Ready, 5000);
        verify(comp.status === Component.Ready, comp.errorString());
        tc.win = comp.createObject(null);
        verify(tc.win !== null);
        tc.win.width = 1456;
        tc.win.height = 939;
        AppController.resetAllShortcuts();
        AppController.language = "en";
        wait(1200);   // splash
        tc.win.requestActivate();
        AppController.currentView = "board";
        tryVerify(function () { return tc.win.activeViewItem() !== null; }, 3000);
        wait(100);
    }

    function cleanupTestCase() {
        if (tc.win) tc.win.destroy();
        tc.win = null;
    }

    function cleanup() {
        closeAll();
        for (let i = 0; i < tc.seeded.length; i++) AppController.deleteTask(tc.seeded[i]);
        for (let j = 0; j < tc.notes.length; j++) AppController.deleteNote(tc.notes[j]);
        tc.seeded = [];
        tc.notes = [];
        AppController.clearPendingUndo();
        AppController.currentView = "board";
    }

    function add(title) {
        const d = AppController.newTaskDraft("todo");
        d._isNew = true;
        d.title = title;
        AppController.saveTask(d);
        tc.seeded.push(d.id);
        return d.id;
    }
    function typeName(o) {
        const s = String(o);
        const i = s.indexOf("(");
        return i > 0 ? s.slice(0, i) : s;
    }
    function popup(prefix) {
        const cd = tc.win.contentData;
        for (let i = 0; i < cd.length; i++) {
            const o = cd[i];
            if (o && o.opened !== undefined && o.open !== undefined && typeName(o).indexOf(prefix) === 0) return o;
        }
        return null;
    }
    function closeAll() {
        const cd = tc.win.contentData;
        for (let i = 0; i < cd.length; i++) {
            const o = cd[i];
            if (o && o.opened === true && o.close !== undefined) o.close();
        }
        wait(150);
        tc.win.focusActiveView();
        wait(20);
    }
    function openWith(text) {
        const cmd = popup("CommandPalette");
        tc.win.requestActivate();
        cmd.openWith(text);
        tryCompare(cmd, "opened", true);
        wait(80);
        return cmd;
    }
    function headers(cmd) {
        return cmd._rows.filter(r => r.header).map(r => r.label);
    }

    // A5-5: восстановить / backup / restore find the time machine.
    function test_recovery_words_find_the_time_machine() {
        for (const w of ["восстановить", "backup", "restore", "снимок", "откатить", "undo"]) {
            const cmd = openWith(w);
            tryVerify(function () { return cmd._matches.some(m => m.commandId === "timeMachine.open"); }, 1000,
                      w + ": " + JSON.stringify(cmd._matches.map(m => m.label)));
            verify(!cmd._matches.some(m => m.kind === "create"), w + " offered a junk task");
            cmd.close();
            tryCompare(cmd, "opened", false);
        }
    }

    // A2-32: "end of day" is the command, not "due today".
    function test_a_command_is_found_by_its_own_name() {
        const cmd = openWith("end of day");
        tryVerify(function () { return cmd._matches.length > 0; }, 1000);
        compare(cmd._matches[0].commandId, "endOfDay.open", JSON.stringify(cmd._matches.map(m => m.label)));
        cmd.close();
    }

    // A4-8: a word only in a note's body offers the notes search, not a task.
    function test_a_word_only_in_notes_offers_the_notes_search() {
        const id = AppController.newNote("b4 palette note");
        tc.notes.push(id);
        AppController.setNoteBody(id, "# b4 palette note\n\nselect zqvbodyword from t");
        const cmd = openWith("zqvbodyword");
        tryVerify(function () { return cmd._matches.some(m => m.action === "searchNotes"); }, 1000,
                  JSON.stringify(cmd._matches.map(m => m.label)));
        compare(cmd._matches[0].action, "searchNotes", "the notes search is what Enter takes");
        keyClick(Qt.Key_Return);
        tryCompare(cmd, "opened", false);
        tryCompare(AppController, "currentView", "notes");
    }

    // A4-20: "gjxbybnm" is "починить" typed in the English layout.
    function test_the_other_layout_is_tried_before_a_task_is_offered() {
        add("починить кран b4");
        const cmd = openWith("gjxbybnm");
        tryVerify(function () { return cmd._matches.some(m => m.kind === "task" && m.label.indexOf("починить") >= 0); }, 1000,
                  JSON.stringify(cmd._matches.map(m => m.label)));
        verify(headers(cmd).some(h => h.indexOf("починить") >= 0), JSON.stringify(headers(cmd)));
        compare(cmd._matches[0].kind, "task");
        cmd.close();
    }

    // A1-16: another profile's task sits under "From other profiles".
    function test_another_profiles_tasks_have_their_own_header() {
        const home = AppController.activeProfileId;
        const other = AppController.createProfile("B4 elsewhere", "");
        verify(other.length > 0);
        const d = AppController.newTaskDraft("todo");
        d._isNew = true;
        d.title = "zqvelsewhere taxes";
        AppController.saveTask(d);
        AppController.activeProfileId = home;
        const cmd = openWith("zqvelsewhere");
        tryVerify(function () { return headers(cmd).indexOf(I18n.t("cmd.group.otherProfiles")) >= 0; }, 1000,
                  JSON.stringify(headers(cmd)));
        verify(headers(cmd).indexOf(I18n.t("cmd.group.other")) < 0, "the task is not a note, doc or person");
        cmd.close();
        AppController.deleteProfile(other);
        AppController.activeProfileId = home;
    }

    // Regression of A4-8: a note found by name and the notes search share one
    // "Notes, docs, people" header.
    function test_note_by_name_and_notes_search_share_one_header() {
        const a = AppController.newNote("zqvsql cheatsheet");
        tc.notes.push(a);
        const b = AppController.newNote("b4 other note");
        tc.notes.push(b);
        AppController.setNoteBody(b, "# b4 other note\n\nrun zqvsql on prod");
        const cmd = openWith("zqvsql");
        tryVerify(function () { return cmd._matches.some(m => m.action === "searchNotes"); }, 1000,
                  JSON.stringify(cmd._rows.map(r => r.header ? "[" + r.label + "]" : (r.action || r.kind))));
        const other = headers(cmd).filter(h => h === I18n.t("cmd.group.other"));
        compare(other.length, 1, JSON.stringify(headers(cmd)));
        cmd.close();
    }

    function savedHost() {
        const find = function (it) {
            if (!it) return null;
            if (it.objectName === "saved-views-host") return it;
            const kids = it.children || [];
            for (let i = 0; i < kids.length; i++) { const r = find(kids[i]); if (r) return r; }
            return null;
        };
        return find(tc.win.contentItem);
    }
    function dropViews(before) {
        const list = AppController.savedViews;
        for (let i = 0; i < list.length; i++)
            if (before.indexOf(list[i].id) < 0) AppController.deleteSavedView(list[i].id);
    }

    // A3-15: Ctrl S from the line on the board saves what the line says,
    // stays on the board and leaves the board's filter alone — cancelled or
    // saved.
    function test_save_from_the_line_keeps_the_board_and_its_filter() {
        const before = AppController.savedViews.map(v => v.id);
        AppController.currentView = "board";
        wait(100);
        tc.win.searchText = "status:blocked p0";
        let cmd = openWith("p1 ");
        tryVerify(function () { return cmd._parse.query.length > 0; }, 1000);
        const q = cmd._parse.query;
        keyClick(Qt.Key_S, Qt.ControlModifier);
        tryCompare(cmd, "opened", false);
        const dlg = savedHost().nameDialog;
        tryCompare(dlg, "opened", true);
        compare(dlg.query, q, "the dialog says what will be saved");
        compare(tc.win.searchText, "status:blocked p0", "opening the dialog changed the filter");
        keyClick(Qt.Key_Escape);
        tryCompare(dlg, "opened", false);
        compare(tc.win.searchText, "status:blocked p0", "Esc lost the filter");
        compare(AppController.savedViews.length, before.length, "Esc saved a view");

        cmd = openWith("p1 ");
        tryVerify(function () { return cmd._parse.query.length > 0; }, 1000);
        keyClick(Qt.Key_S, Qt.ControlModifier);
        tryCompare(dlg, "opened", true);
        keyClick(Qt.Key_Return);
        tryCompare(dlg, "opened", false);
        compare(AppController.savedViews.length, before.length + 1);
        const v = AppController.savedViews[AppController.savedViews.length - 1];
        compare(v.query, q);
        compare(v.view, "board");
        compare(AppController.currentView, "board");
        compare(tc.win.searchText, "status:blocked p0", "saving replaced the filter");
        dropViews(before);
        tc.win.searchText = "";
    }

    // A6-24: the save dialog names the keys a view really gets, not Alt.
    function test_save_dialog_names_the_real_keys() {
        const dlg = savedHost().nameDialog;
        dlg.openFor("save", "x", "p0", 1);
        tryCompare(dlg, "opened", true);
        verify(dlg.fact.indexOf("Alt") < 0, dlg.fact);
        verify(dlg.fact.indexOf(AppController.shortcutText("savedView.1")) >= 0, dlg.fact);
        verify(dlg.fact.indexOf(AppController.shortcutText("savedView.1.alt")) >= 0, dlg.fact);
        dlg.close();
        tryCompare(dlg, "opened", false);
    }

    // A6-24: a view's key on a view whose filters were changed since brings
    // it back; only on the view as it was does it turn it off.
    function test_view_key_restores_a_changed_view() {
        const before = AppController.savedViews.map(v => v.id);
        AppController.currentView = "list";
        wait(100);
        const host = savedHost();
        const id = AppController.saveView("b4 key view", { query: "is:open priority:p0", priorities: [], sort: "manual",
                                                           archived: false, showDone: false, view: "list" });
        verify(id.length > 0);
        const n = AppController.savedViews.map(v => v.id).indexOf(id) + 1;
        verify(host.apply(id));
        compare(tc.win.searchText, "is:open priority:p0");
        tc.win.searchText = "is:open";
        verify(host.modified);
        host.applyAt(n);
        compare(tc.win.searchText, "is:open priority:p0", "the key cleared a changed view instead of restoring it");
        compare(host.activeId, id);
        host.applyAt(n);
        compare(host.activeId, "", "pressed again on the view as it is, it turns off");
        dropViews(before);
        tc.win.searchText = "";
    }
}
