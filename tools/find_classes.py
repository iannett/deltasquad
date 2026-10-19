#!/usr/bin/env python3
"""
finds every native class registration in Default.xbe and writes symbols/classes.csv.

each native class is a static UClass built at startup by a call to the UClass native constructor:
UClass(ENativeConstructor, Size, ClassFlags, Super, Within, Guid, Name, Package, Config, Flags,
       InternalConstructor, StaticConstructor)
this walks the disassembly of every caller and recovers the pushed arguments.

usage: tools/find_classes.py
"""
import csv
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XBE = ROOT / "game" / "Default.xbe"
OUT = ROOT / "symbols" / "classes.csv"
UCLASS_CTOR = "0x0009a85b"

# argument order of the UClass native constructor after the guid
ARGS = ["Name", "Package", "Config", "Flags", "InternalConstructor", "StaticConstructor"]


def ghidra(script, *args):
    out = subprocess.run([str(ROOT / "tools" / "ghidra.sh"), script, *args], capture_output=True, text=True, check=True)
    return out.stdout.splitlines()


def xbe_strings():
    d = XBE.read_bytes()
    u32 = lambda o: struct.unpack_from("<I", d, o)[0]
    base = u32(0x104)
    sections = []
    for i in range(u32(0x11C)):
        _, va, _, raw, rsize, _ = struct.unpack_from("<IIIIII", d, u32(0x120) - base + i * 0x38)
        sections.append((va, rsize, raw))

    def cstr(va):
        for sva, rsize, raw in sections:
            if sva <= va < sva + rsize:
                r = raw + va - sva
                return d[r:d.index(b"\0", r)].decode("latin-1")
        return ""

    return cstr


def parse(lines, cstr):
    """returns one dict per registration found in a disassembly listing."""
    classes = []
    regs, pushes = {}, []
    for line in lines:
        if line.startswith("@@"):
            regs, pushes = {}, []
            continue
        op, _, args = line.split("  ", 1)[1].split("  ;", 1)[0].partition(" ")
        a = [x.strip() for x in args.split(",")] if args else []

        def value(x):
            if x in regs:
                return regs[x]
            try:
                return int(x, 16) if x.startswith("0x") else int(x)
            except ValueError:
                return None

        if op == "MOV" and len(a) == 2 and a[0] in ("EAX", "ECX", "EDX", "EBX", "ESI", "EDI"):
            regs[a[0]] = value(a[1])
        elif op == "XOR" and len(a) == 2 and a[0] == a[1]:
            regs[a[0]] = 0
        elif op == "PUSH":
            pushes.append(value(a[0]))
        elif op == "CALL" and a[0] == UCLASS_CTOR:
            # pushes, last first: native marker, size, class flags, super, within; the guid is copied in between
            first = pushes[-5:][::-1]
            rest = pushes[:-5][-6:][::-1]
            cls = {
                "Address": regs.get("ECX"),
                "Size": first[1],
                "ClassFlags": first[2],
                "Super": first[3],
                "Within": first[4],
            }
            # the six string/function arguments were pushed before the guid, in reverse order
            for name, val in zip(ARGS, rest[::-1][::-1]):
                cls[name] = val
            cls["Name"] = cstr(cls["Name"]) if cls["Name"] else ""
            cls["Package"] = cstr(cls["Package"]) if cls["Package"] else ""
            cls["Config"] = cstr(cls["Config"]) if cls["Config"] else ""
            classes.append(cls)
            regs, pushes = {}, []
        elif op == "CALL":
            pushes = []
    return classes


def cpp_name(cls, by_addr):
    c = cls
    while c:
        if c["Name"] == "Actor":
            return "A" + cls["Name"]
        c = by_addr.get(c["Super"]) if c["Super"] != c["Address"] else None
    return "U" + cls["Name"]


def main():
    callers = [l.split("@ ")[1].strip() for l in ghidra("Callers.java", UCLASS_CTOR[2:]) if l.startswith(" ") and "@ " in l]
    classes = parse(ghidra("Disasm.java", *callers), xbe_strings())
    by_addr = {c["Address"]: c for c in classes}

    OUT.parent.mkdir(exist_ok=True)
    with OUT.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["address", "cpp_name", "name", "package", "super", "within", "size", "class_flags", "flags",
                    "config", "internal_constructor", "static_constructor"])
        for c in sorted(classes, key=lambda c: (c["Package"], c["Name"])):
            sup = by_addr.get(c["Super"])
            within = by_addr.get(c["Within"])
            w.writerow([
                f"{c['Address']:08x}", cpp_name(c, by_addr), c["Name"], c["Package"],
                sup["Name"] if sup and sup is not c else "", within["Name"] if within else "",
                c["Size"], f"{c['ClassFlags']:#x}", f"{c['Flags']:#x}", c["Config"],
                f"{c['InternalConstructor']:08x}", f"{c['StaticConstructor']:08x}",
            ])
    print(f"{len(classes)} native classes written to {OUT}")



def find_vtables():
    """follows each internal constructor to the class constructor and reads the vtable it stores."""
    rows = [{k: v for k, v in r.items() if k not in ("constructor", "vtable")} for r in csv.DictReader(OUT.open())]
    shared = {r["internal_constructor"] for r in rows if sum(x["internal_constructor"] == r["internal_constructor"] for x in rows) > 1}
    internal = sorted({r["internal_constructor"] for r in rows} - shared)

    def vtable_store(line):
        if "MOV dword ptr [E" not in line or "],0x" not in line or "+" in line.split("[")[1].split("]")[0]:
            return None
        return line.rsplit("0x", 1)[1].strip().zfill(8)

    # the internal constructor either jumps to the class constructor, or has it inlined:
    # a call to the parent constructor followed by the store of this class's vtable
    ctor_of, vtable_of = {}, {}
    current = None
    for line in ghidra("Disasm.java", *internal):
        if line.startswith("@@"):
            current = line.split("@ ")[-1].strip()
            continue
        store = vtable_store(line)
        if store:
            ctor_of[current] = current
            vtable_of[current] = store
        elif " JMP 0x" in line and current not in ctor_of:
            ctor_of[current] = line.split("0x")[-1].strip()[-8:]

    current = None
    for line in ghidra("Disasm.java", *sorted({c for i, c in ctor_of.items() if c != i})):
        if line.startswith("@@"):
            current = line.split("@ ")[-1].strip()
        elif current and current not in vtable_of and vtable_store(line):
            vtable_of[current] = vtable_store(line)

    with OUT.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) + ["constructor", "vtable"])
        w.writeheader()
        for r in rows:
            ctor = ctor_of.get(r["internal_constructor"], "")
            w.writerow({**r, "constructor": ctor, "vtable": vtable_of.get(ctor, "")})
    print(f"{sum(1 for r in rows if ctor_of.get(r['internal_constructor']))} constructors, {len(vtable_of)} vtables")


if __name__ == "__main__":
    main()
    find_vtables()
