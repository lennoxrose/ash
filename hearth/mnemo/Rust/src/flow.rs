//! Move-flow engine for the ownership pass (front/src/jown). The C side walks the typed AST and calls
//! in here for every event; this side owns the dataflow state, so the "moved" sets and their joins at
//! branches and loops live in memory-safe code. Contract and ownership rules: `H/flow/flow.h`.

use std::collections::BTreeSet;

#[derive(Clone, Default)]
struct State {
    /// the path ended (return / throw / break / continue); it contributes nothing to a join
    dead: bool,
    moved: BTreeSet<u64>,
}

impl State {
    fn dead() -> State {
        State { dead: true, moved: BTreeSet::new() }
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
