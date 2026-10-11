.pragma library

// Pure helpers for the small motions (APP-167), so the rules are testable
// without a clock: callers pass `now`.

// Whether closing a task plays the done moment (APP-176): one card
// (`moved`), going into Done from another column, with motion on. A bulk
// move and "Reduce motion" just move.
function shouldStack(moved, fromStatus, toStatus, motion) {
    return moved === 1 && toStatus === "done" && fromStatus !== "done" && motion > 0;
}

// Where a dropped card starts its settle: the offset from its home slot to
// the point it was let go, scaled by Theme.motion (0 with reduced motion), so
// it glides the rest of the way home instead of jumping.
function settleFrom(dropX, dropY, homeX, homeY, motion) {
    var m = motion > 0 ? 1 : 0;
    return { x: (dropX - homeX) * m, y: (dropY - homeY) * m };
}

// Which of `ids` were not in the list the last time `memo` saw it, as a
// set ({id: true}), so a row rebuilt from a snapshot can tell an arrival
// (a new task, a group unfolded, a search widening) from a row that was
// already on screen. The first call only remembers: a list that opens
// does not fade every row in.
function arrivals(memo, ids) {
    var fresh = {};
    var next = {};
    for (var i = 0; i < ids.length; i++) {
        if (memo.ids && !memo.ids[ids[i]]) fresh[ids[i]] = true;
        next[ids[i]] = true;
    }
    memo.ids = next;
    return fresh;
}
