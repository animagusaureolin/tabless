// The meeting's time zone (A6-7): a small field under the editor's "Time
// zone" row, the way s opens one under a card. Local time first, then the
// zones the user's meetings are kept in; typing searches ("new york",
// "москва", "EST", "UTC+5"). Each row says what the meeting's start is
// there. Enter sets it, Esc leaves it as it was. The meeting does not move:
// the same moment, now kept on that zone's clock.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TodoCpp

Popup {
    PopupArrival { popup: root; dialog: false }
    id: root
    objectName: "zone-picker"
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Theme.spLg
    width: Theme.px(380)

    // The zone set now ("" is local), and the meeting's start on the
    // viewer's clock, for the hour each row shows.
    property string current: ""
    property var date: new Date()
    property real hour: 9
    signal picked(string tz)

    property string query: ""
    property int index: 0

    function openFor(tz, date, hour) {
        root.current = tz || "";
        root.date = date;
        root.hour = hour;
        input.text = "";
        root.query = "";
        root.index = 0;
        open();
        input.forceActiveFocus();
        // The zone set now is under the cursor, so Enter keeps it.
        for (let i = 0; i < root.rows.length; i++)
            if (root.rows[i].id === root.current) { root.index = i; break; }
    }

    // Local time is always offered while nothing is typed, or when what is
    // typed is its name.
    function _localMatches(q) {
        if (q.length === 0) return true;
        const words = [I18n.t("event.zone.local"), "local", "местное"];
        for (const w of words)
            if (String(w).toLowerCase().indexOf(q) === 0) return true;
        return false;
    }
    readonly property var rows: {
        const q = root.query.trim().toLowerCase();
        const out = [];
        const here = AppController.localZoneId();
        if (root._localMatches(q))
            out.push({ id: "", city: I18n.t("event.zone.local"), region: AppController.zoneName(here),
                       offset: AppController.zoneOffsetLabel("") });
        const found = AppController.timeZoneSearch(root.query);
        for (let i = 0; i < found.length; i++) out.push(found[i]);
        // The zone set now, when it is not among them and nothing is typed.
        if (q.length === 0 && root.current.length > 0 && !out.some(r => r.id === root.current))
            out.splice(1, 0, { id: root.current, city: AppController.zoneName(root.current), region: "",
                               offset: AppController.zoneOffsetLabel(root.current) });
        return out;
    }
    onRowsChanged: if (root.index >= root.rows.length) root.index = Math.max(0, root.rows.length - 1)

    // The meeting's start in a row's zone: "10:00", "10:00 · next day".
    function hourIn(tz) {
        const tzEff = tz.length > 0 ? tz : AppController.localZoneId();
        const h = AppController.zoneHour(root.date, root.hour, tzEff);
        if (isNaN(h)) return Theme.fmtHour(root.hour);
        const shift = AppController.zoneDayShift(root.date, root.hour, tzEff);
        return Theme.fmtHour(h) + (shift > 0 ? " · " + I18n.t("event.zone.nextDay")
                                 : shift < 0 ? " · " + I18n.t("event.zone.prevDay") : "");
    }

    function accept() {
        if (root.index < 0 || root.index >= root.rows.length) return false;
        const id = root.rows[root.index].id;
        root.close();
        root.picked(id);
        return true;
    }
    function _move(d) {
        if (root.rows.length === 0) return;
        root.index = (root.index + d + root.rows.length) % root.rows.length;
        list.positionViewAtIndex(root.index, ListView.Contain);
    }

    background: PopupSurface {}

    contentItem: ColumnLayout {
        spacing: Theme.spSm
        Text {
            text: I18n.t("event.zone.title")
            color: Theme.textMuted
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsXs
        }
        TextField {
            id: input
            objectName: "zone-input"
            Layout.fillWidth: true
            color: Theme.text
            placeholderText: I18n.t("event.zone.ph")
            placeholderTextColor: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsMd
            font.weight: Theme.fwBody
            selectByMouse: true
            background: FieldFrame {
                color: "transparent"
                border.color: input.activeFocus ? Theme.focusRing : Theme.borderStrong
            }
            Accessible.name: I18n.t("event.zone.title")
            ContextMenu.menu: TextEditMenu { editor: input }
            onTextChanged: { root.query = text; root.index = 0; }
            onAccepted: root.accept()
            Keys.onDownPressed: root._move(1)
            Keys.onUpPressed: root._move(-1)
        }
        Text {
            objectName: "zone-none"
            visible: root.rows.length === 0
            Layout.fillWidth: true
            text: I18n.t("event.zone.none")
            wrapMode: Text.Wrap
            color: Theme.textMuted
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsSm
        }
        ListView {
            id: list
            objectName: "zone-list"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Theme.px(32) * 8)
            visible: count > 0
            clip: true
            model: root.rows
            currentIndex: root.index
            boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                id: zrow
                required property var modelData
                required property int index
                readonly property bool hot: root.index === zrow.index
                readonly property bool isSet: zrow.modelData.id === root.current
                width: ListView.view ? ListView.view.width : 0
                height: Theme.px(32)
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius
                    color: Theme.panel3
                    opacity: zrow.hot ? 1.0 : 0.0
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: root.index = zrow.index
                    onClicked: { root.index = zrow.index; root.accept(); }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spMd
                    anchors.rightMargin: Theme.spMd
                    spacing: Theme.spMd
                    Text {
                        text: zrow.modelData.city
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsMd
                        font.weight: zrow.isSet ? Theme.fwTitle : Theme.fwBody
                        elide: Text.ElideRight
                        Layout.maximumWidth: Theme.px(170)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: zrow.modelData.region || ""
                        color: Theme.textDim
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsXs
                        elide: Text.ElideRight
                    }
                    Text {
                        objectName: "zone-row-hour"
                        text: root.hourIn(zrow.modelData.id)
                        color: zrow.hot ? Theme.text : Theme.textMuted
                        font.family: Theme.fontUi
                        font.features: Theme.tabularNums
                        font.pixelSize: Theme.fsSm
                    }
                    Text {
                        text: zrow.modelData.offset || ""
                        color: Theme.textDim
                        font.family: Theme.fontUi
                        font.features: Theme.tabularNums
                        font.pixelSize: Theme.fsXs
                        Layout.preferredWidth: Theme.px(64)
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: I18n.t("event.zone.keys")
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsXs
            elide: Text.ElideRight
        }
    }
}
