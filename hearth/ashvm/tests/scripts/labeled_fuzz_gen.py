#!/usr/bin/env python3
# Random Ash programs full of labeled break / continue, in two spellings of the same program:
#   labeled_fuzz_gen.py SEED label   `outer: for (...) { ... break outer; }` (the feature under test)
#   labeled_fuzz_gen.py SEED flag    the same program with flag variables and plain break / continue
# Used by labeled_fuzz.sh: both spellings must print the same checksum (the flag spelling runs on an engine
# that has no labels, and the program also runs under Java when a JDK is around).
# Loops (for, for-each, while, do-while) nest up to four deep; their bodies mix checksum updates, sole-owner
# Strings and arrays, ifs, try / catch / finally, and switches (colon and arrow form) that hold the jumps.
# `pure` programs (odd seeds) stay inside what a kiln loop region allows: no calls into project code, no try, no
# switch, so that every loop is a region with ASH_REGION_ALL=1.
import random, sys

class Gen:
    def __init__(self, seed, mode):
        self.r = random.Random(seed)
        self.mode = mode
        self.pure = seed % 2 == 1
        self.n = 0
        self.lines = []
        self.stack = []       # containers: dicts {kind: loop|switch, label, counter, pend: set of labels}
        self.vars = []        # declared sole-owner locals: dicts {name, kind S|A, depth}

    def fresh(self):
        self.n += 1
        return self.n

    def emit(self, ind, s):
        self.lines.append("    " * ind + s)

    # ---- expressions
    def counters(self):
        return [c["counter"] for c in self.stack if c["kind"] == "loop"]

    def num(self):
        cs = self.counters()
        parts = [str(self.r.randrange(1, 9))]
        if cs:
            parts.append(self.r.choice(cs))
            if self.r.random() < 0.5: parts.append(self.r.choice(cs))
        return " + ".join(parts)

    def cond(self):
        k = self.r.randrange(4)
        if k == 0: return "(%s) %% %d == %d" % (self.num(), self.r.randrange(2, 5), self.r.randrange(0, 2))
        if k == 1: return "H %% %d == %d" % (self.r.randrange(2, 6), self.r.randrange(0, 3))
        if k == 2: return "(%s) > %d" % (self.num(), self.r.randrange(3, 9))
        return "(%s) %% 3 != 0" % self.num()

    def checksum(self, ind, e=None):
        self.emit(ind, "H = (H * 31 + %s) %% 1000003L;" % (e if e else self.num()))

    # ---- statements
    def body(self, ind, depth, budget):
        mark = len(self.vars)
        for _ in range(budget):
            self.stmt(ind, depth)
        del self.vars[mark:]

    def stmt(self, ind, depth):
        r = self.r
        k = r.randrange(14)
        if k <= 2 or depth >= 4 and k <= 5:
            self.checksum(ind)
        elif k == 3:
            i = self.fresh()
            if r.random() < 0.5:
                self.emit(ind, "String s%d = \"s\" + (%s);" % (i, self.num()))
                self.vars.append({"name": "s%d" % i, "kind": "S", "depth": len(self.stack)})
            else:
                self.emit(ind, "int[] a%d = new int[%d];" % (i, r.randrange(1, 5)))
                self.emit(ind, "a%d[0] = %s;" % (i, self.num()))
                self.vars.append({"name": "a%d" % i, "kind": "A", "depth": len(self.stack)})
        elif k == 4 and self.vars:
            v = r.choice(self.vars)
            self.checksum(ind, "%s.length()" % v["name"] if v["kind"] == "S" else "%s[0] + %s.length" % (v["name"], v["name"]))
        elif k <= 7 and depth < 4:
            self.loop(ind, depth)
        elif k == 8 and self.stack:
            self.guarded_jump(ind)
        elif k == 9 and depth < 4:
            self.if_stmt(ind, depth)
        elif k == 10 and depth < 4 and not self.pure:
            self.try_stmt(ind, depth)
        elif k == 11 and depth < 4 and not self.pure:
            self.switch_stmt(ind, depth)
        elif k == 12 and self.stack:
            self.guarded_jump(ind)
        else:
            self.checksum(ind)

    def if_stmt(self, ind, depth):
        self.emit(ind, "if (%s) {" % self.cond())
        self.body(ind + 1, depth + 1, self.r.randrange(1, 3))
        if self.r.random() < 0.3:
            self.emit(ind, "} else {")
            self.body(ind + 1, depth + 1, self.r.randrange(1, 3))
        self.emit(ind, "}")

    def try_stmt(self, ind, depth):
        r = self.r
        has_catch = r.random() < 0.6
        has_fin = r.random() < 0.6 or not has_catch
        self.emit(ind, "try {")
        self.body(ind + 1, depth + 1, r.randrange(1, 3))
        if has_catch:
            self.emit(ind + 1, "if (%s) throw new RuntimeException(\"x\");" % self.cond())
        if has_catch:
            e = "e%d" % self.fresh()
            self.emit(ind, "} catch (RuntimeException %s) {" % e)
            self.checksum(ind + 1, "50 + %d" % len(e))
            self.body(ind + 1, depth + 1, r.randrange(1, 3))
        if has_fin:
            self.emit(ind, "} finally {")
            self.checksum(ind + 1, str(r.randrange(100, 200)))
        self.emit(ind, "}")

    def switch_stmt(self, ind, depth):
        r = self.r
        arrow = r.random() < 0.5
        if self.mode == "flag": arrow = False                    # the flag spelling needs `break` inside a case
        sw = {"kind": "switch", "label": None, "counter": None, "pend": []}
        self.stack.append(sw)
        self.emit(ind, "switch ((%s) %% 3) {" % self.num())
        for c in range(3):
            lab = "default" if c == 2 else "case %d" % c
            if arrow:
                self.emit(ind + 1, "%s -> {" % lab)
                self.body(ind + 2, depth + 1, r.randrange(1, 3))
                self.emit(ind + 1, "}")
            else:
                self.emit(ind + 1, "%s:" % lab)
                self.body(ind + 2, depth + 1, r.randrange(1, 3))
                self.emit(ind + 2, "break;")
        self.emit(ind, "}")
        self.stack.pop()
        self.after(ind, self.stack[-1] if self.stack else None, sw["pend"])

    # ---- loops
    def loop(self, ind, depth):
        r = self.r
        i = self.fresh()
        kind = r.choice(["for", "foreach", "while", "do"])
        labeled = r.random() < 0.6
        label = "L%d" % i if labeled else None
        ctr = {"for": "i%d", "foreach": "x%d", "while": "w%d", "do": "d%d"}[kind] % i
        c = {"kind": "loop", "label": label, "counter": ctr, "pend": [], "ltype": kind}
        if labeled and self.mode == "flag":
            self.emit(ind, "int f%s = 0;" % label)
        if kind == "while": self.emit(ind, "int %s = 0;" % ctr)
        if kind == "do": self.emit(ind, "int %s = 0;" % ctr)
        pre = (label + ": ") if labeled and self.mode == "label" else ""
        b = r.randrange(2, 5)
        if kind == "for": self.emit(ind, "%sfor (int %s = 0; %s < %d; %s++) {" % (pre, ctr, ctr, b, ctr))
        elif kind == "foreach": self.emit(ind, "%sfor (int %s : AR) {" % (pre, ctr))
        elif kind == "while":
            self.emit(ind, "%swhile (%s < %d) {" % (pre, ctr, b))
            self.emit(ind + 1, "%s++;" % ctr)
        else:
            self.emit(ind, "%sdo {" % pre)
            self.emit(ind + 1, "%s++;" % ctr)
        self.stack.append(c)
        self.body(ind + 1, depth + 1, r.randrange(2, 5))
        self.stack.pop()
        self.emit(ind, ("} while (%s < %d);" % (ctr, b)) if kind == "do" else "}")
        self.after(ind, self.stack[-1] if self.stack else None, c["pend"])

    # after a container ended: pass on the labeled jumps that crossed it (flag spelling only)
    def after(self, ind, parent, pend):
        if self.mode != "flag":
            return
        for lab in pend:
            if parent and parent["label"] == lab:
                self.emit(ind, "if (f%s == 1) {" % lab)
                self.emit(ind + 1, "break;")
                self.emit(ind, "}")
                self.emit(ind, "if (f%s == 2) {" % lab)
                self.emit(ind + 1, "f%s = 0;" % lab)
                self.emit(ind + 1, "continue;")
                self.emit(ind, "}")
            else:
                self.emit(ind, "if (f%s != 0) {" % lab)
                self.emit(ind + 1, "break;")
                self.emit(ind, "}")
                if parent and lab not in parent["pend"]: parent["pend"].append(lab)

    def guarded_jump(self, ind):
        r = self.r
        labeled = [c for c in self.stack if c["kind"] == "loop" and c["label"]]
        loops = [c for c in self.stack if c["kind"] == "loop"]
        if not loops: return
        # innermost container is a switch: only a labeled jump leaves it the way we mean
        if self.stack[-1]["kind"] == "switch" and not labeled: return
        tgt = r.choice(labeled) if labeled and (r.random() < 0.8 or self.stack[-1]["kind"] == "switch") else None
        if tgt is None:
            tgt = [c for c in self.stack if c["kind"] == "loop"][-1]
            if self.stack[-1]["kind"] == "switch": return
            # an unlabeled jump to the innermost loop (no switch between): plain break / continue
        cont = r.random() < 0.5
        ti = self.stack.index(tgt)
        self.emit(ind, "if (%s) {" % self.cond())
        mv = [v for v in self.vars if v["depth"] >= ti + 1]
        for v in self.vars:
            if r.random() < 0.4:
                self.checksum(ind + 1, "%s.length()" % v["name"] if v["kind"] == "S" else "%s[0] + %s.length" % (v["name"], v["name"]))
        if mv and not self.pure and r.random() < 0.4:
            v = r.choice(mv)
            self.emit(ind + 1, "hold%s(%s);" % (v["kind"], v["name"]))
        word = "continue" if cont else "break"
        innermost = self.stack[-1]
        if self.mode == "label" and tgt["label"]:
            self.emit(ind + 1, "%s %s;" % (word, tgt["label"]))
        elif innermost is tgt or tgt["label"] is None:
            self.emit(ind + 1, "%s;" % word)
        else:
            self.emit(ind + 1, "f%s = %d;" % (tgt["label"], 2 if cont else 1))
            self.emit(ind + 1, "break;")
            if tgt["label"] not in innermost["pend"]: innermost["pend"].append(tgt["label"])
        self.emit(ind, "}")
        # a plain jump must stay the last statement of its block: the caller never adds one after an `if`'s jump

NMETH = 8        # small methods: ashvm keeps a function's constants in a table of 256

def program(seed, mode):
    g = Gen(seed, mode)
    methods = []
    for m in range(NMETH):
        g.lines = []
        g.stack = []
        g.vars = []
        g.body(2, 0, 2)
        methods.append("\n".join(g.lines))
    out = ["public class F%d {" % seed,
           "    static long H = 7;",
           "    static int[] AR = {3, 1, 4};",
           "    static String KS;",
           "    static int[] KA;",
           "    static void holdS(String s) { KS = s; }",
           "    static void holdA(int[] a) { KA = a; }"]
    for i, body in enumerate(methods):
        out.append("    static void m%d() {" % i)
        out.append(body)
        out.append("    }")
    out.append("    public static void main(String[] args) {")
    for i in range(NMETH):
        out.append("        m%d();" % i)
        out.append("        System.out.println(H);")
    out.append("    }")
    out.append("}")
    return "\n".join(out) + "\n"

if __name__ == "__main__":
    print(program(int(sys.argv[1]), sys.argv[2]), end="")
