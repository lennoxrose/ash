local a = [1, 2];
attempt { say a[5]; } handle (e) { say e; }
attempt { say {"k": 1}["zz"]; } handle (e) { say e; }
attempt { say "abc"[9]; } handle (e) { say e; }
attempt { say "x" + 1; } handle (e) { say e; }
attempt { say 1 + "x"; } handle (e) { say e; }
attempt { say str("t"); } handle (e) { say e; }
attempt { pop([]); } handle (e) { say e; }
attempt { delete([1], 3); } handle (e) { say e; }
attempt { insert([1], 3, 0); } handle (e) { say e; }
attempt { slice([1, 2], 1, 5); } handle (e) { say e; }
attempt { chr(0); } handle (e) { say e; }
attempt { ord(""); } handle (e) { say e; }
attempt { repeat("a", -2); } handle (e) { say e; }
attempt { read_file("/nonexistent/file"); } handle (e) { say e; }
attempt { list_dir("/nonexistent"); } handle (e) { say e; }
attempt { given [1] { say "bad"; } } handle (e) { say e; }
attempt { raise "custom"; } handle (e) { say e; }
attempt { raise {"kind": "parse", "line": 3, "col": 7}; } handle (e) { say e["kind"] + ":" + str(e["line"]) + ":" + str(e["col"]); }
attempt { raise [1, 2]; } handle (e) { say e; }
attempt { raise 42; } handle (e) { say e + 1; }
forge f() { raise {"m": "inner"}; }
attempt { f(); } handle (e) { say e["m"]; }
