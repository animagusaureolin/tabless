// Side-by-side layout for events that share a time window (qml/Overlap.js).
//
// Two meetings at 10:00 were drawn exactly on top of each other in the week
// grid: the second hid the first, and only the top one could be clicked. The
// day grid had solved this and the week grid had never been taught, so the
// algorithm now lives in one place and both call it.
import QtQuick
import QtTest
import "../../qml/Overlap.js" as Overlap

TestCase {
    id: tc
    name: "Overlap"

    function ev(id, start, end) {
        return { id: id, start: start, end: end };
    }

    function test_one_event_takes_the_whole_width() {
        const m = Overlap.compute([ev("a", 10, 11)]);
        compare(m["a"].col, 0);
        compare(m["a"].cols, 1);
    }

    function test_two_overlapping_events_split_the_width() {
        const m = Overlap.compute([ev("a", 10, 11), ev("b", 10, 11)]);
        compare(m["a"].cols, 2);
        compare(m["b"].cols, 2);
        verify(m["a"].col !== m["b"].col, "they must not land in the same column");
    }

    function test_three_overlapping_events_split_three_ways() {
        const m = Overlap.compute([ev("a", 10, 12), ev("b", 10.5, 11), ev("c", 10.5, 11.5)]);
        compare(m["a"].cols, 3);
        compare(m["b"].cols, 3);
        compare(m["c"].cols, 3);
        const cols = [m["a"].col, m["b"].col, m["c"].col].sort();
        compare(cols, [0, 1, 2]);
    }

    // Events that merely touch do not overlap: a 10–11 and an 11–12 are
    // consecutive, and forcing them side by side would halve both for nothing.
    function test_back_to_back_events_do_not_overlap() {
        const m = Overlap.compute([ev("a", 10, 11), ev("b", 11, 12)]);
        compare(m["a"].cols, 1);
        compare(m["b"].cols, 1);
    }

    // A column frees up when its event ends, so a third event can reuse it
    // rather than making the whole cluster narrower.
    function test_a_column_is_reused_once_it_is_free() {
        const m = Overlap.compute([ev("a", 9, 10), ev("b", 9, 12), ev("c", 10, 11)]);
        compare(m["b"].cols, 2) << "only two are ever concurrent";
        compare(m["a"].col, m["c"].col);
    }

    // Every member of a cluster gets the cluster's count, so the widths match
    // and the events tile the day exactly instead of leaving a gap.
    function test_a_cluster_shares_one_column_count() {
        const m = Overlap.compute([ev("a", 9, 12), ev("b", 9.5, 10), ev("c", 10.5, 11)]);
        compare(m["a"].cols, m["b"].cols);
        compare(m["b"].cols, m["c"].cols);
    }

    // Two separate clusters are sized independently — a crowded morning must
    // not squeeze a lone afternoon meeting.
    function test_separate_clusters_are_independent() {
        const m = Overlap.compute([ev("a", 9, 10), ev("b", 9, 10), ev("late", 15, 16)]);
        compare(m["a"].cols, 2);
        compare(m["late"].cols, 1);
    }

    function test_input_order_does_not_matter() {
        const forward = Overlap.compute([ev("a", 9, 11), ev("b", 10, 12)]);
        const backward = Overlap.compute([ev("b", 10, 12), ev("a", 9, 11)]);
        compare(forward["a"].col, backward["a"].col);
        compare(forward["b"].col, backward["b"].col);
        compare(forward["a"].cols, backward["a"].cols);
    }

    function test_an_empty_day_is_an_empty_map() {
        const m = Overlap.compute([]);
        compare(Object.keys(m).length, 0);
    }

    // The week groups by day first: events on different days never overlap,
    // however close their hours are.
    function test_events_on_different_days_do_not_overlap() {
        const m = Overlap.computeByDay([[ev("mon", 10, 11)], [ev("tue", 10, 11)]]);
        compare(m["mon"].cols, 1);
        compare(m["tue"].cols, 1);
    }

    function test_by_day_keeps_each_days_own_clusters() {
        const m = Overlap.computeByDay([
            [ev("m1", 10, 11), ev("m2", 10, 11)],
            [ev("t1", 10, 11)]
        ]);
        compare(m["m1"].cols, 2);
        compare(m["t1"].cols, 1);
    }

    // A zero-length event is degenerate but must not break the sweep.
    function test_a_zero_length_event_is_handled() {
        const m = Overlap.compute([ev("a", 10, 10), ev("b", 10, 11)]);
        verify(m["a"] !== undefined);
        verify(m["b"] !== undefined);
    }

    // Lanes tile while each stays readable (VISU-15); past that only as many
    // as fit are drawn and the rest fold onto a "+N" chip (A1-7, EYES-1).
    function test_lanes_tile_while_wide_enough() {
        const a = Overlap.lane(1, 2, 200, 64);
        compare(a.x, 100);
        compare(a.w, 100);
        verify(!a.folded);
        const solo = Overlap.lane(0, 1, 40, 64);
        compare(solo.x, 0);
        compare(solo.w, 40, "a lone event never grows past its day");
    }

    function test_narrow_lanes_fold_instead_of_cascading() {
        const avail = 130, minW = 64, cols = 4;
        compare(Overlap.capacity(avail, minW), 2);
        const l0 = Overlap.lane(0, cols, avail, minW);
        const l1 = Overlap.lane(1, cols, avail, minW);
        compare(l0.w, 65); compare(l0.x, 0); verify(!l0.folded);
        compare(l1.w, 65); compare(l1.x, 65); verify(!l1.folded);
        for (let c = 2; c < cols; c++) {
            const l = Overlap.lane(c, cols, avail, minW);
            verify(l.folded, "lane " + c + " folds");
            compare(l.x, 65, "drawn in the last lane under the cursor");
        }
        // Nothing drawn overlaps: no title is covered by a neighbour.
        verify(l0.x + l0.w <= l1.x + 1e-9);
        compare(Overlap.lane(0, 3, 50, 64).w, 25, "two lanes always stand side by side");
        verify(!Overlap.lane(1, 3, 50, 64).folded, "the second of two overlapping is drawn");
    }

    // B3 verify: a 102-120px day (1312-1440px windows) draws two
    // overlapping meetings side by side; none hides behind "+1".
    function test_an_ordinary_day_draws_two_lanes() {
        compare(Overlap.capacity(98), 2);
        compare(Overlap.capacity(116), 2);
        compare(Overlap.capacity(30), 2);
        const items = [{ id: "dev", start: 9, end: 10 }, { id: "ops", start: 9.5, end: 11 }];
        compare(Overlap.folds(items, Overlap.compute(items), Overlap.capacity(98)).length, 0);
    }

    // F2-REG-2..4: the week measures its own minimum and floors at one
    // lane, so no lane is drawn narrower than it.
    function test_a_measured_minimum_never_squeezes() {
        compare(Overlap.capacity(100, 57, 1), 1);
        compare(Overlap.capacity(114, 57, 1), 2);
        const l = Overlap.lane(1, 2, 100, 57, 1);
        verify(l.folded, "the second folds onto +1 instead of two 50px lanes");
        compare(l.w, 100);
        compare(Overlap.capacity(100, 57), 2, "the default floor is still two");
    }

    // The "+N" chip keeps off the title of the block drawn in the last lane.
    function test_fold_chip_keeps_off_titles() {
        const chip = 0.4, title = 1.4;
        // Nothing in the last lane there: at the first folded start.
        compare(Overlap.chipStart({ start: 10, lane: [] }, chip, title), 10);
        // A tall block from 9 to 13: at its bottom, under its title.
        fuzzyCompare(Overlap.chipStart({ start: 9.5, lane: [{ start: 9, end: 13 }] }, chip, title), 12.6, 1e-9);
        // A short one (16:00-17:30): under it.
        compare(Overlap.chipStart({ start: 16.25, lane: [{ start: 16, end: 17.5 }] }, chip, title), 17.5);
        // The folded start already below the block's title stays put.
        compare(Overlap.chipStart({ start: 11, lane: [{ start: 9, end: 13 }] }, chip, title), 11);
        const items = [{ id: "a", start: 9, end: 10 }, { id: "b", start: 9, end: 11 },
                       { id: "c", start: 9.5, end: 10.5 }];
        const f = Overlap.folds(items, Overlap.compute(items), 2);
        compare(f.length, 1);
        compare(f[0].lane.length, 1, "b is drawn in the last lane");
        compare(f[0].lane[0].start, 9);
    }

    function test_folds_count_what_is_not_drawn() {
        const items = [{ id: "a", start: 9, end: 9.5 }, { id: "b", start: 9, end: 10 },
                       { id: "c", start: 9.25, end: 9.75 }, { id: "d", start: 9.5, end: 11 },
                       { id: "e", start: 14, end: 15 }];
        const slots = Overlap.compute(items);
        const f = Overlap.folds(items, slots, 2);
        compare(f.length, 1);
        compare(f[0].n, 1, "c sits in a third lane; d reuses the first");
        compare(f[0].start, 9.25);
    }

    // APP-199: a week event of half an hour or more is two lines high, the
    // time on top of the title; a shorter one is one line. The grown block
    // reports how far down it reaches so the overlap layout can tile it.
    function test_week_block_heights() {
        const hourH = 38, one = 20, two = 34;
        const half = Overlap.block(11, 11.5, hourH, one, two);
        verify(half.twoLine, "a half-hour meeting gets two lines");
        compare(half.height, two, "17px grows to two lines");
        verify(half.visualEnd > 11.5, "and reaches past its end");
        const quarter = Overlap.block(10, 10.25, hourH, one, two);
        verify(!quarter.twoLine, "a 15-minute standup is one line");
        compare(quarter.height, one);
        const twoHours = Overlap.block(13, 15, hourH, one, two);
        verify(twoHours.twoLine);
        compare(twoHours.height, 2 * hourH - 2, "a long event keeps its own height");
        compare(twoHours.visualEnd, 15);
        // Grown to two lines, 11:00-11:30 now sits beside an 11:30 meeting.
        const m = Overlap.compute([ev("a", 11, half.visualEnd), ev("b", 11.5, 12)]);
        compare(m["a"].cols, 2);
    }
}
