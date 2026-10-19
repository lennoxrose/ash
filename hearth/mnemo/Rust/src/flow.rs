//! Move-flow engine for the ownership pass (front/src/jown). The C side walks the typed AST and calls
//! in here for every event; this side owns the dataflow state, so the "moved" sets and their joins at
//! branches and loops live in memory-safe code. Contract and ownership rules: `H/flow/flow.h`.

use std::collections::{BTreeMap, BTreeSet};

/// Closed integer interval; `i64::MIN` / `i64::MAX` stand for minus / plus infinity.
type Iv = (i64, i64);

#[derive(Clone, Default)]
struct State {
    /// the path ended (return / throw / break / continue); it contributes nothing to a join
    dead: bool,
    moved: BTreeSet<u64>,
    /// known value ranges of integer variables (absent = unknown)
    ranges: BTreeMap<u64, Iv>,
    /// known length ranges of array variables (absent = unknown)
    lens: BTreeMap<u64, Iv>,
}

fn hull(a: Iv, b: Iv) -> Iv {
    (a.0.min(b.0), a.1.max(b.1))
}

/// Widening: a bound that moved since `old` jumps to infinity, so loops reach a fixpoint in a few passes.
fn widen(old: Iv, new: Iv) -> Iv {
    (if new.0 < old.0 { i64::MIN } else { old.0 }, if new.1 > old.1 { i64::MAX } else { old.1 })
}

fn join_maps(a: &mut BTreeMap<u64, Iv>, b: &BTreeMap<u64, Iv>, f: fn(Iv, Iv) -> Iv) {
    a.retain(|k, _| b.contains_key(k));
    for (k, v) in a.iter_mut() {
        *v = f(*v, b[k]);
    }
}

impl State {
    fn dead() -> State {
        State { dead: true, ..State::default() }
    }

    /// Joins `other` into `self`: a dead path contributes nothing.
    fn join(&mut self, other: &State) {
        if other.dead {
            return;
        }
        if self.dead {
            *self = other.clone();
            return;
        }
        self.moved.extend(other.moved.iter().copied());
        join_maps(&mut self.ranges, &other.ranges, hull);
        join_maps(&mut self.lens, &other.lens, hull);
    }

    /// Like `join`, but bounds that grew relative to `self` go to infinity (loop back edges).
    fn join_widen(&mut self, other: &State) {
        if other.dead {
            return;
        }
        if self.dead {
            *self = other.clone();
            return;
        }
        self.moved.extend(other.moved.iter().copied());
        join_maps(&mut self.ranges, &other.ranges, widen);
        join_maps(&mut self.lens, &other.lens, widen);
    }
}

pub struct Walk {
    cur: State,
    /// saved states; a handle is slot index + 1 so that 0 is never valid
    snaps: Vec<Option<State>>,
    free: Vec<usize>,
}

impl Walk {
    fn slot(&mut self, st: State) -> u32 {
        let i = match self.free.pop() {
            Some(i) => {
                self.snaps[i] = Some(st);
                i
            }
            None => {
                self.snaps.push(Some(st));
                self.snaps.len() - 1
            }
        };
        (i + 1) as u32
    }

    fn get(&self, h: u32) -> Option<&State> {
        (h as usize).checked_sub(1).and_then(|i| self.snaps.get(i)).and_then(|s| s.as_ref())
    }
}

fn walk<'a>(w: *mut Walk) -> Option<&'a mut Walk> {
    // SAFETY: the caller passes a pointer from mnemo_walk_new that has not been freed (flow.h).
    unsafe { w.as_mut() }
}

#[no_mangle]
pub extern "C" fn mnemo_walk_new() -> *mut Walk {
    Box::into_raw(Box::new(Walk { cur: State::default(), snaps: Vec::new(), free: Vec::new() }))
}

#[no_mangle]
pub extern "C" fn mnemo_walk_free(w: *mut Walk) {
    if !w.is_null() {
        // SAFETY: allocated by mnemo_walk_new, freed exactly once by contract.
        drop(unsafe { Box::from_raw(w) });
    }
}

#[no_mangle]
pub extern "C" fn mnemo_move(w: *mut Walk, id: u64) {
    if let Some(w) = walk(w) {
        if !w.cur.dead {
            w.cur.moved.insert(id);
        }
    }
}

#[no_mangle]
pub extern "C" fn mnemo_reinit(w: *mut Walk, id: u64) {
    if let Some(w) = walk(w) {
        w.cur.moved.remove(&id);
    }
}

#[no_mangle]
pub extern "C" fn mnemo_is_moved(w: *mut Walk, id: u64) -> i32 {
    match walk(w) {
        Some(w) => (!w.cur.dead && w.cur.moved.contains(&id)) as i32,
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn mnemo_kill(w: *mut Walk) {
    if let Some(w) = walk(w) {
        w.cur.dead = true;
    }
}

#[no_mangle]
pub extern "C" fn mnemo_is_dead(w: *mut Walk) -> i32 {
    walk(w).map_or(0, |w| w.cur.dead as i32)
}

#[no_mangle]
pub extern "C" fn mnemo_save(w: *mut Walk) -> u32 {
    match walk(w) {
        Some(w) => {
            let st = w.cur.clone();
            w.slot(st)
        }
        None => 0,
    }
}

/// A snapshot that is already dead: the empty accumulator for `break`/`continue` states.
#[no_mangle]
pub extern "C" fn mnemo_save_dead(w: *mut Walk) -> u32 {
    walk(w).map_or(0, |w| w.slot(State::dead()))
}

#[no_mangle]
pub extern "C" fn mnemo_restore(w: *mut Walk, h: u32) -> i32 {
    let Some(w) = walk(w) else { return -1 };
    match w.get(h).cloned() {
        Some(st) => {
            w.cur = st;
            0
        }
        None => -1,
    }
}

#[no_mangle]
pub extern "C" fn mnemo_join(w: *mut Walk, h: u32) -> i32 {
    let Some(w) = walk(w) else { return -1 };
    match w.get(h).cloned() {
        Some(st) => {
            w.cur.join(&st);
            0
        }
        None => -1,
    }
}

/// Joins the current state into snapshot `h` (accumulates a `break` / `continue` exit).
#[no_mangle]
pub extern "C" fn mnemo_join_into(w: *mut Walk, h: u32) -> i32 {
    let Some(w) = walk(w) else { return -1 };
    let cur = w.cur.clone();
    match (h as usize).checked_sub(1).and_then(|i| w.snaps.get_mut(i)).and_then(|s| s.as_mut()) {
        Some(st) => {
            st.join(&cur);
            0
        }
        None => -1,
    }
}

/// Releases snapshot `h`; its slot is reused. Unknown or already released handles are ignored.
#[no_mangle]
pub extern "C" fn mnemo_release(w: *mut Walk, h: u32) {
    if let Some(w) = walk(w) {
        if let Some(i) = (h as usize).checked_sub(1) {
            if matches!(w.snaps.get(i), Some(Some(_))) {
                w.snaps[i] = None;
                w.free.push(i);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn mnemo_rng_set(w: *mut Walk, id: u64, lo: i64, hi: i64) {
    if let Some(w) = walk(w) {
        if lo <= hi {
            w.cur.ranges.insert(id, (lo, hi));
        } else {
            w.cur.ranges.remove(&id);
        }
    }
}

#[no_mangle]
pub extern "C" fn mnemo_rng_clear(w: *mut Walk, id: u64) {
    if let Some(w) = walk(w) {
        w.cur.ranges.remove(&id);
    }
}

#[no_mangle]
pub extern "C" fn mnemo_rng_get(w: *mut Walk, id: u64, lo: *mut i64, hi: *mut i64) -> i32 {
    let Some(w) = walk(w) else { return 0 };
    if w.cur.dead || lo.is_null() || hi.is_null() {
        return 0;
    }
    match w.cur.ranges.get(&id) {
        // SAFETY: the caller passes valid out pointers (flow.h).
        Some(&(a, b)) => unsafe {
            *lo = a;
            *hi = b;
            1
        },
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn mnemo_len_set(w: *mut Walk, id: u64, lo: i64, hi: i64) {
    if let Some(w) = walk(w) {
        if 0 <= lo && lo <= hi {
            w.cur.lens.insert(id, (lo, hi));
        } else {
            w.cur.lens.remove(&id);
        }
    }
}

#[no_mangle]
pub extern "C" fn mnemo_len_clear(w: *mut Walk, id: u64) {
    if let Some(w) = walk(w) {
        w.cur.lens.remove(&id);
    }
}

#[no_mangle]
pub extern "C" fn mnemo_len_get(w: *mut Walk, id: u64, lo: *mut i64, hi: *mut i64) -> i32 {
    let Some(w) = walk(w) else { return 0 };
    if w.cur.dead || lo.is_null() || hi.is_null() {
        return 0;
    }
    match w.cur.lens.get(&id) {
        // SAFETY: the caller passes valid out pointers (flow.h).
        Some(&(a, b)) => unsafe {
            *lo = a;
            *hi = b;
            1
        },
        None => 0,
    }
}

/// Narrows `id` to its intersection with [lo, hi] (a branch condition held). An empty
/// intersection kills the path: no execution gets here. Returns 0 in that case, else 1.
#[no_mangle]
pub extern "C" fn mnemo_rng_refine(w: *mut Walk, id: u64, lo: i64, hi: i64) -> i32 {
    let Some(w) = walk(w) else { return 1 };
    if w.cur.dead {
        return 0;
    }
    let (a, b) = w.cur.ranges.get(&id).copied().unwrap_or((i64::MIN, i64::MAX));
    let (a, b) = (a.max(lo), b.min(hi));
    if a > b {
        w.cur.dead = true;
        return 0;
    }
    w.cur.ranges.insert(id, (a, b));
    1
}

/// Forgets every range and length (state too imprecise to keep, e.g. a loop that never stabilised).
#[no_mangle]
pub extern "C" fn mnemo_havoc(w: *mut Walk) {
    if let Some(w) = walk(w) {
        w.cur.ranges.clear();
        w.cur.lens.clear();
    }
}

/// Loop back edge: current := current widened with snapshot `h`.
#[no_mangle]
pub extern "C" fn mnemo_join_widen(w: *mut Walk, h: u32) -> i32 {
    let Some(w) = walk(w) else { return -1 };
    match w.get(h).cloned() {
        Some(st) => {
            w.cur.join_widen(&st);
            0
        }
        None => -1,
    }
}

/// 1 when the current state equals snapshot `h` (moved set, ranges and lengths), else 0.
#[no_mangle]
pub extern "C" fn mnemo_equal(w: *mut Walk, h: u32) -> i32 {
    let Some(w) = walk(w) else { return 0 };
    match w.get(h) {
        Some(st) => {
            (st.dead == w.cur.dead && st.moved == w.cur.moved && st.ranges == w.cur.ranges && st.lens == w.cur.lens) as i32
        }
        None => 0,
    }
}

fn clamp(v: i128) -> i64 {
    v.clamp(i64::MIN as i128, i64::MAX as i128) as i64
}

fn corners(a: Iv, b: Iv, f: impl Fn(i128, i128) -> i128) -> (i128, i128) {
    let c = [f(a.0 as i128, b.0 as i128), f(a.0 as i128, b.1 as i128), f(a.1 as i128, b.0 as i128), f(a.1 as i128, b.1 as i128)];
    (*c.iter().min().unwrap(), *c.iter().max().unwrap())
}

/// Interval arithmetic for `op` in `+ - * / % &`. Returns 1 with the result in `*olo..*ohi`;
/// 2 when the exact result does not fit in 64 bits (treat as unknown); 3 when the divisor is
/// exactly zero (`/` or `%` can only fail); 0 for an unsupported operator.
#[no_mangle]
pub extern "C" fn mnemo_iv_op(op: u8, alo: i64, ahi: i64, blo: i64, bhi: i64, olo: *mut i64, ohi: *mut i64) -> i32 {
    if olo.is_null() || ohi.is_null() {
        return 0;
    }
    let (a, b) = ((alo, ahi), (blo, bhi));
    let (lo, hi): (i128, i128) = match op {
        b'+' => (a.0 as i128 + b.0 as i128, a.1 as i128 + b.1 as i128),
        b'-' => (a.0 as i128 - b.1 as i128, a.1 as i128 - b.0 as i128),
        b'*' => corners(a, b, |x, y| x * y),
        b'/' | b'%' => {
            if b == (0, 0) {
                return 3;
            }
            if op == b'/' {
                // split the divisor around zero; each half has a fixed sign, so the corners bound it
                let mut res: Option<(i128, i128)> = None;
                for part in [(b.0, b.1.min(-1)), (b.0.max(1), b.1)] {
                    if part.0 > part.1 {
                        continue;
                    }
                    let r = corners(a, part, |x, y| x / y);
                    res = Some(res.map_or(r, |p| (p.0.min(r.0), p.1.max(r.1))));
                }
                res.unwrap()
            } else {
                let m = (b.0 as i128).abs().max((b.1 as i128).abs()) - 1;
                if a.0 >= 0 {
                    (0, (a.1 as i128).min(m))
                } else if a.1 <= 0 {
                    ((a.0 as i128).max(-m), 0)
                } else {
                    ((a.0 as i128).max(-m), (a.1 as i128).min(m))
                }
            }
        }
        b'&' => {
            // a non-negative operand caps the result: x & m is in [0, m] for m >= 0
            let mut cap: Option<i128> = None;
            for iv in [a, b] {
                if iv.0 >= 0 {
                    cap = Some(cap.map_or(iv.1 as i128, |c| c.min(iv.1 as i128)));
                }
            }
            match cap {
                Some(c) => (0, c),
                None => return 2,
            }
        }
        _ => return 0,
    };
    if lo < i64::MIN as i128 || hi > i64::MAX as i128 {
        return 2;
    }
    // SAFETY: the caller passes valid out pointers (flow.h).
    unsafe {
        *olo = clamp(lo);
        *ohi = clamp(hi);
    }
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    fn op(o: u8, a: Iv, b: Iv) -> (i32, Iv) {
        let (mut l, mut h) = (0, 0);
        let r = mnemo_iv_op(o, a.0, a.1, b.0, b.1, &mut l, &mut h);
        (r, (l, h))
    }

    #[test]
    fn arithmetic() {
        assert_eq!(op(b'+', (1, 2), (10, 20)), (1, (11, 22)));
        assert_eq!(op(b'-', (1, 2), (10, 20)), (1, (-19, -8)));
        assert_eq!(op(b'*', (-2, 3), (4, 5)), (1, (-10, 15)));
        assert_eq!(op(b'/', (10, 20), (2, 5)), (1, (2, 10)));
        assert_eq!(op(b'/', (10, 20), (-1, 1)), (1, (-20, 20)));
        assert_eq!(op(b'/', (1, 1), (0, 0)).0, 3);
        assert_eq!(op(b'%', (0, 100), (1, 8)), (1, (0, 7)));
        assert_eq!(op(b'%', (-5, 5), (3, 3)), (1, (-2, 2)));
        assert_eq!(op(b'&', (-5, 5), (0, 7)), (1, (0, 7)));
        assert_eq!(op(b'+', (i64::MAX, i64::MAX), (1, 1)).0, 2);
    }

    #[test]
    fn widening_reaches_a_fixpoint() {
        let w = mnemo_walk_new();
        mnemo_rng_set(w, 1, 0, 0);
        let head = mnemo_save(w);
        mnemo_rng_set(w, 1, 1, 1); // end of the body: i = 1
        mnemo_restore(w, head);
        let end = {
            mnemo_rng_set(w, 1, 1, 1);
            mnemo_save(w)
        };
        mnemo_restore(w, head);
        mnemo_join_widen(w, end);
        let (mut lo, mut hi) = (0, 0);
        assert_eq!(mnemo_rng_get(w, 1, &mut lo, &mut hi), 1);
        assert_eq!((lo, hi), (0, i64::MAX));
        let h2 = mnemo_save(w);
        assert_eq!(mnemo_equal(w, h2), 1);
        mnemo_walk_free(w);
    }

    #[test]
    fn refine_kills_impossible_paths() {
        let w = mnemo_walk_new();
        mnemo_rng_set(w, 7, 10, 10);
        assert_eq!(mnemo_rng_refine(w, 7, i64::MIN, 3), 0);
        assert_eq!(mnemo_is_dead(w), 1);
        mnemo_walk_free(w);
    }

    #[test]
    fn join_drops_facts_known_on_one_side_only() {
        let w = mnemo_walk_new();
        let entry = mnemo_save(w);
        mnemo_rng_set(w, 1, 5, 5);
        mnemo_join(w, entry);
        let (mut lo, mut hi) = (0, 0);
        assert_eq!(mnemo_rng_get(w, 1, &mut lo, &mut hi), 0);
        mnemo_walk_free(w);
    }
}
