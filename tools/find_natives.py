#!/usr/bin/env python3
"""
finds the unreal native function table in Default.xbe and names each entry after its script class.

the table is a list of {name string, exec function, native index} entries in .data.
classes come from the decompiled scripts in scripts/ (run tools/uscript-decompile first).

usage: tools/find_natives.py [xbe] [scripts dir] [out csv]
"""
import collections
import csv
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XBE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "game" / "Default.xbe"
SCRIPTS = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "scripts"
OUT = Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT / "symbols" / "natives.csv"

DECL = re.compile(
    r"^\s*(?:[\w()]+\s+)*?native(?:\((\d+)\))?\s+(?:[\w()<>.]+\s+)*?"
    r"(?:function|event|operator\(\d+\)|preoperator|postoperator)\s+(?:[\w<>.]+\s+)?([^\s(]+)\s*\(",
    re.M,
)
CLASS = re.compile(r"^class\s+(\w+)(?:\s+extends\s+(\w+))?", re.M)


class Xbe:
    def __init__(self, path):
        self.d = path.read_bytes()
        u32 = lambda o: struct.unpack_from("<I", self.d, o)[0]
        base = u32(0x104)
        self.sections = {}
        for i in range(u32(0x11C)):
            flags, va, vsize, raw, rsize, name = struct.unpack_from("<IIIIII", self.d, u32(0x120) - base + i * 0x38)
            n = self.d[name - base:self.d.index(b"\0", name - base)].decode()
            self.sections[n] = (va, vsize, raw, rsize)

    def to_raw(self, va):
        for sva, vsize, raw, rsize in self.sections.values():
            if sva <= va < sva + rsize:
                return raw + va - sva

    def cstr(self, va):
        r = self.to_raw(va)
        if r is None:
            return None
        end = self.d.find(b"\0", r, r + 128)
        s = self.d[r:end]
        return s.decode() if end > r and all(32 <= c < 127 for c in s) else None

    def in_section(self, name, va):
        sva, vsize, _, _ = self.sections[name]
        return sva <= va < sva + vsize


def read_table(xbe):
    """returns runs of consecutive table entries as [address, name, exec address, native index]."""
    va, _, raw, rsize = xbe.sections[".data"]
    runs, cur = [], []
    for r in range(raw, raw + rsize - 12, 4):
        name_ptr, fn, index = struct.unpack_from("<III", xbe.d, r)
        if index >= 0x1000 or not xbe.in_section(".text", fn):
            continue
        name = xbe.cstr(name_ptr)
        if not name or not re.fullmatch(r"[A-Za-z_]\w*", name):
            continue
        entry = [va + r - raw, name, fn, index]
        if cur and entry[0] - cur[-1][0] != 12:
            runs.append(cur)
            cur = []
        cur.append(entry)
    runs.append(cur)
    return [run for run in runs if len(run) >= 2]


def read_scripts():
    by_name, by_index, parents = collections.defaultdict(set), {}, {}
    for f in SCRIPTS.glob("*/Classes/*.uc"):
        text = f.read_text(errors="replace")
        m = CLASS.search(text)
        if m:
            parents[m.group(1)] = m.group(2)
        for m in DECL.finditer(text):
            by_name[m.group(2)].add(f.stem)
            if m.group(1) and int(m.group(1)) > 0:
                by_index[int(m.group(1))] = f.stem
    return by_name, by_index, parents


def cpp_name(cls, parents):
    """unreal c++ prefix: A for actors, U for everything else."""
    c = cls
    while c:
        if c == "Actor":
            return "A" + cls
        c = parents.get(c)
    return "U" + cls


def main():
    xbe = Xbe(XBE)
    runs = read_table(xbe)
    by_name, by_index, parents = read_scripts()

    for run in runs:
        for e in run:
            cls = by_index.get(e[3])
            if not cls and len(by_name.get(e[1], ())) == 1:
                cls = next(iter(by_name[e[1]]))
            e.append(cls)
        # unresolved entries take the class of the nearest resolved neighbour in the same run
        for i, e in enumerate(run):
            if e[4]:
                continue
            for k in range(1, len(run)):
                hits = [run[j][4] for j in (i - k, i + k) if 0 <= j < len(run) and run[j][4]]
                hits = [h for h in hits if e[1] not in by_name or h in by_name[e[1]]]
                if hits:
                    e[4] = hits[0]
                    break

    entries = [e for run in runs for e in run]
    OUT.parent.mkdir(exist_ok=True)
    with OUT.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["address", "class", "name", "native_index", "table_entry"])
        for addr, name, fn, index, cls in sorted(entries, key=lambda e: e[2]):
            w.writerow([f"{fn:08x}", cpp_name(cls, parents) if cls else "", "exec" + name, index, f"{addr:08x}"])

    named = sum(1 for e in entries if e[4])
    print(f"{len(entries)} native table entries, {named} matched to a class, written to {OUT}")


if __name__ == "__main__":
    main()
