// The keyboard core of the 0.9 acceptance (APP-221, batch B1): type-to-search
// on the board, the cursor after d, the undated list, bulk delete, saved view
// keys, focus mode, What's new, a checklist's own keys, the notes list, the
// conflict and someday rows of the task menu.
//
// Drives the real Main.qml with real key events.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "B1Keys"
    when: windowShown

    property var win: null
    property var seeded: []

    function initTestCase() {
        const comp = Qt.createComponent("qrc:/qt/qml/TodoCpp/qml/Main.qml");
        tryCompare(comp, "status", Component.Ready, 5000);
        verify(comp.status === Component.Ready, comp.errorString());
        tc.win = comp.createObject(null);
        verify(tc.win !== null);
        tc.win.width = 1456;
        tc.win.height = 939;
        AppController.resetAllShortcuts();
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

    function seed(id, title) {
        const d = AppController.newTaskDraft("todo");
        d._isNew = true;
        d.id = id;
        d.title = title;
        AppController.saveTask(d);
        tc.seeded.push(id);
    }

    function init() {
        tc.seeded = [];
        for (let i = 0; i < 3; i++) seed("BK-" + i, "bkprobe card " + i);
        tc.win.searchText = "";
        closeAll();
        router().cancel();
    }

    function cleanup() {
        closeAll();
        for (let i = 0; i < tc.seeded.length; i++) AppController.deleteTask(tc.seeded[i]);
        tc.seeded = [];
        AppController.clearPendingUndo();
        tc.win.searchText = "";
        const field = byName(tc.win.contentItem, "topbar-search");
        if (field) field.text = "";
        router().cancel();
        tc.win.focusActiveView();
    }

    function typeName(o) {
        const s = String(o);
        const i = s.indexOf("(");
        return i > 0 ? s.slice(0, i) : s;
    }
    function popups() {
        const out = [];
        const cd = tc.win.contentData;
        for (let i = 0; i < cd.length; i++) {
            const o = cd[i];
            if (o && o.opened !== undefined && o.open !== undefined) out.push(o);
        }
        return out;
    }
    function dataByName(name) {
        const cd = tc.win.contentData;
        for (let i = 0; i < cd.length; i++) if (cd[i] && cd[i].objectName === name) return cd[i];
        return null;
    }
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
    function router() { return dataByName("key-router"); }
    function closeAll() {
        const ps = popups();
        for (let i = 0; i < ps.length; i++) if (ps[i].opened) ps[i].close();
        wait(150);
        AppController.clearSelection();
        const b = tc.win.activeViewItem();
        if (b && b.clearCursor) b.clearCursor();
        tc.win.focusActiveView();
        wait(20);
    }
    function boardWithCursor(id) {
        AppController.currentView = "board";
        tryVerify(function () { return tc.win.activeViewItem() && tc.win.activeViewItem().moveCursor; }, 3000);
        tc.win.focusActiveView();
        const b = tc.win.activeViewItem();
        b.cursorTaskId = id;
        b.cursorVisible = true;
        wait(30);
        return b;
    }
    function typeText(s) { for (let i = 0; i < s.length; i++) keyClick(s[i]); }
    function category(id) { return AppController.statusCategory(AppController.taskById(id).status); }

    // A4-3: a word typed on the board searches; its first letter is not run
    // as the command it also is.
    function test_a_word_typed_on_the_board_searches() {
        boardWithCursor("BK-0");
        keyClick(Qt.Key_D);
        keyClick(Qt.Key_E);
        keyClick(Qt.Key_P);
        wait(400);
        compare(category("BK-0"), "todo", "d of \"dep\" marked the card done");
        const field = byName(tc.win.contentItem, "topbar-search");
        compare(field.text, "dep");
        verify(field.activeFocus, "the rest of the word goes on into the field");
    }

    function test_s_of_a_word_opens_no_schedule_field() {
        boardWithCursor("BK-0");
        keyClick(Qt.Key_S);
        keyClick(Qt.Key_T);
        wait(400);
        const field = byName(tc.win.contentItem, "topbar-search");
        compare(field.text, "st");
        const sched = popups().filter(p => p.objectName === "schedule-popup");
        verify(sched.length === 0 || !sched[0].opened, "s opened the schedule field under the word");
    }

    // A command letter alone still runs, a moment later.
    function test_a_lone_command_letter_still_runs() {
        boardWithCursor("BK-0");
        keyClick(Qt.Key_D);
        tryVerify(function () { return category("BK-0") === "done"; }, 1500);
        compare(byName(tc.win.contentItem, "topbar-search").text, "");
    }

    // "dd" is Vim's habit, not a word.
    function test_dd_is_not_a_search() {
        boardWithCursor("BK-0");
        keyClick(Qt.Key_D);
        keyClick(Qt.Key_D);
        tryVerify(function () { return category("BK-0") === "done"; }, 1500);
        compare(byName(tc.win.contentItem, "topbar-search").text, "");
    }

    // A2-4, A4-16: after d the cursor is on the card's neighbour, not on the
    // card that left for the folded Done column.
    function test_d_steps_the_board_cursor_to_the_neighbour() {
        tc.win.typeAheadHoldMs = 0;
        const b = boardWithCursor("BK-1");
        keyClick(Qt.Key_D);
        tryVerify(function () { return category("BK-1") === "done"; }, 1000);
        verify(b.cursorTaskId !== "BK-1" && b.cursorTaskId.length > 0, "cursor " + b.cursorTaskId);
        const before = b.cursorTaskId;
        keyClick(Qt.Key_K);
        keyClick(Qt.Key_J);
        verify(b.cursorTaskId.length > 0);
        tc.win.typeAheadHoldMs = 250;
        verify(before.length > 0);
    }

    // A2-1: the board scrolls after a card the keyboard moved sideways.
    function test_the_board_follows_a_moved_card() {
        tc.win.typeAheadHoldMs = 0;
        tc.win.width = 640;
        wait(300);
        const b = boardWithCursor("BK-0");
        const hs = byName(b, "board-hscroll");
        verify(hs !== null);
        if (hs.contentWidth <= hs.width + 10) {
            tc.win.width = 1456;
            tc.win.typeAheadHoldMs = 250;
            skip("the board fits this window");
        }
        for (let i = 0; i < 3; i++) {
            keyClick(Qt.Key_L, Qt.ShiftModifier);
            wait(200);
        }
        verify(AppController.taskById("BK-0").status !== "todo", "the card moved");
        const card = b._cardItem("BK-0");
        verify(card !== null);
        const p = card.mapToItem(hs, 0, 0);
        verify(p.x >= -1 && p.x + card.width <= hs.width + 1, "card at " + p.x + " in " + hs.width);
        AppController.undo(); AppController.undo(); AppController.undo();
        tc.win.width = 1456;
        tc.win.typeAheadHoldMs = 250;
        wait(200);
    }

    // A1-1, A2-9, A5-17: "N без даты — посмотреть и решить" lands on rows.
    function test_the_undated_list_is_not_folded() {
        tc.win.typeAheadHoldMs = 0;
        tc.win.showQuery("is:undated", "list");
        tryVerify(function () { const v = tc.win.activeViewItem(); return v && v._taskIds && v._taskIds().length >= 3; }, 3000);
        const list = tc.win.activeViewItem();
        compare(list._groupCount, 1);
        verify(!list.isFolded("none:"), "the only group is open");
        tc.win.focusActiveView();
        keyClick(Qt.Key_J);
        verify(list.cursorTaskId.length > 0, "j has a row to walk");

        // A1-3: the cursor's task leaves the list; the next row takes its place.
        const ids = list._taskIds();
        list.cursorTaskId = ids[1];
        list.cursorVisible = true;
        const tomorrow = new Date(AppController.today.getFullYear(), AppController.today.getMonth(), AppController.today.getDate() + 1);
        AppController.rescheduleTask(ids[1], "scheduled", tomorrow, false);
        tryVerify(function () { return list._taskIds().indexOf(ids[1]) < 0; }, 2000);
        compare(list.cursorTaskId, ids[2], "the row that took its place");
        AppController.clearTaskDate(ids[1], "scheduled");
        tc.win.typeAheadHoldMs = 250;
    }

    // A3-16, A5-24: Ctrl A takes every task the filter lets through.
    function test_select_all_in_the_list_takes_folded_rows_too() {
        AppController.currentView = "list";
        tryVerify(function () { const v = tc.win.activeViewItem(); return v && v.selectAllVisible; }, 3000);
        const list = tc.win.activeViewItem();
        wait(100);
        list.selectAllVisible();
        compare(AppController.selectionCount, list.taskCount);
        AppController.clearSelection();
    }

    // A1-28: a bulk delete of 10+ asks once; Enter deletes, Esc cancels; a
    // small local one goes at once.
    function test_bulk_delete_asks_first() {
        tc.win.typeAheadHoldMs = 0;
        const many = [];
        for (let i = 0; i < 11; i++) { seed("BKM-" + i, "bkprobe many " + i); many.push("BKM-" + i); }
        AppController.currentView = "board";
        wait(100);
        tc.win.focusActiveView();
        AppController.setSelectedTaskIds(many);
        keyClick(Qt.Key_Delete);
        const dlg = dataByName("bulk-delete-dialog");
        verify(dlg !== null);
        tryCompare(dlg, "opened", true);
        verify(AppController.taskById("BKM-0").id === "BKM-0", "nothing deleted before the answer");
        verify(dlg.title.indexOf("11") >= 0, dlg.title);
        keyClick(Qt.Key_Escape);
        tryCompare(dlg, "opened", false);
        verify(AppController.taskById("BKM-5").id === "BKM-5", "Esc cancels");

        AppController.setSelectedTaskIds(many);
        tc.win.focusActiveView();
        keyClick(Qt.Key_Delete);
        tryCompare(dlg, "opened", true);
        keyClick(Qt.Key_Return);
        tryCompare(dlg, "opened", false);
        tryVerify(function () { return !AppController.taskById("BKM-0").id; }, 1000, "Enter deletes");
        AppController.undo();
        tryVerify(function () { return AppController.taskById("BKM-0").id === "BKM-0"; }, 1000);

        AppController.setSelectedTaskIds(["BK-0", "BK-1"]);
        tc.win.focusActiveView();
        keyClick(Qt.Key_Delete);
        wait(100);
        verify(!dlg.opened, "two local tasks go at once");
        verify(!AppController.taskById("BK-0").id);
        AppController.undo();
        tc.win.typeAheadHoldMs = 250;
    }

    // A1-27: a saved view's key from Today opens it; only on screen does it
    // turn it off.
    function test_a_saved_view_key_opens_it_from_today() {
        const host = dataByName("saved-views-host");
        verify(host !== null);
        AppController.currentView = "board";
        wait(50);
        const id = AppController.saveView("bk view", host.currentState());
        verify(id.length > 0);
        host.activeId = id;
        AppController.currentView = "today";
        wait(50);
        const n = AppController.savedViews.findIndex(v => v.id === id) + 1;
        host.applyAt(n);
        compare(AppController.currentView, "board", "the key opened the view");
        compare(host.activeId, id);
        host.applyAt(n);
        compare(host.activeId, "", "on screen, pressed again: off");
        AppController.deleteSavedView(id);
    }

    // A1-24: focus mode takes the task under the cursor.
    function test_focus_mode_takes_the_cursor_task() {
        boardWithCursor("BK-2");
        compare(tc.win._immersionTaskId(), "BK-2");
        AppController.setSelectedTaskIds(["BK-0"]);
        compare(tc.win._immersionTaskId(), "BK-0", "one selected task wins over the cursor");
        AppController.clearSelection();
    }

    // A3-5: Esc on What's new hands the keyboard back to the view.
    function test_whats_new_esc_returns_focus() {
        AppController.currentView = "today";
        wait(50);
        const wn = popups().filter(p => p.objectName === "whats-new")[0];
        verify(wn !== undefined);
        if (!wn.available) skip("no What's new notes in this build");
        wn.showNow();
        tryCompare(wn, "opened", true);
        keyClick(Qt.Key_Escape);
        tryCompare(wn, "opened", false);
        tryVerify(function () { return tc.win.activeFocusItem && tc.win.activeFocusItem !== tc.win.contentItem; }, 1000,
                  "focus fell to the window root");
    }

    // A4-15, A1-11: on a checklist row d ticks the item (not the task), j / k
    // walk, Tab moves on, Alt+→ indents.
    function test_checklist_rows_keep_their_keys() {
        tc.win.typeAheadHoldMs = 0;
        AppController.setTaskChecklistText("BK-0", "- [ ] one\n- [ ] two\n- [ ] three");
        AppController.currentView = "board";
        wait(50);
        tc.win.openTask("BK-0");
        let row0 = null;
        tryVerify(function () { row0 = byName(tc.win.contentItem, "cl-item-0"); return row0 !== null && row0.visible; }, 3000);
        row0.forceActiveFocus();
        keyClick(Qt.Key_J);
        tryVerify(function () { return tc.win.activeFocusItem && tc.win.activeFocusItem.objectName === "cl-item-1"; }, 1000,
                  "j walks to the next item");
        keyClick(Qt.Key_K);
        tryVerify(function () { return tc.win.activeFocusItem && tc.win.activeFocusItem.objectName === "cl-item-0"; }, 1000);
        keyClick(Qt.Key_D);
        wait(100);
        compare(category("BK-0"), "todo", "d closed the whole task");
        verify(AppController.taskChecklist("BK-0")[0].done, "d ticked the item");
        keyClick(Qt.Key_Right, Qt.AltModifier);
        wait(50);
        keyClick(Qt.Key_J);
        wait(50);
        keyClick(Qt.Key_Right, Qt.AltModifier);
        wait(50);
        compare(AppController.taskChecklist("BK-0")[1].level, 2, "Alt+→ indents");
        keyClick(Qt.Key_Tab);
        wait(50);
        const f = tc.win.activeFocusItem;
        verify(!f || String(f.objectName).indexOf("cl-item-") !== 0 || f.objectName !== "cl-item-1", "Tab moved on");
        tc.win.typeAheadHoldMs = 250;
    }

    // A4-5: the note list is the view: ? opens the cheat sheet there.
    function test_router_keys_work_on_the_note_list() {
        AppController.currentView = "notes";
        let list = null;
        tryVerify(function () { list = byName(tc.win.contentItem, "note-list"); return list !== null && list.visible; }, 3000);
        list.forceActiveFocus();
        verify(!tc.win._focusOnControl, "the list counted as a stray control");
        keyClick(Qt.Key_G);
        keyClick(Qt.Key_T);
        tryCompare(AppController, "currentView", "today");
    }

    // A3-9: the task menu offers resolving a conflict, only on a conflict.
    function test_task_menu_resolve_row() {
        const host = createTemporaryQmlObject('import TodoCpp; TaskMenuHost { }', tc);
        host.task = { id: "BK-0", title: "x", status: "todo", ticket: { provider: "jira", conflict: true } };
        host.anchorItem = tc;
        const m = host.contextMenu();
        m.popup(tc, 0, 0);
        tryVerify(() => m.opened, 1000);
        let resolve = null;
        for (let i = 0; i < m.count; i++) {
            const it = m.itemAt(i);
            if (it && it.objectName === "tc-menu-resolve") resolve = it;
        }
        verify(resolve !== null && resolve.visible, "Resolve the conflict… is a row");
        host.task = { id: "BK-0", title: "x", status: "todo", ticket: { provider: "jira" } };
        verify(!resolve.visible);
        m.close();
    }

    // A1-15: s takes "когда-нибудь" / "someday"; the command line finds it.
    function test_someday_from_s_and_the_command_line() {
        tc.win.typeAheadHoldMs = 0;
        boardWithCursor("BK-2");
        keyClick(Qt.Key_S);
        const sched = popups().filter(p => p.objectName === "schedule-popup")[0];
        tryCompare(sched, "opened", true);
        typeText("someday");
        keyClick(Qt.Key_Return);
        tryVerify(function () { return AppController.taskById("BK-2").someday === true; }, 1000);
        AppController.undo();
        boardWithCursor("BK-1");
        tc.win.runCommand("task.someday");
        tryVerify(function () { return AppController.taskById("BK-1").someday === true; }, 1000);
        tc.win.typeAheadHoldMs = 250;
    }

    // A1-8: a new meeting over one already there says so.
    function test_event_capture_names_an_overlap() {
        const day = AppController.today;
        const d = AppController.newEventDraft(8, day);
        d.title = "bkprobe deep work";
        d.start = 8; d.end = 11;
        AppController.saveEvent(d);
        const cap = popups().filter(p => p.objectName === "event-capture")[0];
        verify(cap !== undefined);
        cap.openAt({ date: day, start: 14, end: 15 });
        tryCompare(cap, "opened", true);
        const input = find(cap.contentItem, it => it.objectName === "event-capture-input");
        const clash = find(cap.contentItem, it => it.objectName === "event-capture-clash");
        verify(input !== null && clash !== null);
        verify(!clash.visible, "14:00 is free");
        input.text = "bkprobe intro 9:00";
        tryVerify(() => clash.visible, 1000, "9:00 sits on the 8–11 meeting");
        verify(clash.text.indexOf("bkprobe deep work") >= 0, clash.text);
        cap.close();
        AppController.deleteEvent(d.id);
    }

    // A1-20: a date already over is said in the schedule field.
    function test_schedule_says_a_past_date() {
        tc.win.typeAheadHoldMs = 0;
        boardWithCursor("BK-2");
        keyClick(Qt.Key_S);
        const sched = popups().filter(p => p.objectName === "schedule-popup")[0];
        tryCompare(sched, "opened", true);
        const result = find(sched.contentItem, it => it.objectName === "schedule-result");
        typeText("tomorrow");
        tryVerify(() => result.text.length > 0, 500);
        verify(result.text.indexOf(I18n.t("schedule.inPast")) < 0, result.text);
        compare(sched.isPast({ state: "ok", when: new Date(2026, 9, 11, 10, 0), timed: true }, new Date(2026, 9, 12, 9, 0)), true);
        compare(sched.isPast({ state: "ok", when: new Date(2026, 9, 12), timed: false }, new Date(2026, 9, 12, 23, 0)), false,
                "today without a time is not past");
        compare(sched.isPast({ state: "ok", when: new Date(2026, 9, 12, 10, 0), timed: true }, new Date(2026, 9, 12, 9, 0)), false);
        sched.close();
        tc.win.typeAheadHoldMs = 250;
    }

    // A2-28: a capture toast with an Undo gives way to a newer Undo.
    function test_a_newer_undo_replaces_a_capture_toast() {
        const toast = byName(tc.win.contentItem, "toast");
        verify(toast !== null);
        toast.clear();
        toast.showWithActions("Created BK-9", [{ label: "Open", fn: function () {} }, { label: "Undo", fn: function () {} }], 8, "info", 1);
        toast.showUndo("Done: BK-0", "Undo", 5, function () {});
        compare(toast._items.length, 1);
        compare(toast._items[0].message, "Done: BK-0", "the newest action's toast is on screen");
        toast.clear();
    }

    // A2-4, A4-16: d on the last card of a column hands the cursor to the
    // nearest column with cards; it used to stay on the card, now in the
    // folded Done column, and j/k/l did nothing until h.
    function test_d_on_a_lone_card_moves_the_cursor_to_another_column() {
        tc.win.typeAheadHoldMs = 0;
        const sts = AppController.statuses;
        let other = "";
        for (let i = 0; i < sts.length && !other; i++)
            if (sts[i].id !== "todo" && AppController.statusCategory(sts[i].id) !== "done") other = sts[i].id;
        verify(other.length > 0, "a second open column");
        AppController.moveTask("BK-1", other);
        tc.win.searchText = "bkprobe";
        wait(150);
        const b = boardWithCursor("BK-1");
        keyClick(Qt.Key_D);
        tryVerify(function () { return category("BK-1") === "done"; }, 1000);
        verify(b.cursorTaskId === "BK-0" || b.cursorTaskId === "BK-2", "cursor " + b.cursorTaskId);
        const before = b.cursorTaskId;
        keyClick(Qt.Key_J);
        keyClick(Qt.Key_K);
        verify(b.cursorTaskId === "BK-0" || b.cursorTaskId === "BK-2", "j/k walk the column: " + b.cursorTaskId);
        // The pure rule: next, else above, else the nearest column (right
        // first), else nothing.
        compare(b._neighbourOf([{ ids: ["a"] }, { ids: ["x"] }, { ids: ["p", "q"] }], { col: 1, row: 0 }), "p");
        compare(b._neighbourOf([{ ids: ["a"] }, { ids: ["x"] }, { ids: [] }], { col: 1, row: 0 }), "a");
        compare(b._neighbourOf([{ ids: ["x", "y"] }], { col: 0, row: 1 }), "x");
        compare(b._neighbourOf([{ ids: ["x"] }, { ids: [] }], { col: 0, row: 0 }), "");
        tc.win.searchText = "";
        tc.win.typeAheadHoldMs = 250;
        // The next test's d on BK-1 is not this one's "dd".
        tc.win._lastDoneKey = "";
        verify(before.length > 0);
    }

    // A1-18: in the calendar lens the undated tray is a region of its own.
    function test_f6_reaches_the_calendar_tray() {
        AppController.currentView = "week";
        tryVerify(function () { return tc.win._trayItem() !== null; }, 3000, "the tray is found beside the grid");
        verify(tc.win._regions().indexOf("tray") >= 0, String(tc.win._regions()));
        tc.win.focusActiveView();
        let reached = false;
        for (let i = 0; i < 6 && !reached; i++) {
            keyClick(Qt.Key_F6);
            wait(30);
            reached = tc.win._regionOf(tc.win.activeFocusItem) === "tray";
        }
        verify(reached, "F6 never landed in the tray");
        AppController.currentView = "board";
        tryVerify(function () { return tc.win.activeViewItem() && tc.win.activeViewItem().moveCursor; }, 3000);
    }

    // A1-15: "потом", "park", "later" name the someday command, not only its
    // label.
    function test_someday_command_answers_to_its_other_names() {
        const pal = dataByName("command-line");
        verify(pal !== null);
        const e = { kind: "command", label: I18n.t("palette.cmd.someday"), aliases: I18n.t("palette.cmd.someday.words") };
        verify(pal._nameHit("someday", e));
        verify(pal._nameHit("later", e));
        verify(pal._nameHit("отложить", e));
        verify(!pal._nameHit("deploy", e));
    }

    // A4-3 residue: "zap" typed on the board searches; z a does not fold the
    // column first.
    function test_a_word_starting_with_a_sequence_searches() {
        const b = boardWithCursor("BK-0");
        const folded = JSON.stringify(b.collapsed);
        typeText("zap");
        wait(400);
        compare(byName(tc.win.contentItem, "topbar-search").text, "zap");
        compare(JSON.stringify(b.collapsed), folded, "z a folded a column under the word");
    }

    // A4-3 residue: u with nothing to undo is still nobody's key, but a word
    // starting with it searches whole.
    function test_a_word_starting_with_an_idle_u_searches_whole() {
        AppController.clearPendingUndo();
        verify(!AppController.hasPendingUndo);
        boardWithCursor("BK-0");
        typeText("update");
        wait(400);
        compare(byName(tc.win.contentItem, "topbar-search").text, "update");
        tc.win.searchText = "";
        byName(tc.win.contentItem, "topbar-search").text = "";
        tc.win.focusActiveView();
        keyClick(Qt.Key_U);
        wait(400);
        compare(byName(tc.win.contentItem, "topbar-search").text, "", "a lone idle u types nothing");
    }

    // A3-21 / A6-15 follow-up: the card's due line picks its short form by
    // its own box, with no binding loop on "text".
    function test_card_due_line_has_no_binding_loop() {
        failOnWarning(/Binding loop detected/);
        const d = new Date(AppController.today);
        d.setDate(d.getDate() - 19);
        const t = AppController.taskById("BK-0");
        t.dueAt = d;
        t.dueHasTime = false;
        AppController.saveTask(t);
        tc.win.width = 900;
        wait(300);
        tc.win.width = 1456;
        wait(300);
        const b = boardWithCursor("BK-0");
        const card = b._cardItem("BK-0");
        verify(card !== null);
        const due = byName(card, "tc-due");
        verify(due !== null && due.visible);
        verify(due.text.length > 0);
    }
}
