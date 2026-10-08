attempt { local a = [1]; say a[3]; } handle (e) { say e.message; say "x: " + e; }
attempt { raise {"message": "custom", "line": 3}; } handle (e) { say e.message; say e.line; }
attempt { raise "plain"; } handle (e) { say e.message == e; }
local m = {"inner": {"deep": 7}};
say m.inner.deep;
