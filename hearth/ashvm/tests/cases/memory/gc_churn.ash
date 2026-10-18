// Allocates ~1.2M short-lived strings, arrays and maps. The default VM never frees them (about 200 MB
// here); the experimental collector (make GC=1) keeps the footprint small. Output is identical.
forge churn(n) {
    local keep = [];
    local i = 0;
    during i < n {
        local s = "item-" + str(i);
        local t = s + "-" + s;
        local m = {"a": s, "b": [s, t]};
        given i % 1000 == 0 { push(keep, m); }
        i += 1;
    }
    yield len(keep);
}
say churn(200000);
