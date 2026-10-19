#!/usr/bin/env python3
"""
names virtual function overrides in every class vtable and writes symbols/virtuals.csv.

a slot is named Class::Function when its target differs from the parent class's vtable at the same
slot, i.e. the class overrides it there. slot names come from SLOTS below, filled in as each base
class's virtuals are identified. targets shared by unrelated slots (empty stubs) are left alone.

usage: tools/name_virtuals.py
"""
import collections
import csv
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XBE = ROOT / "game" / "Default.xbe"
CLASSES = ROOT / "symbols" / "classes.csv"
OUT = ROOT / "symbols" / "virtuals.csv"

# vtable slot names per base class: (first slot offset, names). orders were worked out from behaviour
# and match the virtuals the pc Core.dll exports.
SLOTS = {
    "UObject": (0x00, [
        "~UObject", "ProcessEvent", "ProcessDelegate", "ProcessState", "ProcessRemoteFunction",
        "Modify", "PostLoad", "Destroy", "Serialize", "IsPendingKill", "IsInState", "GotoState",
        "GotoLabel", "InitExecution", "ShutdownAfterError", "PostEditChange", "PreEditUndo",
        "PostEditUndo", "CallFunction", "ScriptConsoleExec", "Register", "LanguageChange",
        "GetDescription", "DebugOutput", "DebugOutputSelf", "DebugWindowSizeGet", "Rename",
        "NetDirty", "IsRuntimeStatic", "IsDefaultValue",
    ]),
    "UField": (0x78, ["AddCppProperty", "MergeBools", "Bind", "GetOwnerClass", "GetPropertiesSize"]),
    "UStruct": (0x8C, [
        "GetInheritanceSuper", "Link", "SerializeBin", "SerializeTaggedProperties", "CleanupDestroyed",
        "SerializeExpr", "GetNameCPP",
    ]),
    "UProperty": (0x8C, [
        "GetElementSize", "GetAlign", "CleanupDestroyed", "Link", "Identical", "GetAddress", "ExportCpp",
        "ExportCppItem", "SerializeItem", "NetSerializeItem", "ExportTextItem", "ImportText", "ExportText",
        "CopySingleValue", "CopyCompleteValue", "DestroyValue", "Port", "GetID",
    ]),
}


# vtables of classes whose constructor is shared, found by hand
KNOWN_VTABLES = {
    "UObject": 0x003B4E30,
}


def read_xbe():
    d = XBE.read_bytes()
    u32 = lambda o: struct.unpack_from("<I", d, o)[0]
    base = u32(0x104)
    sections = {}
    for i in range(u32(0x11C)):
        _, va, vsize, raw, rsize, name = struct.unpack_from("<IIIIII", d, u32(0x120) - base + i * 0x38)
        sections[d[name - base:d.index(b"\0", name - base)].decode()] = (va, vsize, raw, rsize)

    def read(va):
        for sva, _, raw, rsize in sections.values():
            if sva <= va < sva + rsize:
                return u32(raw + va - sva)

    text_va, text_size, _, _ = sections[".text"]
    return read, lambda va: text_va <= va < text_va + text_size


def main():
    read, in_text = read_xbe()
    classes = {r["cpp_name"]: r for r in csv.DictReader(CLASSES.open())}
    by_name = {r["name"]: r["cpp_name"] for r in classes.values()}

    def slot_names(cpp):
        """slot names inherited down the parent chain, closest base first."""
        chain, c = [], cpp
        while c in classes:
            chain.append(c)
            parent = classes[c]["super"]
            c = by_name.get(parent) if parent else None
        names = {}
        for base in reversed(chain):
            start, slots = SLOTS.get(base, (0, []))
            for i, name in enumerate(slots):
                names[start + i * 4] = name
        return names

    def vtable(cpp):
        if cpp in KNOWN_VTABLES:
            return KNOWN_VTABLES[cpp]
        row = classes.get(cpp)
        return int(row["vtable"], 16) if row and row["vtable"] else None

    def parent_vtable(cpp):
        """vtable of the closest ancestor whose vtable is known."""
        parent = classes[cpp]["super"]
        while parent:
            p = by_name.get(parent)
            if vtable(p) is not None:
                return vtable(p)
            parent = classes[p]["super"] if p in classes else None
        return None

    def ancestors(cpp):
        chain = []
        while cpp in classes:
            chain.append(cpp)
            parent = classes[cpp]["super"]
            cpp = by_name.get(parent) if parent else None
        return chain

    def common_ancestor(cpps):
        common = ancestors(cpps[0])
        for c in cpps[1:]:
            others = set(ancestors(c))
            common = [a for a in common if a in others]
        return common[0] if common else "UObject"

    candidates = collections.defaultdict(set)
    for cpp, row in classes.items():
        vt = vtable(cpp)
        if vt is None:
            continue
        parent_vt = parent_vtable(cpp)
        for offset, slot in slot_names(cpp).items():
            target = read(vt + offset)
            if target is None or not in_text(target):
                continue
            if parent_vt is not None and read(parent_vt + offset) == target:
                continue
            candidates[target].add((cpp, slot.replace("~UObject", "~" + cpp)))

    with OUT.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["address", "class", "name"])
        named = 0
        for target, names in sorted(candidates.items()):
            if len(names) == 1:
                cpp, slot = next(iter(names))
            elif len({s for _, s in names if not s.startswith("~")} | ({"~"} if any(s.startswith("~") for _, s in names) else set())) == 1:
                # siblings sharing one implementation of the same slot inherit it from a common ancestor
                cpp = common_ancestor([c for c, _ in names])
                slot = next(iter(names))[1]
                if slot.startswith("~"):
                    slot = "~" + cpp
            else:
                continue
            w.writerow([f"{target:08x}", cpp, slot])
            named += 1
    print(f"{named} virtual overrides named, {len(candidates) - named} shared targets skipped")


if __name__ == "__main__":
    main()
