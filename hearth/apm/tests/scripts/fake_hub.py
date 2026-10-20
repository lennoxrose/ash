#!/usr/bin/env python3
"""A stand-in for the APMH hub for apm's offline tests: prints its port, then serves the bits of /api/v1 apm uses.
Packages are the bare git repos under $FAKE_GIT_DIR/<owner>/<repo>.git; every request is appended to $FAKE_LOG as
"METHOD path auth=yes|no". Run: fake_hub.py <git dir> <log file>."""
import json, subprocess, sys
from http.server import BaseHTTPRequestHandler, HTTPServer

GIT, LOG = sys.argv[1], sys.argv[2]
TOKEN = "apm:" + "A" * 86
state = {"polls": 0, "linked": set()}

def git(repo, *args):
    return subprocess.run(["git", "-C", f"{GIT}/{repo}.git", *args], capture_output=True, text=True).stdout.strip()

def tags(repo):
    return [t for t in git(repo, "tag", "--sort=-v:refname").split("\n") if t]

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body=None, raw=None):
        data = raw if raw is not None else (json.dumps({"schema": "apmh.v1", **body}).encode() if body is not None else b"")
        self.send_response(code); self.send_header("content-type", "application/json"); self.send_header("content-length", str(len(data))); self.end_headers(); self.wfile.write(data)
    def err(self, code, error, message=""): self.send(code, {"error": error, "message": message})
    def authed(self): return self.headers.get("authorization") == f"Bearer {TOKEN}"
    def body(self):
        n = int(self.headers.get("content-length") or 0)
        return json.loads(self.rfile.read(n) or b"{}") if n else {}
    def handle_any(self, method):
        path = self.path.split("?")[0]
        with open(LOG, "a") as f: f.write(f"{method} {self.path} auth={'yes' if self.headers.get('authorization') else 'no'}\n")
        parts = path.strip("/").split("/")  # api v1 ...
        if path == "/api/v1/auth/device" and method == "POST":
            self.body(); return self.send(200, {"device_code": "d" * 43, "user_code": "BCDF-GHJK", "verification_uri": "http://hub.test/login/device", "expires_in": 600, "interval": 1, "scopes": ["packages:write"]})
        if path == "/api/v1/auth/device/token" and method == "POST":
            state["polls"] += 1
            if state["polls"] == 1: return self.err(400, "authorization_pending")
            if state["polls"] == 2: return self.err(400, "slow_down")
            return self.send(200, {"token": TOKEN, "scopes": ["packages:read", "packages:write"], "expiresAt": "2027-01-18T10:00:00.000Z", "login": "alice"})
        if path == "/api/v1/me/token":
            if not self.authed(): return self.err(401, "unauthorized")
            return self.send(200, {"token": {"login": "alice", "name": "ci", "hint": "AAAA", "scopes": ["packages:read", "packages:write"], "createdAt": "2026-10-20T10:00:00.000Z", "expiresAt": None}})
        if parts[:3] == ["api", "v1", "me"] and len(parts) >= 4 and parts[3] == "packages":
            if not self.authed(): return self.err(401, "unauthorized")
            if len(parts) == 4: return self.send(200, {"packages": [{"owner": "alice", "repo": r, "version": "1.0.0", "status": "ACTIVE", "validity": "VALID"} for r in sorted(state["linked"])]})
            repo = parts[5]
            pkg = {"package": {"owner": parts[4], "repo": repo, "version": "1.0.0", "status": "ACTIVE", "validity": "VALID"}}
            if method == "PUT": existed = repo in state["linked"]; state["linked"].add(repo); return self.send(200 if existed else 201, pkg)
            if method == "DELETE": state["linked"].discard(repo); return self.send(204)
            if method == "POST": return self.send(200, pkg)
        if parts[:3] == ["api", "v1", "search"]: return self.send(200, {"total": 1, "items": [{"owner": "alice", "repo": "greeter", "version": "1.0.0", "description": "says hi"}], "nextCursor": None})
        if len(parts) >= 5 and parts[2] == "packages" and parts[-1] == "versions":
            t = tags(f"{parts[3]}/{parts[4]}")
            return self.send(200, {"owner": parts[3], "repo": parts[4], "latest": t[0] if t else None, "versions": []}) if git(f"{parts[3]}/{parts[4]}", "rev-parse", "HEAD") else self.err(404, "not_found")
        if len(parts) == 5 and parts[2] == "packages":
            return self.send(200, {"package": {"owner": parts[3], "repo": parts[4], "version": "1.0.0", "description": "says hi", "license": "MIT", "github": f"https://github.com/{parts[3]}/{parts[4]}", "latestVersion": "v1.0.0", "warnings": []}})
        if len(parts) == 5 and parts[2] == "resolve":
            owner, repo = parts[3], parts[4]
            if owner == "evil": # hostile answers: a repository name with shell syntax, nesting bombs, oversized bodies
                if repo == "shell": return self.send(200, {"github": "https://github.com/$(touch pwned)/x", "ref": {"type": "branch", "name": "main", "commit": "a" * 40}, "warnings": []})
                if repo == "deep": return self.send(200, raw=b"[" * 500 + b"]" * 500)
                if repo == "big": return self.send(200, raw=b'{"x":"' + b"a" * 2_000_000 + b'"}')
                if repo == "host": return self.send(200, {"github": "https://evil.example/o/r", "ref": {"type": "branch", "name": "main", "commit": "a" * 40}, "warnings": []})
            if not git(f"{owner}/{repo}", "rev-parse", "HEAD"): return self.err(404, "not_found", "No such package.")
            want = (self.path.split("version=")[1] if "version=" in self.path else "")
            ref = want if want else git(f"{owner}/{repo}", "symbolic-ref", "--short", "HEAD")
            commit = git(f"{owner}/{repo}", "rev-parse", ref + "^{commit}")
            if not commit: return self.err(404, "not_found", "No such version.")
            return self.send(200, {"owner": owner, "repo": repo, "github": f"https://github.com/{owner}/{repo}", "ref": {"type": "tag" if want else "branch", "name": ref, "commit": commit}, "manifest": {}, "warnings": []})
        self.err(404, "not_found")
    do_GET = lambda s: s.handle_any("GET")
    do_POST = lambda s: s.handle_any("POST")
    do_PUT = lambda s: s.handle_any("PUT")
    do_DELETE = lambda s: s.handle_any("DELETE")

srv = HTTPServer(("127.0.0.1", 0), H)
print(srv.server_port, flush=True)
srv.serve_forever()
