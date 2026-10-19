#!/usr/bin/env python3
# Scripted client for `ashvm lsp` (see lsp_check.sh). Usage: lsp_check.py <path to ashvm>
import json, os, select, shutil, subprocess, sys, tempfile, time

ASHVM = os.path.abspath(sys.argv[1])
failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL: " + what)


class Client:
    def __init__(self, cwd, env=None):
        self.p = subprocess.Popen([ASHVM, "lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, cwd=cwd, env=env)
        self.buf = b""
        self.next_id = 0

    def send_raw(self, data):
        self.p.stdin.write(data)
        self.p.stdin.flush()

    def send(self, obj):
        body = json.dumps(obj).encode()
        self.send_raw(b"Content-Length: %d\r\n\r\n" % len(body) + body)

    def read(self, timeout=30):
        end = time.time() + timeout
        while True:
            i = self.buf.find(b"\r\n\r\n")
            if i >= 0:
                head = self.buf[:i].decode()
                n = int([l for l in head.split("\r\n") if l.lower().startswith("content-length")][0].split(":")[1])
                if len(self.buf) >= i + 4 + n:
                    body = self.buf[i + 4:i + 4 + n]
                    self.buf = self.buf[i + 4 + n:]
                    return json.loads(body)
            left = end - time.time()
            if left <= 0:
                return None
            r, _, _ = select.select([self.p.stdout], [], [], left)
            if not r:
                return None
            chunk = os.read(self.p.stdout.fileno(), 65536)
            if not chunk:
                return None
            self.buf += chunk

    def request(self, method, params, timeout=30):
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
        end = time.time() + timeout
        while time.time() < end:
            m = self.read(end - time.time())
            if m is None:
                break
            if m.get("id") == self.next_id and "method" not in m:
                return m
        raise RuntimeError("no reply to " + method)

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def diagnostics(self, uri, timeout=30):
        end = time.time() + timeout
        while time.time() < end:
            m = self.read(end - time.time())
            if m is None:
                break
            if m.get("method") == "textDocument/publishDiagnostics" and m["params"]["uri"] == uri:
                return m["params"]["diagnostics"]
        raise RuntimeError("no diagnostics for " + uri)

    def open(self, uri, text, version=1):
        self.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "ash", "version": version, "text": text}})

    def change(self, uri, text, version):
        self.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": version}, "contentChanges": [{"text": text}]})

    def hover(self, uri, line, ch):
        return self.request("textDocument/hover", {"textDocument": {"uri": uri}, "position": {"line": line, "character": ch}}).get("result")

    def definition(self, uri, line, ch):
        return self.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": {"line": line, "character": ch}}).get("result")

    def alive(self, uri=None):
        """a request that has an answer proves the server is still reading"""
        try:
            r = self.request("textDocument/hover", {"textDocument": {"uri": uri or "file:///nonexistent.ash"}, "position": {"line": 0, "character": 0}}, 10)
            return "result" in r
        except RuntimeError:
            return False

    def shutdown(self):
        r = self.request("shutdown", None, 10)
        self.notify("exit", None)
        return r, self.p.wait(10)


def pos_of(text, needle, nth=0):
    """(line, character) of the nth occurrence of needle, in UTF-16 units"""
    idx = -1
    for _ in range(nth + 1):
        idx = text.index(needle, idx + 1)
    line = text.count("\n", 0, idx)
    start = text.rfind("\n", 0, idx) + 1
    ch = len(text[start:idx].encode("utf-16-le")) // 2
    return line, ch


def hover_text(r):
    return r["contents"]["value"] if r else None


def main():
    root = tempfile.mkdtemp(prefix="ashlsp")
    try:
        run(root)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    if failures:
        print("%d check(s) failed" % len(failures))
        sys.exit(1)
    print("lsp check ok")


def run(root):
    os.makedirs(root + "/util")
    open(root + "/ash.pkg", "w").write("name: lsp-demo\nversion: 1\n")
    env = dict(os.environ, ASH_LSP_SECONDS="3")          # the time limit of one analysis (10 s by default)

    main_src = """import util.Maths;

public class Main {
    private String label = "x";
    static int twice(int v) {
        return v * 2;
    }
    public static void main(String[] args) {
        int count = 3;
        String s = "hi";
        int r = twice(count) + s.length() + Maths.square(count);
        Main m = new Main();
        System.out.println(m.label + r + s.trim());
    }
}
"""
    maths_src = """package util;

public class Maths {
    public static int square(int n) {
        return n * n;
    }
}
"""
    mf = root + "/Main.ash"
    uf = root + "/util/Maths.ash"
    open(mf, "w").write(main_src)
    open(uf, "w").write(maths_src)
    muri, uuri = "file://" + mf, "file://" + uf

    c = Client(root, env)
    init = c.request("initialize", {"processId": None, "rootUri": "file://" + root, "capabilities": {}})["result"]
    caps = init["capabilities"]
    check(caps.get("hoverProvider") is True and caps.get("definitionProvider") is True, "initialize announces hover and definition")
    check(caps["textDocumentSync"]["change"] == 1 and caps["textDocumentSync"]["openClose"] is True, "initialize announces full-text sync")
    c.notify("initialized", {})

    # ---- a clean file: an (empty) diagnostics list, hover and definition
    c.open(muri, main_src)
    check(c.diagnostics(muri) == [], "a clean file publishes an empty diagnostics list")
    h = c.hover(muri, *pos_of(main_src, "count", 1))
    check(hover_text(h) == "```ash\n(local) int count\n```", "hover on a local: %r" % hover_text(h))
    line, ch = pos_of(main_src, "count", 1)
    check(h["range"] == {"start": {"line": line, "character": ch}, "end": {"line": line, "character": ch + 5}}, "hover range covers the identifier")
    d = c.definition(muri, line, ch)
    dl, dc = pos_of(main_src, "count")
    check(d == {"uri": muri, "range": {"start": {"line": dl, "character": dc}, "end": {"line": dl, "character": dc + 5}}}, "definition of a local: %r" % d)
    h = c.hover(muri, *pos_of(main_src, "v * 2"))
    check(hover_text(h) == "```ash\n(parameter) int v\n```", "hover on a parameter: %r" % hover_text(h))
    h = c.hover(muri, *pos_of(main_src, "twice(count)"))
    check(hover_text(h) == "```ash\nstatic int twice(int v)   // in Main\n```", "hover on a call of a method of this class: %r" % hover_text(h))
    d = c.definition(muri, *pos_of(main_src, "twice(count)"))
    tl, tc = pos_of(main_src, "twice(int")
    check(d and d["uri"] == muri and d["range"]["start"] == {"line": tl, "character": tc}, "definition of a method: %r" % d)
    h = c.hover(muri, *pos_of(main_src, "label", 1))
    check(hover_text(h) == "```ash\nprivate String label   // in Main\n```", "hover on a field reached through a variable: %r" % hover_text(h))
    d = c.definition(muri, *pos_of(main_src, "label", 1))
    ll, lc = pos_of(main_src, "label")
    check(d and d["range"]["start"] == {"line": ll, "character": lc}, "definition of a field: %r" % d)
    h = c.hover(muri, *pos_of(main_src, "Main m"))
    check(hover_text(h) == "```ash\npublic class Main\n```", "hover on a class name: %r" % hover_text(h))
    h = c.hover(muri, *pos_of(main_src, "trim"))
    check(hover_text(h) == "```ash\npublic String trim()   // in ash.lang.String\n```", "hover on a library method: %r" % hover_text(h))
    check(c.definition(muri, *pos_of(main_src, "trim")) is None, "no definition for the built-in core library (it is no file)")
    # another file of the project
    d = c.definition(muri, *pos_of(main_src, "square"))
    sl, sc = pos_of(maths_src, "square")
    check(d and d["uri"] == uuri and d["range"]["start"] == {"line": sl, "character": sc}, "definition in another file: %r" % d)
    h = c.hover(muri, *pos_of(main_src, "Maths.square"))
    check(hover_text(h) == "```ash\npublic class util.Maths\n```", "hover on a class from another package: %r" % hover_text(h))
    check(c.hover(muri, 2, 3) is None, "no hover on a keyword")
    check(c.hover(muri, 999999, 0) is None and c.hover(muri, -1, 0) is None and c.hover(muri, 0, 999999) is None, "positions outside the file give null")

    # ---- an error appears, is described, and goes away when the text is fixed
    bad = main_src.replace('String s = "hi";', 'String s = "hi"; int oops = "text";')
    c.change(muri, bad, 2)
    ds = c.diagnostics(muri)
    check(len(ds) == 1 and ds[0]["severity"] == 1 and ds[0]["source"] == "ash", "one error: %r" % ds)
    l, ch = pos_of(bad, '"text"')
    check(ds and ds[0]["range"] == {"start": {"line": l, "character": ch}, "end": {"line": l, "character": ch + 6}}, "the error covers the literal: %r" % (ds[0]["range"] if ds else None))
    check(ds and "String cannot be converted to int" in ds[0]["message"], "error message")
    c.change(muri, main_src, 3)
    check(c.diagnostics(muri) == [], "fixing the text clears the error")

    # ---- ownership errors: a code, help and a note that points at the move
    own = main_src.replace("int r = twice(count)", "int[] arr = {1, 2}; Main.keep(arr); int q = arr[0]; int r = twice(count)").replace(
        "static int twice(int v)", "static int[] kept;\n    static void keep(int[] p) { kept = p; }\n    static int twice(int v)")
    c.change(muri, own, 4)
    ds = c.diagnostics(muri)
    check(len(ds) == 1 and ds[0].get("code") == "E0101", "ownership error with its code: %r" % ds)
    check(ds and "help:" in ds[0]["message"], "help lines are part of the message")
    rel = ds[0].get("relatedInformation") if ds else None
    check(rel and rel[0]["location"]["uri"] == muri and "moved" in rel[0]["message"], "the note that points at the move: %r" % rel)
    c.change(muri, main_src, 5)
    check(c.diagnostics(muri) == [], "clean again")

    # ---- an error in another file shows on that file and goes away with it
    open(uf, "w").write(maths_src.replace("n * n", 'n * "x"'))
    c.change(muri, main_src + "\n", 6)
    got = {}
    end = time.time() + 30
    while time.time() < end and uuri not in got:
        m = c.read(end - time.time())
        if m and m.get("method") == "textDocument/publishDiagnostics":
            got[m["params"]["uri"]] = m["params"]["diagnostics"]
    check(uuri in got and len(got[uuri]) >= 1, "an error in an imported file is published on that file: %r" % got)
    open(uf, "w").write(maths_src)
    c.change(muri, main_src, 7)
    got = {}
    end = time.time() + 30
    while time.time() < end and uuri not in got:
        m = c.read(end - time.time())
        if m and m.get("method") == "textDocument/publishDiagnostics":
            got[m["params"]["uri"]] = m["params"]["diagnostics"]
    check(got.get(uuri) == [], "the imported file's diagnostics are cleared when the error is gone: %r" % got)

    # ---- bad input: the server answers afterwards every time
    c.send_raw(b"Content-Length: 5\r\n\r\nhello")
    m = c.read(10)
    check(m and m.get("error", {}).get("code") == -32700, "a body that is not JSON gets a parse error: %r" % m)
    c.send_raw(b"Content-Type: x\r\n\r\n")
    c.send_raw(b"Content-Length: 2\r\n\r\n{}")
    c.send({"jsonrpc": "2.0", "id": {"nested": 1}, "method": "textDocument/hover", "params": "not an object"})
    c.send({"jsonrpc": "2.0", "id": 99, "method": "textDocument/hover", "params": {"textDocument": 5, "position": "x"}})
    m = c.read(10)
    check(m and m.get("id") == 99 and m.get("result") is None, "hover with parameters of the wrong types answers null: %r" % m)
    c.send({"jsonrpc": "2.0", "id": 100, "method": "textDocument/hover", "params": {"textDocument": {"uri": muri}, "position": {"line": "1", "character": 1.5}}})
    m = c.read(10)
    check(m and m.get("id") == 100, "hover with a string for a line answers: %r" % m)
    c.send({"jsonrpc": "2.0", "id": 101, "method": "no/such/method", "params": {}})
    m = c.read(10)
    check(m and m.get("id") == 101 and m.get("error", {}).get("code") == -32601, "an unknown request gets method-not-found: %r" % m)
    c.send({"jsonrpc": "2.0", "method": "no/such/notification", "params": {}})
    c.send({"jsonrpc": "2.0", "id": 102, "method": "textDocument/completion", "params": {"textDocument": {"uri": muri}, "position": {"line": 0, "character": 0}}})
    m = c.read(10)
    check(m and m.get("id") == 102 and m.get("result") is None, "a textDocument request we do not offer answers null: %r" % m)
    deep = b"[" * 100000
    c.send_raw(b"Content-Length: %d\r\n\r\n" % len(deep) + deep)
    m = c.read(10)
    check(m and m.get("error", {}).get("code") == -32700, "JSON nested a hundred thousand deep is refused: %r" % m)
    c.notify("textDocument/didChange", {"textDocument": {"uri": "file:///never/opened.ash", "version": 1}, "contentChanges": [{"text": "x"}]})
    c.notify("textDocument/didClose", {"textDocument": {"uri": "file:///never/opened.ash"}})
    c.notify("textDocument/didOpen", {"textDocument": {"uri": "untitled:Scratch-1", "text": "class A {}"}})
    c.notify("textDocument/didOpen", {"textDocument": 7})
    c.notify("textDocument/didChange", {"textDocument": {"uri": muri}, "contentChanges": [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}}, "text": ""}]})
    check(c.alive(muri), "the server still answers after all that")

    # ---- text the front end cannot make sense of: parse errors, binary noise, a nesting it cannot finish
    for name, text in (("parse error", "public class {{{{ )))"), ("binary noise", "\x00\x01\x02\xff\xfe class"), ("empty", ""),
                       ("unterminated string", 'class A { String s = "abc'), ("many errors", "class A {" + " int x = y;" * 400 + "}")):
        c.change(muri, text, 10)
        ds = c.diagnostics(muri)
        check(isinstance(ds, list), "text is analysed even when it is %s" % name)
        check(c.alive(muri), "the server answers after %s" % name)
    c.change(muri, main_src, 20)
    check(c.diagnostics(muri) == [], "and recovers on good text")
    nest = "public class Main { static int f() { return " + "(" * 60000 + "1" + ")" * 60000 + "; } }\n"
    t0 = time.time()
    c.change(muri, nest, 30)
    ds = c.diagnostics(muri, 60)
    check(len(ds) >= 0 and c.alive(muri), "a nesting the front end cannot finish does not take the server with it (%.1fs)" % (time.time() - t0))
    c.change(muri, main_src, 31)
    check(c.diagnostics(muri, 60) == [], "...and the next good text is analysed")

    # ---- close clears
    c.notify("textDocument/didClose", {"textDocument": {"uri": muri}})
    ds = c.diagnostics(muri)
    check(ds == [], "closing a document clears its diagnostics")
    check(c.hover(muri, 0, 0) is None, "no hover on a document that is closed")
    r, rc = c.shutdown()
    check(r.get("result") is None and rc == 0, "shutdown + exit leaves with status 0 (got %r)" % rc)

    # ---- the client going away without shutdown
    c2 = Client(root, env)
    c2.request("initialize", {"capabilities": {}})
    c2.p.stdin.close()
    check(c2.p.wait(10) == 1, "end of input without shutdown leaves with status 1")

    # ---- positions: UTF-16 by default, bytes when the client asks for UTF-8
    uni = 'public class Main {\n    static void f() {\n        String s = "é\U0001F600"; int x = "bad";\n    }\n}\n'
    open(mf, "w").write(uni)
    for enc, expect_ch in ((None, 34), ("utf-8", 37)):
        c3 = Client(root, env)
        caps3 = {"general": {"positionEncodings": [enc, "utf-16"]}} if enc else {}
        res = c3.request("initialize", {"capabilities": caps3})["result"]["capabilities"]
        check(res.get("positionEncoding") == enc, "position encoding %r is announced as %r" % (enc, res.get("positionEncoding")))
        c3.open(muri, uni)
        ds = c3.diagnostics(muri)
        check(len(ds) == 1 and ds[0]["range"]["start"]["character"] == expect_ch and ds[0]["range"]["end"]["character"] == expect_ch + 5,
              "diagnostic columns with encoding %r: %r" % (enc, ds[0]["range"] if ds else None))
        xl, xc = pos_of(uni, "x =") if enc is None else (2, uni.split("\n")[2].encode().index(b"x ="))
        hv = c3.hover(muri, xl, xc)
        check(hover_text(hv) == "```ash\n(local) int x\n```", "hover on a name after non-ASCII text with encoding %r: %r" % (enc, hover_text(hv)))
        c3.shutdown()


main()
