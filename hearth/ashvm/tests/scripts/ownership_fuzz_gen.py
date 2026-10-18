#!/usr/bin/env python3
# Random Ash programs that mix ownership patterns (aliasing, moves, factories, containers, try/finally, loops).
# Used by ownership_fuzz.sh: the output of a program must be identical with memory-safety on and off.
import random, sys

HELPERS = '''
    static String SK;                    // statics that keep values alive
    static int[] AK;
    static List<String> LK;
    static Box BK;
    static Map<String, Integer> MK;
    static int rdS(String s) { return s.length(); }
    static int rdA(int[] a) { int t = 0; for (int x : a) t += x; return t; }
    static int rdL(List<String> l) { int t = 0; for (int i = 0; i < l.size(); i++) t += l.get(i).length(); return t; }
    static int rdB(Box b) { return b.n.length() + b.d.length; }
    static int rdM(Map<String, Integer> m) { return m.size() + (m.containsKey("k") ? 1 : 0); }
    static void keepS(String s) { SK = s; }
    static void keepA(int[] a) { AK = a; }
    static void keepL(List<String> l) { LK = l; }
    static void keepB(Box b) { BK = b; }
    static void keepM(Map<String, Integer> m) { MK = m; }
    static String retS(String s) { return s; }
    static int[] retA(int[] a) { return a; }
    static List<String> retL(List<String> l) { return l; }
    static Box retB(Box b) { return b; }
    static String mkS(int i) { String s = "m" + i; return s; }
    static int[] mkA(int i) { int[] a = new int[3]; a[0] = i; return a; }
    static List<String> mkL(int i) { List<String> l = new ArrayList<>(); l.add("e" + i); return l; }
    static Box mkB(int i) { Box b = new Box("b" + i); return b; }
    static String pickS(bool c, String a) { if (c) return a; String t = "alt" + a; return t; }
    static int deep(String s, int d) { return d == 0 ? s.length() : deep(s, d - 1) + 1; }
'''

CLASSES = '''
    static class Box {
        String n;
        int[] d = new int[2];
        List<String> items = new ArrayList<>();
        Box(String n) { d[0] = n.length(); items.add("i" + n); this.n = n; }
        int size() { return n.length() + d[0] + items.size(); }
        String name() { return n; }
        void rename(String x) { n = "r" + x; }
        List<String> list() { return items; }
    }
'''

class Gen:
    def __init__(self, seed):
        self.r = random.Random(seed)
        self.n = 0
        self.lines = []
        self.out_i = 0

    def fresh(self):
        self.n += 1
        return self.n

    def emit(self, ind, s):
        self.lines.append("    " * ind + s)

    def stmt_block(self, ind, vars, depth, budget):
        vars = dict(vars)          # name -> kind, usable (not moved)
        for _ in range(budget):
            self.stmt(ind, vars, depth)
        return vars

    def pick(self, vars, kinds=None):
        c = [v for v, k in vars.items() if kinds is None or k in kinds]
        return self.r.choice(c) if c else None

    def use(self, ind, v, k):
        # a read of v that would show corruption
        e = {"S": f"{v}.length()", "A": f"rdA({v})", "L": f"rdL({v})", "B": f"rdB({v})", "M": f"rdM({v})"}[k]
        self.emit(ind, f"total += {e};")

    def stmt(self, ind, vars, depth):
        r = self.r
        op = r.randrange(30)
        if op <= 3:   # declare something fresh
            i = self.fresh()
            k = r.choice("SALBM")
            v = f"v{i}"
            if k == "S":
                self.emit(ind, f'String {v} = "p" + {i} + total;')
            elif k == "A":
                self.emit(ind, f"int[] {v} = new int[{r.randrange(1, 5)}];"); self.emit(ind, f"{v}[0] = {i};")
            elif k == "L":
                self.emit(ind, f"List<String> {v} = new ArrayList<>();"); self.emit(ind, f'{v}.add("x" + {i});')
            elif k == "B":
                self.emit(ind, f'Box {v} = new Box("n" + {i});')
            else:
                self.emit(ind, f"Map<String, Integer> {v} = new HashMap<>();"); self.emit(ind, f'{v}.put("k", {i});')
            vars[v] = k
        elif op == 4:  # from a factory
            i = self.fresh(); k = r.choice("SALB"); v = f"v{i}"
            self.emit(ind, {"S": f"String {v} = mkS({i});", "A": f"int[] {v} = mkA({i});",
                            "L": f"List<String> {v} = mkL({i});", "B": f"Box {v} = mkB({i});"}[k])
            vars[v] = k
        elif op <= 7:  # read something
            v = self.pick(vars)
            if v: self.use(ind, v, vars[v])
        elif op == 8:  # call a reader helper
            v = self.pick(vars)
            if v:
                k = vars[v]
                self.emit(ind, f"total += rd{k}({v});")
        elif op == 9:  # keep it (moves in on-mode)
            v = self.pick(vars, "SALBM")
            if v:
                k = vars[v]
                self.emit(ind, f"keep{k}({v});")
                del vars[v]
        elif op == 10:  # return-it helper then use result
            v = self.pick(vars, "SALB")
            if v:
                k = vars[v]; i = self.fresh(); w = f"v{i}"
                ty = {"S": "String", "A": "int[]", "L": "List<String>", "B": "Box"}[k]
                self.emit(ind, f"{ty} {w} = ret{k}({v});")
                del vars[v]; vars[w] = k
        elif op == 11:  # alias
            v = self.pick(vars)
            if v:
                k = vars[v]; i = self.fresh(); w = f"v{i}"
                ty = {"S": "String", "A": "int[]", "L": "List<String>", "B": "Box", "M": "Map<String, Integer>"}[k]
                self.emit(ind, f"{ty} {w} = {v};")
                del vars[v]; vars[w] = k
        elif op == 12 and depth < 2:  # loop
            i = self.fresh()
            self.emit(ind, f"for (int j{i} = 0; j{i} < {r.randrange(1, 4)}; j{i}++) {{")
            inner = self.stmt_block(ind + 1, vars, depth + 1, r.randrange(1, 5))
            self.emit(ind, "}")
            # variables declared inside are gone; outer ones may have been moved inside the loop: drop any moved
            for v in list(vars):
                if v not in inner: del vars[v]
        elif op == 13 and depth < 2:  # if / else
            self.emit(ind, f"if (total % {r.randrange(2, 5)} == 0) {{")
            a = self.stmt_block(ind + 1, vars, depth + 1, r.randrange(1, 4))
            self.emit(ind, "} else {")
            b = self.stmt_block(ind + 1, vars, depth + 1, r.randrange(1, 4))
            self.emit(ind, "}")
            for v in list(vars):
                if v not in a or v not in b: del vars[v]
        elif op == 14:  # string work
            v = self.pick(vars, "S")
            if v:
                i = self.fresh(); w = f"v{i}"
                self.emit(ind, f'String {w} = {v} + "!" + total;')
                vars[w] = "S"
        elif op == 16:  # hand out a field of a Box
            v = self.pick(vars, "B")
            if v:
                c = r.randrange(4)
                if c == 0: self.emit(ind, f"keepL({v}.list());")
                elif c == 1: self.emit(ind, f"keepS({v}.name());")
                elif c == 2: self.emit(ind, f"keepA({v}.d);")
                else: self.emit(ind, f"keepL({v}.items);")
        elif op == 17:  # put a value into a container
            a = self.pick(vars, "S"); l = self.pick(vars, "L")
            if a and l:
                self.emit(ind, f"{l}.add({a});"); del vars[a]
        elif op == 18:  # store a string into a Box's list
            a = self.pick(vars, "S"); b = self.pick(vars, "B")
            if a and b:
                self.emit(ind, f"{b}.items.add({a});"); del vars[a]
        elif op == 19:  # string array of fresh and shared strings
            i = self.fresh(); w = f"v{i}"
            a = self.pick(vars, "S")
            self.emit(ind, f"String[] {w} = new String[2];")
            self.emit(ind, f'{w}[0] = "q" + {i};')
            if a: self.emit(ind, f"{w}[1] = {a};"); del vars[a]
            else: self.emit(ind, f'{w}[1] = "z" + {i};')
            self.emit(ind, f"total += {w}[0].length() + {w}[1].length();")
        elif op == 20 and depth < 2:  # loop with continue / break
            i = self.fresh()
            self.emit(ind, f"for (int j{i} = 0; j{i} < 4; j{i}++) {{")
            self.emit(ind + 1, f"String t{i} = \"t\" + j{i};")
            self.emit(ind + 1, f"if (j{i} == 1) continue;")
            self.emit(ind + 1, f"total += t{i}.length();")
            self.emit(ind + 1, f"if (j{i} == 2) break;")
            self.emit(ind, "}")
        elif op == 21 and depth < 2:  # try / finally touching outer locals
            v = self.pick(vars, "SALB")
            if v:
                k = vars[v]
                e = {"S": f"{v}.length()", "A": f"rdA({v})", "L": f"rdL({v})", "B": f"rdB({v})"}[k]
                self.emit(ind, "try {")
                self.emit(ind + 1, f"total += {e};")
                self.emit(ind + 1, f"if (total % 3 == 0) throw new RuntimeException(\"x\");")
                self.emit(ind, "} catch (RuntimeException e) {")
                self.emit(ind + 1, f"total += {e} + 1;")
                self.emit(ind, "} finally {")
                self.emit(ind + 1, f"total += {e};")
                self.emit(ind, "}")
        elif op == 22:  # reassign a string local
            v = self.pick(vars, "S")
            if v: self.emit(ind, f'{v} = {v} + "+" + total;')
        elif op == 23:  # conditional helper returning one of two
            v = self.pick(vars, "S")
            if v:
                i = self.fresh(); w = f"v{i}"
                self.emit(ind, f"String {w} = pickS(total % 2 == 0, {v});")
                del vars[v]; vars[w] = "S"
        elif op == 24:  # fresh element, then read it in harmless and in escaping ways
            l = self.pick(vars, "L")
            if l:
                i = self.fresh()
                self.emit(ind, f'{l}.add("e" + {i} + total);')
                c = r.randrange(5)
                if c == 0: self.emit(ind, f"total += {l}.get(0).length();")
                elif c == 1: self.emit(ind, f"keepS({l}.get(0));")
                elif c == 2: self.emit(ind, f"String w{i} = {l}.get({l}.size() - 1); total += w{i}.length();")
                elif c == 3: self.emit(ind, f"for (String x{i} : {l}) {{ total += x{i}.length(); }}")
                else: self.emit(ind, f"for (String x{i} : {l}) {{ if (total % 5 == 0) SK = x{i}; }}")
        elif op == 25:  # String[] with fresh / shared stores and reads
            i = self.fresh(); w = f"v{i}"
            self.emit(ind, f"String[] {w} = new String[3];")
            self.emit(ind, f'{w}[0] = "a" + {i} + total;')
            self.emit(ind, f'{w}[1] = "b" + {i};')
            c = r.randrange(5)
            if c == 0: self.emit(ind, f'{w}[2] = "lit";')
            elif c == 1: self.emit(ind, f'{w}[2] = {w}[0];')
            elif c == 2: self.emit(ind, f'{w}[2] = "c" + {i};')
            elif c == 3: self.emit(ind, f"keepS({w}[1]);")
            else: self.emit(ind, f"total += {w}[0].length();")
            self.emit(ind, f"total += {w}[1].length();")
        elif op == 26:  # a list of strings stored in a field and read later
            b = self.pick(vars, "B")
            if b:
                i = self.fresh()
                self.emit(ind, f'{b}.items.add("it" + {i} + total);')
                if r.randrange(2): self.emit(ind, f"total += {b}.items.get(0).length();")
                else: self.emit(ind, f"keepS({b}.items.get(0));")
        elif op == 27:  # ternary / conditional use of an element
            l = self.pick(vars, "L")
            if l: self.emit(ind, f'total += ({l}.size() > 0 ? {l}.get(0) : "none").length();')
        elif op == 28:  # remove / set return the old element
            l = self.pick(vars, "L")
            if l:
                i = self.fresh()
                self.emit(ind, f'{l}.add("r" + {i});')
                self.emit(ind, f"String o{i} = {l}.remove({l}.size() - 1); total += o{i}.length();")
        elif op == 29:  # string array from a loop
            i = self.fresh(); w = f"v{i}"
            self.emit(ind, f"String[] {w} = new String[3];")
            self.emit(ind, f'for (int q{i} = 0; q{i} < 3; q{i}++) {{ {w}[q{i}] = "s" + q{i} + total; }}')
            self.emit(ind, f"for (String y{i} : {w}) {{ total += y{i}.length(); }}")
        elif op == 15:  # box mutation / field reads
            v = self.pick(vars, "B")
            if v:
                c = r.randrange(3)
                if c == 0: self.emit(ind, f'{v}.rename("{r.randrange(100)}");')
                elif c == 1: self.emit(ind, f"total += {v}.size();")
                else:
                    i = self.fresh(); w = f"v{i}"
                    self.emit(ind, f"String {w} = {v}.name();"); self.emit(ind, f"total += {w}.length();")
                    # w aliases a field: only read it right here

    def program(self, name):
        self.emit(1, "public static void main(String[] args) {")
        self.emit(2, "long total = 0;")
        vars = {}
        self.stmt_block(2, vars, 0, self.r.randrange(6, 18))
        # observe everything that survived
        for v, k in vars.items(): self.use(2, v, k)
        self.emit(2, 'String? sk = SK; if (sk != null) total += sk.length();')
        self.emit(2, 'int[]? ak = AK; if (ak != null) total += rdA(ak);')
        self.emit(2, 'List<String>? lk = LK; if (lk != null) total += rdL(lk);')
        self.emit(2, 'Box? bk = BK; if (bk != null) total += rdB(bk);')
        self.emit(2, 'Map<String, Integer>? mk = MK; if (mk != null) total += rdM(mk);')
        self.emit(2, "System.out.println(total);")
        self.emit(1, "}")
        body = "\n".join(self.lines)
        helpers = HELPERS.replace("static String SK;", "static String? SK;").replace("static int[] AK;", "static int[]? AK;") \
            .replace("static List<String> LK;", "static List<String>? LK;").replace("static Box BK;", "static Box? BK;") \
            .replace("static Map<String, Integer> MK;", "static Map<String, Integer>? MK;")
        return f"""import ash.util.ArrayList;
import ash.util.HashMap;
import ash.util.List;
import ash.util.Map;

public class {name} {{
{CLASSES}
{helpers}
{body}
}}
"""

if __name__ == "__main__":
    seed = int(sys.argv[1]); name = f"F{seed}"
    sys.stdout.write(Gen(seed).program(name))
