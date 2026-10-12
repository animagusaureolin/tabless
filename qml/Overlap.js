.pragma library

// Side-by-side layout for events that share a time window.
//
// Two meetings at 10:00 used to be drawn exactly on top of each other in the
// week grid — the second one hid the first, and only the top one could be
// clicked. The day grid had solved this; the week grid had never been taught.
// Rather than a second copy of the algorithm, both now call this.
//
// Given [{ id, start, end }, …] it returns { id: { col, cols } }: which column
// an event sits in, and how many columns its overlap cluster spans, so the
// caller can size it to 1/cols of the day's width.

// Events that overlap each other form a cluster. Every member of a cluster
// gets the cluster's column count, so their widths match and they tile the
// day exactly — sizing each one by its own neighbours would leave gaps.
function _flush(cluster, map) {
    const colEnds = [];   // the last end-time occupying each column
    for (let k = 0; k < cluster.length; k++) {
        const e = cluster[k];
        let placed = false;
        for (let c = 0; c < colEnds.length; c++) {
            if (e.start >= colEnds[c] - 1e-9) {   // that column is free again
                colEnds[c] = e.end;
                e._col = c;
                placed = true;
                break;
            }
        }
        if (!placed) { e._col = colEnds.length; colEnds.push(e.end); }
    }
    const total = Math.max(1, colEnds.length);
    const key = cluster[0].id;
    for (let k = 0; k < cluster.length; k++)
        map[cluster[k].id] = { col: cluster[k]._col, cols: total, cluster: key };
}

// `events` is [{ id, start, end }, …] for ONE day. Events from different days
// never overlap, so the caller groups first.
function compute(events) {
    const evs = events.slice();
    // By start, then end. The sweep below depends on it.
    evs.sort(function (a, b) { return (a.start - b.start) || (a.end - b.end); });

    const map = {};
    let cluster = [];
    let clusterEnd = -1;
    for (let i = 0; i < evs.length; i++) {
        const e = evs[i];
        if (cluster.length === 0) { cluster = [e]; clusterEnd = e.end; continue; }
        if (e.start < clusterEnd - 1e-9) {        // overlaps the running cluster
            cluster.push(e);
            clusterEnd = Math.max(clusterEnd, e.end);
        } else {
            _flush(cluster, map);
            cluster = [e];
            clusterEnd = e.end;
        }
    }
    if (cluster.length > 0) _flush(cluster, map);
    return map;
}

// Same, for a week: `days` is [[{id,start,end}, …], …], one list per day.
// Returns one flat map, because ids are unique across the week.
function computeByDay(days) {
    const map = {};
    for (let i = 0; i < days.length; i++) {
        const dayMap = compute(days[i]);
        for (const id in dayMap) map[id] = dayMap[id];
    }
    return map;
}

// Where a slot from compute() goes across `avail` pixels: { x, w, folded }.
//
// Tiled 1/cols each while a lane is at least `minW` wide. Past that the
// lanes used to cascade over each other: four meetings in a 106px day came
// out as staircase slivers titled "S", "D", "1" and a task in the middle
// lane sat under both its neighbours (A1-7, A2-12, A6-10, EYES-1). Now only
// as many lanes as fit at `minW` are drawn, side by side; the rest are
// `folded` - not drawn, counted on a "+N" chip that opens the day, and drawn
// in the last lane while the keyboard cursor is on one of them.
// The narrowest a lane gets (`minW` overrides): its title keeps ~6 letters.
const MIN_LANE_W = 64;

// How many lanes fit side by side.
function capacity(avail, minW) {
    if (minW === undefined) minW = MIN_LANE_W;
    return Math.max(1, Math.floor((avail + 1e-9) / minW));
}

function lane(col, cols, avail, minW) {
    const n = Math.max(1, cols);
    const fit = capacity(avail, minW);
    if (n <= fit) {
        const tile = avail / n;
        return { x: col * tile, w: tile, folded: false };
    }
    const w = avail / fit;
    if (col < fit - 1) return { x: col * w, w: w, folded: false };
    // The last lane holds the first folded-over one's place: its own when
    // it is in that lane, else drawn there only under the cursor.
    return { x: (fit - 1) * w, w: w, folded: col > fit - 1 };
}

// The "+N" chips of one day: for every cluster wider than `fit` lanes, how
// many are folded and from what hour. `items` is [{ id, start, end }] and
// `slots` the compute() map.
function folds(items, slots, fit) {
    const byCluster = {};
    for (let i = 0; i < items.length; i++) {
        const s = slots[items[i].id];
        if (!s || s.cols <= fit || s.col < fit) continue;
        const k = s.cluster;
        if (!byCluster[k]) byCluster[k] = { n: 0, start: items[i].start, ids: [] };
        byCluster[k].n++;
        byCluster[k].start = Math.min(byCluster[k].start, items[i].start);
        byCluster[k].ids.push(items[i].id);
    }
    const out = [];
    for (const k in byCluster) out.push(byCluster[k]);
    return out;
}

// How tall an event is drawn in the week grid, and whether it gets two lines
// (APP-199). Half an hour or more: at least two lines high, the time small
// on top and the title under it; a 30-minute meeting was 17px and clipped
// down to its start time. Shorter: one line, time and title side by side.
// `visualEnd` is the hour the block reaches once grown, so the overlap
// layout tiles a stretched block beside the next one instead of under it.
var TWO_LINE_HOURS = 0.5;
function block(start, end, hourH, oneLineH, twoLineH) {
    const dur = Math.max(0, end - start);
    const twoLine = dur >= TWO_LINE_HOURS - 1e-9;
    const height = Math.max(twoLine ? twoLineH : oneLineH, dur * hourH - 2);
    return { height: height, twoLine: twoLine, visualEnd: Math.max(end, start + (height + 2) / hourH) };
}
