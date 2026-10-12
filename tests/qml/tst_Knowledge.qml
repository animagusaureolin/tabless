// Knowledge (APP-269): Docs and Notes in one screen — the reference links,
// pages and snippets in the list beside the notes, the note edited where it
// is drawn, and the side with the tasks in the note and what links here.
import QtQuick
import QtTest
import TodoCpp

TestCase {
    id: tc
    name: "Knowledge"
    when: windowShown
    visible: true
    width: 1300
    height: 800

    Item { id: host; anchors.fill: parent }

    property string _docs: ""
    function initTestCase() {
        _docs = AppController.docsState;
        AppController.docsState = JSON.stringify({
            sections: [{ id: "s", title: "Ref", items: [{ ref: "RFC 9110", title: "HTTP Semantics", url: "https://www.rfc-editor.org/rfc/rfc9110", pinned: true }] }],
            snippets: [{ title: "Run the stand", lang: "sh", code: "make up" }],
            contacts: []
        });
    }
    function cleanupTestCase() { AppController.docsState = tc._docs; }

    function make() {
        const nv = createTemporaryQmlObject('import TodoCpp; NotesView { anchors.fill: parent }', host);
        verify(nv !== null);
        return nv;
    }
    function docsOf() {
        const nv = make();
        nv.mode = "docs";
        return nv;
    }
    function rowsOf(nv, kind) {
        const list = findChild(nv, "notes-list-pane");
        list.rebuildNow();
        return list.rows.filter((r) => r.kind === kind);
    }

    function test_docs_list_the_catalogue() {
        const nv = docsOf();
        const refs = rowsOf(nv, "ref");
        compare(refs.length, 1);
        compare(refs[0].tag, "RFC");
        verify(refs[0].title.indexOf("9110") >= 0);
        compare(rowsOf(nv, "snippet").length, 1);
        // The search finds a reference by its words.
        findChild(nv, "notes-list-pane").filter = "semantics";
        compare(rowsOf(nv, "ref").length, 1);
        compare(rowsOf(nv, "snippet").length, 0);
        findChild(nv, "notes-list-pane").filter = "";
    }

    function test_no_modes_the_note_is_edited_where_it_is_drawn() {
        const nv = make();
        compare(nv.viewMode, "live");
        const live = findChild(nv, "notes-live");
        verify(live.visible);
        verify(!findChild(nv, "notesEditor").visible);
        // The whole source is one switch away and back.
        nv.viewMode = "edit";
        verify(findChild(nv, "notesEditor").visible);
        nv.viewMode = "live";
    }

    function test_a_task_linking_the_note_is_a_backlink() {
        const noteId = AppController.newNote("Knowledge backlink");
        const draft = AppController.quickTaskDraft("linked task", new Date());
        draft.desc = "see [[Knowledge backlink]]";
        AppController.saveTask(draft);
        const nv = make();
        nv.showBacklinks = true;
        nv._refreshLinks();
        verify(nv._taskLinks.some((t) => t.id === draft.id), "the task is not in 'Linked here'");
        // And the note names the task back: "Mentioned in" on the task.
        AppController.setNoteBody(noteId, "about " + draft.id);
        verify(AppController.notesMentioningTask(draft.id).some((n) => n.id === noteId));
        compare(AppController.noteTaskRefs("about " + draft.id)[0].id, draft.id);
        AppController.deleteTask(draft.id);
        AppController.deleteNote(noteId);
    }

    // "+" in Docs offers a page, a snippet, a link and a contact.
    function test_docs_plus_offers_page_snippet_link_contact() {
        const nv = docsOf();
        for (const k of ["page", "snippet", "ref", "contact"])
            verify(findChild(nv, "knowledge-new-" + k) !== null, k);
        compare(findChild(nv, "knowledge-title").text, I18n.t("siderail.docs"));
        nv.mode = "notes";
        compare(findChild(nv, "knowledge-title").text, I18n.t("siderail.notes"));
    }

    // Docs with no page open says so; Notes draws the note.
    function test_docs_without_a_page_shows_the_empty_line() {
        const nv = docsOf();
        verify(findChild(nv, "docs-no-page").visible);
        verify(!findChild(nv, "notes-live").visible);
        nv.mode = "notes";
        verify(!findChild(nv, "docs-no-page").visible);
        verify(findChild(nv, "notes-live").visible);
    }

    // Each screen keeps what it had open.
    function test_docs_keeps_its_page_across_notes() {
        const nv = docsOf();
        const id = AppController.newDocPage("Probe page keep");
        nv.openPage(id);
        nv.mode = "notes";
        compare(nv.pageId, "");
        nv.mode = "docs";
        compare(nv.pageId, id);
        AppController.deleteDocPage(id);
    }

    function test_links_and_contacts_are_added_edited_and_deleted() {
        const nv = docsOf();
        const pane = findChild(nv, "notes-list-pane");
        pane.saveRef(-1, -1, "Probe link", "RFC 1", "https://example.org/probe");
        let r = rowsOf(nv, "ref").filter((x) => x.name === "Probe link");
        compare(r.length, 1);
        pane.saveRef(r[0].sec, r[0].item, "Probe link", "RFC 2", "https://example.org/probe2");
        r = rowsOf(nv, "ref").filter((x) => x.name === "Probe link");
        compare(r[0].url, "https://example.org/probe2");
        pane.deleteRef(r[0].sec, r[0].item);
        compare(rowsOf(nv, "ref").filter((x) => x.name === "Probe link").length, 0);
        compare(rowsOf(nv, "ref").length, 1, "the seeded link stays");

        pane.saveContact(-1, "Probe person", "qa", "@probe");
        let c = rowsOf(nv, "contact").filter((x) => x.name === "Probe person");
        compare(c.length, 1);
        compare(c[0].handle, "@probe");
        pane.saveContact(c[0].idx, "Probe person", "lead", "@probe2");
        c = rowsOf(nv, "contact").filter((x) => x.name === "Probe person");
        compare(c[0].handle, "@probe2");
        pane.deleteContact(c[0].idx);
        compare(rowsOf(nv, "contact").filter((x) => x.name === "Probe person").length, 0);
    }

    function test_a_snippet_is_added_edited_and_deleted() {
        const nv = docsOf();
        const pane = findChild(nv, "notes-list-pane");
        pane.saveSnippet(-1, "Probe snippet", "sql", "select 1");
        let sn = rowsOf(nv, "snippet").filter((r) => r.title === "Probe snippet");
        compare(sn.length, 1);
        compare(sn[0].code, "select 1");
        pane.saveSnippet(sn[0].idx, "Probe snippet", "sql", "select 2");
        sn = rowsOf(nv, "snippet").filter((r) => r.title === "Probe snippet");
        compare(sn.length, 1);
        compare(sn[0].code, "select 2");
        // The seeded snippet and the reference are left as they were.
        compare(rowsOf(nv, "snippet").filter((r) => r.title === "Run the stand").length, 1);
        compare(rowsOf(nv, "ref").length, 1);
        pane.deleteSnippet(sn[0].idx);
        compare(rowsOf(nv, "snippet").filter((r) => r.title === "Probe snippet").length, 0);
        compare(rowsOf(nv, "snippet").length, 1);
    }

    // With no catalogue yet, the first snippet makes one.
    function test_first_snippet_into_an_empty_catalogue() {
        const keep = AppController.docsState;
        AppController.docsState = "";
        const nv = docsOf();
        findChild(nv, "notes-list-pane").saveSnippet(-1, "First", "", "echo hi");
        compare(rowsOf(nv, "snippet").length, 1);
        AppController.docsState = keep;
    }


    // APP-290: a snippet or a link opens in the document to be read — the
    // list only copied the code or opened the address.
    function test_a_snippet_shows_its_code_in_the_document() {
        const nv = docsOf();
        const list = findChild(nv, "notes-list-pane");
        const sn = rowsOf(nv, "snippet")[0];
        list.itemShown(sn);
        const view = findChild(nv, "docs-item-view");
        verify(view.visible, "the snippet is not shown");
        verify(!findChild(nv, "docs-no-page").visible);
        compare(findChild(nv, "docs-item-text").text, "make up");
        compare(list.shownKey, "snippet:" + sn.idx);
        verify(!findChild(nv, "docs-item-open").visible, "a snippet has nothing to open");
        // A page takes the document back.
        nv.openPage("");
        verify(!view.visible);
    }

    function test_a_link_shows_its_address_and_follows_its_edit() {
        const nv = docsOf();
        const keep = AppController.docsState;
        const list = findChild(nv, "notes-list-pane");
        list.itemShown(rowsOf(nv, "ref")[0]);
        verify(findChild(nv, "docs-item-view").visible);
        verify(findChild(nv, "docs-item-open").visible);
        compare(findChild(nv, "docs-item-title").text, "HTTP Semantics");
        // Edited elsewhere: the pane reads the new text; gone: it clears.
        const d = JSON.parse(keep);
        d.sections[0].items[0].desc = "the semantics of HTTP";
        AppController.docsState = JSON.stringify(d);
        compare(findChild(nv, "docs-item-text").text, "the semantics of HTTP");
        d.sections[0].items = [];
        AppController.docsState = JSON.stringify(d);
        verify(!findChild(nv, "docs-item-view").visible);
        verify(findChild(nv, "docs-no-page").visible);
        AppController.docsState = keep;
    }
}
