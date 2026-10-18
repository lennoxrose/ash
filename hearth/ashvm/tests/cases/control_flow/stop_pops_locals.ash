// `stop` leaves a loop whose body declared locals: later locals must still get the right slots.
forge plain() {
    local q = 4;
    during yes {
        local t = q;
        q = t + 1;
        given q > 5 { stop; }
    }
    local s = "n";
    yield s + str(q);
}

forge nested() {
    local q = 0;
    during yes {
        local a = q;
        given a >= 0 {
            local b = a + 1;
            given b == 2 { stop; }
        }
        q += 1;
    }
    local after = "after";
    yield after + str(q);
}

forge each_stop() {
    local last = 0;
    each x in [1, 2, 3, 4] {
        local y = x * 2;
        given y > 4 { stop; }
        last = y;
    }
    local tail = "tail";
    yield tail + str(last);
}

forge with_next() {
    local c = 0;
    during c < 3 {
        local t = c;
        c += 1;
        given t == 1 { next; }
    }
    local done = "done";
    yield done + str(c);
}

say plain();
say nested();
say each_stop();
say with_next();
