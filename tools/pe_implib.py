#!/usr/bin/env python3
"""Write the import library a release game library links against: the exports of build/xcde.exe.

A release runtime (CMake -DMONADO_RELEASE=ON) is linked with --export-all-symbols; the game code library that the
launcher builds on the player's PC (tools/gamebuild.py) resolves the runtime's functions and data through it. zig's
linker does not write an import library itself, so this reads the executable's PE export table, writes a .def
(data exports marked DATA) and runs `zig dlltool` on it.

    python tools/pe_implib.py build/xcde.exe build/gamebuild/xcde_imports.a --zig <zig>
"""
import argparse
import struct
import subprocess
import sys
from pathlib import Path

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def exports(exe):
    data = Path(exe).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise SystemExit(f'{exe}: not a PE file')
    sections_count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    if struct.unpack_from('<H', data, optional)[0] != 0x20B:
        raise SystemExit(f'{exe}: not a PE32+ (x86-64) image')
    export_rva, export_size = struct.unpack_from('<II', data, optional + 112)
    sections = []
    for i in range(sections_count):
        base = optional + optional_size + 40 * i
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, base + 8)
        flags = struct.unpack_from('<I', data, base + 36)[0]
        sections.append((va, max(vsize, rsize), raw, flags))

    def offset(rva):
        for va, size, raw, _ in sections:
            if va <= rva < va + size:
                return raw + rva - va
        raise SystemExit(f'{exe}: RVA {rva:#x} outside every section')

    def executable(rva):
        return any(va <= rva < va + size and flags & IMAGE_SCN_MEM_EXECUTE for va, size, _, flags in sections)

    if not export_rva:
        raise SystemExit(f'{exe} exports nothing: link it with -Wl,--export-all-symbols (CMake -DMONADO_RELEASE=ON)')
    d = offset(export_rva)
    functions_count, names_count, functions, names, ordinals = struct.unpack_from('<IIIII', data, d + 20)
    result = []
    for i in range(names_count):
        name_rva = struct.unpack_from('<I', data, offset(names) + 4 * i)[0]
        end = data.index(b'\0', offset(name_rva))
        name = data[offset(name_rva):end].decode()
        ordinal = struct.unpack_from('<H', data, offset(ordinals) + 2 * i)[0]
        target = struct.unpack_from('<I', data, offset(functions) + 4 * ordinal)[0]
        forwarded = export_rva <= target < export_rva + export_size
        if not forwarded:
            result.append((name, not executable(target)))
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('exe')
    ap.add_argument('output', help='the import library to write (.a)')
    ap.add_argument('--zig', required=True, help='zig executable (its dlltool writes the library)')
    a = ap.parse_args(argv)
    names = exports(a.exe)
    if len(names) < 100:
        raise SystemExit(f'{a.exe}: only {len(names)} exports; the runtime was not linked with --export-all-symbols')
    output = Path(a.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    definition = output.with_suffix('.def')
    definition.write_text(f'LIBRARY {Path(a.exe).name}\nEXPORTS\n' +
                          ''.join(f'  {n}{" DATA" if is_data else ""}\n' for n, is_data in sorted(names)), encoding='ascii')
    subprocess.run([a.zig, 'dlltool', '-m', 'i386:x86-64', '-D', Path(a.exe).name, '-d', str(definition),
                    '-l', str(output)], check=True)
    print(f'{output}: {len(names)} exports ({sum(d for _, d in names)} data)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
