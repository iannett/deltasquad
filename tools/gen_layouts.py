#!/usr/bin/env python3
"""Lays out every script class and struct the way the xbox's Link does and checks native classes against
their xbox C++ sizes from symbols/classes.csv, or generates the C++ classes, native layout tables and native
function tables.

usage: gen_layouts.py <layout-dump .json>                       check the layouts
       gen_layouts.py <layout-dump .json> generate <Package>...  write src/<Package>/{Inc,Src}/<Package>Classes.*
                                                                 and <Package>NativeStubs.cpp
the json comes from tools/layout-dump.
"""
import csv
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

# element sizes on the xbox. arrays and strings are a data pointer plus a packed count word.
XBOX_SIZES = {
    'UByteProperty': 1,
    'UIntProperty': 4,
    'UBoolProperty': 4,
    'UFloatProperty': 4,
    'UNameProperty': 4,
    'UObjectProperty': 4,
    'UClassProperty': 4,
    'UStrProperty': 8,
    'UArrayProperty': 8,
    'UDelegateProperty': 8,
    'UMapProperty': 16,
}


class Layouts:
    def __init__(self, dump):
        self.structs = {}
        self.functions = {}
        for package in dump:
            self.functions[package['name'].lower()] = package.get('natives', [])
            for s in package['structs']:
                s['package'] = package['name']
                self.structs[(s['outer'].lower(), s['name'])] = s
        self.done = {}

    def find(self, outer, name):
        return self.structs.get((outer.lower(), name))

    def super_of(self, s):
        if not s['super']:
            return None
        return self.find(s['superOuter'], s['super'])

    def element(self, p):
        """xbox element size and alignment of a property"""
        if p['type'] == 'UStructProperty':
            inner = self.layout(self.find(p['structOuter'], p['structName']))
            return inner['size'], inner['align']
        size = XBOX_SIZES[p['type']]
        align = 4 if size % 4 == 0 else (2 if size % 2 == 0 else 1)
        return size, align

    def layout(self, s):
        """offsets of a struct's own properties, its size and alignment, as UStruct::Link computes them"""
        key = (s['outer'].lower(), s['name'])
        if key in self.done:
            return self.done[key]
        sup = self.super_of(s)
        start, align = 0, 1
        if sup:
            sl = self.layout(sup)
            start, align = sl['aligned'], sl['align']
        size = start
        props = []
        prev = None
        for p in s['properties']:
            if p['type'] == 'UBoolProperty' and prev and prev['type'] == 'UBoolProperty' and prev['bit'] < 31:
                entry = dict(p, offset=prev['offset'], bit=prev['bit'] + 1, elem=4)
            else:
                elem, a = self.element(p)
                offset = (size + a - 1) & ~(a - 1)
                entry = dict(p, offset=offset, bit=0, elem=elem)
                align = max(align, a)
            size = entry['offset'] + entry['elem'] * p['arrayDim']
            props.append(entry)
            prev = entry
        result = {'start': start, 'size': size, 'aligned': (size + 3) & ~3, 'align': align, 'props': props}
        self.done[key] = result
        return result


def xbox_natives():
    """xbox exec function address and native table entry by (C++ class, function name)"""
    with open(os.path.join(ROOT, 'symbols', 'natives.csv')) as f:
        return {(row['class'], row['name'][len('exec'):]): row for row in csv.DictReader(f)}


def written_natives(src_dir, generated):
    """exec functions already hand-written in a module's sources, as (C++ class, function name)"""
    found = set()
    for name in os.listdir(src_dir):
        if name.endswith('.cpp') and name not in generated:
            found.update(re.findall(r'^\w[\w\s:*&]*?\b(\w+)::exec(\w+)\s*\(', open(os.path.join(src_dir, name)).read(), re.M))
    return found


def xbox_classes():
    with open(os.path.join(ROOT, 'symbols', 'classes.csv')) as f:
        return {(row['package'], row['name']): row for row in csv.DictReader(f)}


def check(layouts, natives):
    good = bad = missing = 0
    for (package, name), row in sorted(natives.items()):
        s = layouts.find(package, name)
        if not s:
            missing += 1
            print(f'missing  {package}.{name}')
            continue
        size = layouts.layout(s)['aligned']
        if size == int(row['size']):
            good += 1
        else:
            bad += 1
            print(f'size     {package}.{name}: script {size}, xbox {row["size"]}')
    print(f'{good} match, {bad} differ, {missing} not in the packages')


# script structs of Core that already have a hand-written C++ type
CORE_STRUCTS = {
    'Vector': 'FVector', 'Plane': 'FPlane', 'Rotator': 'FRotator', 'Coords': 'FCoords', 'Quat': 'FQuat',
    'Scale': 'FScale', 'Matrix': 'FMatrix', 'Guid': 'FGuid', 'Color': 'FColor', 'Box': 'FBox', 'Range': 'FRange',
    'RangeVector': 'FRangeVector', 'CompressedPosition': 'FCompressedPosition', 'InterpCurve': 'FInterpCurve',
    'InterpCurvePoint': 'FInterpCurvePoint', 'IntBox': 'FIntBox', 'FloatBox': 'FFloatBox',
}

# script members a hand-written C++ type stores under another name
CPP_MEMBER_NAMES = {('FMatrix', 'XPlane'): 'M[0]', ('FMatrix', 'YPlane'): 'M[1]', ('FMatrix', 'ZPlane'): 'M[2]', ('FMatrix', 'WPlane'): 'M[3]'}

# classes C++ knows by a name other than prefix + script name, or that have no script
CPP_CLASS_NAMES = {'Object': 'UObject', 'Class': 'UClass', 'Field': 'UField', 'Struct': 'UStruct', 'Function': 'UFunction',
                   'State': 'UState', 'Package': 'UPackage', 'Subsystem': 'USubsystem', 'Commandlet': 'UCommandlet'}

# C++-only members whose real type script can't express, by (class, member). the type replaces the script
# placeholder's array size too.
CPP_MEMBER_TYPES = {('Texture', 'Mips'): 'TArrayNoInit<FMipmap>', ('Sound', 'Data'): 'TLazyArray<BYTE>'}

# hand-written headers with the C++ types those members use, by package
CPP_TYPE_HEADERS = {'Engine': ['UnTex.h', 'UnFont.h']}

# generated headers of the packages a package's classes build on
PACKAGE_DEPENDENCIES = {'Gameplay': ['Engine'], 'CTGame': ['Gameplay'], 'CTMarkers': ['CTGame'], 'IpDrv': ['Engine'], 'XGame': ['Engine'],
                        'XInterface': ['Engine'], 'XInterfaceLive': ['XInterface']}

CPF_NATIVE = 0x1000

HEADER_NOTE = '// generated by tools/gen_layouts.py from the packages. do not edit; rerun the generator instead.\n'


class Generator:
    def __init__(self, layouts, natives, package):
        self.layouts = layouts
        self.natives = {(p.lower(), n): row for (p, n), row in natives.items()}
        self.package = package
        self.xbox_natives = xbox_natives()
        self.classes = {}
        for s in layouts.structs.values():
            if s['kind'] == 'class':
                self.classes.setdefault(s['name'], s)

    def is_native(self, s):
        return (s['package'].lower(), s['name']) in self.natives

    def class_cpp(self, name):
        """the C++ type for a reference to class Name: the nearest native class at or above it"""
        s = self.classes.get(name)
        while s and not self.is_native(s):
            s = self.classes.get(s['super']) if s['super'] else None
        if not s:
            return CPP_CLASS_NAMES.get(name, 'U' + name)
        if s['name'] in CPP_CLASS_NAMES:
            return CPP_CLASS_NAMES[s['name']]
        return self.natives[(s['package'].lower(), s['name'])]['cpp_name']

    def struct_cpp(self, outer, name):
        if outer == 'Object' and name in CORE_STRUCTS:
            return CORE_STRUCTS[name]
        return 'F' + name

    def cpp_type(self, p):
        t = p['type']
        if t == 'UByteProperty':
            return 'BYTE'
        if t == 'UIntProperty':
            return 'INT'
        if t == 'UFloatProperty':
            return 'FLOAT'
        if t == 'UBoolProperty':
            return 'BITFIELD'
        if t == 'UNameProperty':
            return 'FName'
        if t == 'UStrProperty':
            return 'FStringNoInit'
        if t == 'UClassProperty':
            return 'UClass*'
        if t == 'UObjectProperty':
            return self.class_cpp(p['objectClass']) + '*'
        if t == 'UStructProperty':
            return self.struct_cpp(p['structOuter'], p['structName'])
        if t == 'UDelegateProperty':
            return 'FScriptDelegate'
        if t == 'UArrayProperty':
            inner = self.cpp_type(p['inner'])
            if inner == 'FStringNoInit':
                inner = 'FString'
            return f'TArrayNoInit<{inner}>'
        raise ValueError(f'no C++ type for {t}')

    def members(self, s):
        """C++ member lines for a struct or class's own properties. the xbox compiler never packs a member into a
        parent's tail padding or into the rest of a bitfield word, and Link doesn't either, so a byte-aligned
        member in those places is pinned to 4 bytes, and a bool group starts a fresh word."""
        lines = []
        prev = None
        prev_align = 4
        for p in s['properties']:
            t = self.cpp_type(p)
            if t == 'BITFIELD':
                if prev is None or (prev != 'BITFIELD' and prev_align < 4):
                    lines.append('    BITFIELD : 0;')
                lines.append(f'    BITFIELD {p["name"]} : 1;')
            else:
                dim = f'[{p["arrayDim"]}]' if p['arrayDim'] > 1 else ''
                if (s['name'], p['name']) in CPP_MEMBER_TYPES:
                    t, dim = CPP_MEMBER_TYPES[(s['name'], p['name'])], ''
                note = '  // C++ only' if p['flags'] & CPF_NATIVE else ''
                overridden = (s['name'], p['name']) in CPP_MEMBER_TYPES
                pin = 'alignas(4) ' if self.layouts.element(p)[1] < 4 and prev in (None, 'BITFIELD') and not overridden else ''
                lines.append(f'    {pin}{t} {p["name"]}{dim};{note}')
            prev = t
            prev_align = self.layouts.element(p)[1] if t != 'BITFIELD' else 4
        return lines

    def offset_entries(self, cpp, s):
        entries = []
        for p in s['properties']:
            if p['type'] == 'UBoolProperty':
                value = f'NativeBitfieldOffset<{cpp}>([]({cpp}& X) {{ X.{p["name"]} = 1; }})'
            else:
                member = CPP_MEMBER_NAMES.get((cpp, p['name']), p['name'])
                value = f'static_cast<INT>(offsetof({cpp}, {member}))'
            entries.append(f'    {{ TEXT("{p["name"]}"), {value} }},')
        return entries

    def own_structs(self):
        """the package's script structs that native C++ sees, in dependency order, each name once: those declared in
        native classes and those native class members use"""
        result, seen, names = [], set(), {}
        pkg = self.package.lower()
        candidates = [s for s in self.layouts.structs.values() if s['kind'] == 'struct' and s['package'].lower() == pkg]

        def visit(s):
            key = (s['outer'], s['name'])
            if key in seen:
                return
            seen.add(key)
            deps = [s['super']] if s['super'] else []
            for p in s['properties']:
                q = p['inner'] or p
                if q['type'] == 'UStructProperty':
                    deps.append((q['structOuter'], q['structName']))
            if s['super']:
                deps[0] = (s['superOuter'], s['super'])
            for outer, name in deps:
                d = self.layouts.find(outer, name)
                if d and d['package'].lower() == pkg and d['kind'] == 'struct':
                    visit(d)
            if s['name'] in names:
                if [ (p['name'], p['type']) for p in names[s['name']]['properties'] ] != [(p['name'], p['type']) for p in s['properties']]:
                    raise ValueError(f'two different structs named {s["name"]}')
                return
            names[s['name']] = s
            result.append(s)

        native_outers = {c['name'] for c in self.classes.values() if self.is_native(c)}
        used = set()
        for c in self.classes.values():
            if self.is_native(c) and c['package'].lower() == pkg:
                for p in c['properties']:
                    q = p['inner'] or p
                    if q['type'] == 'UStructProperty':
                        used.add((q['structOuter'].lower(), q['structName']))
        for s in sorted(candidates, key=lambda s: (s['outer'], s['name'])):
            if s['outer'] in native_outers or (s['outer'].lower(), s['name']) in used:
                visit(s)
        return result

    def own_classes(self):
        """the package's native classes that have script, parents first"""
        pkg = self.package.lower()
        result, seen = [], set()

        def visit(s):
            if s['name'] in seen:
                return
            seen.add(s['name'])
            sup = self.layouts.super_of(s)
            if sup and sup['package'].lower() == pkg:
                visit(sup)
            result.append(s)

        for s in sorted(self.layouts.structs.values(), key=lambda s: s['name']):
            if s['kind'] == 'class' and s['package'].lower() == pkg and self.is_native(s):
                visit(s)
        return result

    def shell_classes(self):
        """the package's native classes that have no script, parents first. they get no members until their code
        is decompiled."""
        pkg = self.package.lower()
        rows = {n: row for (p, n), row in self.natives.items() if p == pkg and n not in self.classes}
        result = []

        def visit(name):
            row = rows.pop(name, None)
            if row:
                visit(row['super'])
                result.append(row)

        for name in sorted(rows):
            visit(name)
        return result

    def class_natives(self, s):
        """a class's native functions in xbox table order, then any the xbox table lacks"""
        cpp = self.class_cpp(s['name'])
        funcs = [f for f in self.layouts.functions[self.package.lower()] if f['ownerClass'] == s['name']]
        return sorted(funcs, key=lambda f: self.xbox_natives.get((cpp, f['name']), {}).get('table_entry', 'z'))

    def write_header(self, path, structs, classes, cpptext_dir):
        out = ['#pragma once', '', HEADER_NOTE.rstrip(), '', '#include "Core.h"', '#include "UnMath.h"']
        out += [f'#include "{d}Classes.h"' for d in PACKAGE_DEPENDENCIES.get(self.package, [])]
        out += [f'#include "{h}"' for h in CPP_TYPE_HEADERS.get(self.package, [])]
        out.append('')
        out.append('// the classes members point at, declared ahead')
        declared = set(CPP_CLASS_NAMES.values())
        for s in structs + classes:
            for p in s['properties']:
                for q in (p, p['inner']):
                    if q and q['type'] == 'UObjectProperty':
                        cpp = self.class_cpp(q['objectClass'])
                        if cpp not in declared:
                            declared.add(cpp)
                            out.append(f'class {cpp};')
        for s in classes:
            cpp = self.class_cpp(s['name'])
            if cpp not in declared:
                declared.add(cpp)
                out.append(f'class {cpp};')
        for row in self.shell_classes():
            if row['cpp_name'] not in declared:
                declared.add(row['cpp_name'])
                out.append(f'class {row["cpp_name"]};')
        out.append('')
        for s in structs:
            sup = self.layouts.super_of(s)
            base = f' : public {self.struct_cpp(sup["outer"], sup["name"])}' if sup else ''
            out.append(f'struct {self.struct_cpp(s["outer"], s["name"])}{base}')
            out.append('{')
            out += self.members(s)
            out.append('};')
            out.append('')
        for s in classes:
            cpp = self.class_cpp(s['name'])
            row = self.natives[(s['package'].lower(), s['name'])]
            sup = self.class_cpp(s['super'])
            within = self.class_cpp(row['within'])
            out.append(f'class {cpp} : public {sup}')
            out.append('{')
            out.append(f'    DECLARE_CLASS({cpp}, {sup}, {row["class_flags"]}, {within})')
            out.append('public:')
            out += self.members(s)
            natives = self.class_natives(s)
            if natives:
                out.append('')
                out += [f'    DECLARE_FUNCTION(exec{f["name"]})' for f in natives]
            if os.path.exists(os.path.join(cpptext_dir, cpp + '.h')):
                out.append('')
                out.append(f'#include "CppText/{cpp}.h"')
            out.append('};')
            out.append('')
        out.append('// native classes without script. their members are added when their code is decompiled.')
        out.append('')
        for row in self.shell_classes():
            cpp = row['cpp_name']
            sup = self.class_cpp(row['super'])
            out.append(f'// {row["size"]} bytes on the xbox')
            out.append(f'class {cpp} : public {sup}')
            out.append('{')
            out.append(f'    DECLARE_CLASS({cpp}, {sup}, {row["class_flags"]}, {self.class_cpp(row["within"])})')
            if os.path.exists(os.path.join(cpptext_dir, cpp + '.h')):
                out.append('public:')
                out.append(f'#include "CppText/{cpp}.h"')
            out.append('};')
            out.append('')
        open(path, 'w').write('\n'.join(out))

    def has_dynamic(self, s):
        """whether a struct or class, or a parent, holds a string, array or delegate, which are bigger in the port"""
        while s:
            for p in s['properties']:
                if p['type'] in ('UStrProperty', 'UArrayProperty', 'UDelegateProperty'):
                    return True
                if p['type'] == 'UStructProperty' and self.has_dynamic(self.layouts.find(p['structOuter'], p['structName'])):
                    return True
            s = self.layouts.super_of(s)
        return False

    def write_source(self, path, header, structs, classes):
        out = [HEADER_NOTE.rstrip(), '', '#include <cstdint>', '', f'#include "{header}"', '']
        for s in classes:
            out.append(f'IMPLEMENT_CLASS({self.class_cpp(s["name"])}, {self.package})')
        for row in self.shell_classes():
            out.append(f'IMPLEMENT_CLASS({row["cpp_name"]}, {self.package})')
        out.append('')
        out += self.native_tables(classes)
        out.append('// on 32-bit targets, classes without strings or arrays (which the port stores in 12 bytes, the xbox in 8)')
        out.append('// come out exactly the xbox size')
        out.append('#if UINTPTR_MAX == 0xFFFFFFFF')
        for s in classes:
            if not self.has_dynamic(s):
                row = self.natives[(s['package'].lower(), s['name'])]
                out.append(f'static_assert(sizeof({self.class_cpp(s["name"])}) == {row["size"]}, "{s["name"]} is not its xbox size");')
        out.append('#endif')
        out.append('')
        out += self.layout_tables(structs, classes)
        open(path, 'w').write('\n'.join(out))

    def native_tables(self, classes):
        """each class's native function table, hooked up right after the IMPLEMENT_CLASS lines"""
        out = ['// native functions by name, with their fixed native index or 0']
        for s in classes:
            natives = self.class_natives(s)
            if not natives:
                continue
            cpp = self.class_cpp(s['name'])
            out.append(f'static FNativeEntry {cpp}Natives[] =')
            out.append('{')
            for f in natives:
                out.append(f'    {{ TEXT("{f["name"]}"), {{ static_cast<Native>(&{cpp}::exec{f["name"]}) }}, {f["native"]} }},')
            out.append('    { nullptr, { nullptr }, 0 },')
            out.append('};')
            out.append(f'IMPLEMENT_NATIVE_TABLE({cpp}, {cpp}Natives)')
            out.append('')
        return out

    def write_stubs(self, path, header, classes, written):
        """a stub for every native that isn't hand-written yet"""
        out = [HEADER_NOTE.rstrip(), '// natives that are hand-written elsewhere in Src/ are left out; rerun after writing one.', '',
               f'#include "{header}"', '']
        count = 0
        for s in classes:
            cpp = self.class_cpp(s['name'])
            for f in self.class_natives(s):
                if (cpp, f['name']) in written:
                    continue
                row = self.xbox_natives.get((cpp, f['name']))
                address = f'0x{row["address"]}' if row else 'none'
                out.append(f'// {address}' if row else '// not in the xbox native table')
                out.append(f'void {cpp}::exec{f["name"]}(FFrame& Stack, RESULT_DECL)')
                out.append('{')
                out.append(f'    NOT_DECOMPILED({address});')
                out.append('}')
                out.append('')
                count += 1
        open(path, 'w').write('\n'.join(out))
        return count

    def layout_tables(self, structs, classes):
        out = ['#pragma clang diagnostic push', '#pragma clang diagnostic ignored "-Winvalid-offsetof"', '', 'namespace', '{']
        for s in structs:
            cpp = self.struct_cpp(s['outer'], s['name'])
            out.append(f'    // {s["outer"]}.{s["name"]}')
            out.append(f'    const FNativePropertyOffset {cpp}Offsets[] =')
            out.append('    {')
            out += ['    ' + e for e in self.offset_entries(cpp, s)]
            out.append('    };')
            out.append(f'    const FNativeLayout {cpp}Layout(TEXT("{s["outer"]}"), TEXT("{s["name"]}"), sizeof({cpp}), alignof({cpp}), '
                       f'{cpp}Offsets, ARRAY_COUNT({cpp}Offsets));')
            out.append('')
        for s in classes:
            cpp = self.class_cpp(s['name'])
            row = self.natives[(s['package'].lower(), s['name'])]
            out.append(f'    // {s["name"]}: {row["size"]} bytes on the xbox')
            if s['properties']:
                out.append(f'    const FNativePropertyOffset {cpp}Offsets[] =')
                out.append('    {')
                out += ['    ' + e for e in self.offset_entries(cpp, s)]
                out.append('    };')
                out.append(f'    const FNativeLayout {cpp}Layout(&{cpp}::StaticClass, sizeof({cpp}), alignof({cpp}), {cpp}Offsets, ARRAY_COUNT({cpp}Offsets));')
            else:
                out.append(f'    const FNativeLayout {cpp}Layout(&{cpp}::StaticClass, sizeof({cpp}), alignof({cpp}), nullptr, 0);')
            out.append('')
        out += ['}', '', '#pragma clang diagnostic pop', '']
        return out


def generate(layouts, natives, package):
    gen = Generator(layouts, natives, package)
    structs = gen.own_structs()
    src = os.path.join(ROOT, 'src', package)
    if package == 'Core':
        # Core's classes are hand-written; only its script structs get tables
        structs = [s for s in structs if s['name'] in CORE_STRUCTS]
        out = [HEADER_NOTE.rstrip(), '', '#include "Core.h"', '#include "UnMath.h"', '']
        out += gen.layout_tables(structs, [])
        open(os.path.join(src, 'Src', 'CoreStructLayouts.cpp'), 'w').write('\n'.join(out))
        print(f'Core: {len(structs)} struct layouts')
        return
    classes = gen.own_classes()
    header = f'{package}Classes.h'
    gen.write_header(os.path.join(src, 'Inc', header), structs, classes, os.path.join(src, 'Inc', 'CppText'))
    gen.write_source(os.path.join(src, 'Src', f'{package}Classes.cpp'), header, structs, classes)
    stubs = f'{package}NativeStubs.cpp'
    written = written_natives(os.path.join(src, 'Src'), {f'{package}Classes.cpp', stubs})
    count = gen.write_stubs(os.path.join(src, 'Src', stubs), header, classes, written)
    natives = sum(len(gen.class_natives(s)) for s in classes)
    print(f'{package}: {len(structs)} structs, {len(classes)} classes, {len(gen.shell_classes())} without script, '
          f'{natives} natives ({count} stubs)')


def main():
    dump = json.load(open(sys.argv[1]))
    layouts = Layouts(dump)
    natives = xbox_classes()
    if len(sys.argv) > 2 and sys.argv[2] == 'generate':
        for package in sys.argv[3:]:
            generate(layouts, natives, package)
    else:
        check(layouts, natives)


if __name__ == '__main__':
    main()
