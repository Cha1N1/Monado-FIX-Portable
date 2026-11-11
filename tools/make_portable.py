#!/usr/bin/env python3
"""Monado: build a self-contained, movable game folder.

    python tools/make_portable.py <out dir> [--no-saves] [--no-game] [--msys <ucrt64 bin dir>] [--bundle-vulkan]

The result runs from anywhere (another drive, another PC, a USB disk): no MSYS2, no Git, no Python needed.
It holds build/xcde.exe, build/xcde_launcher.exe, every non-system DLL they need (found by reading the PE import
tables, recursively, so it does not depend on `ldd`), targets/ (a REAL copy of exefs + romfs, even if your targets/
is a link), data/, mods/, xcde.ini, save/ and "Play Monado.bat".

NOT copied: cache/ (shader caches are rebuilt on each PC by the launcher's "Precompile shaders"; --with-cache
copies the portable part). targets/ is copied as real files and verified (no links to your original folder).

The folder contains the game: it is for your own use. Do not share it (Constitution art. I).
Only the standard library is used.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

# Windows' own loader (System32) supplies these; bundling a copy can only cause mismatches.
SYSTEM_PREFIXES = ('api-ms-win-', 'ext-ms-win-')
ALWAYS_SYSTEM = {'kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'shell32.dll', 'ole32.dll', 'oleaut32.dll',
                 'ntdll.dll', 'ws2_32.dll', 'winmm.dll', 'imm32.dll', 'version.dll', 'setupapi.dll', 'shlwapi.dll',
                 'comdlg32.dll', 'comctl32.dll', 'dbghelp.dll', 'bcrypt.dll', 'crypt32.dll', 'secur32.dll',
                 'userenv.dll', 'dwmapi.dll', 'hid.dll', 'cfgmgr32.dll', 'psapi.dll', 'iphlpapi.dll', 'msvcrt.dll',
                 'ucrtbase.dll', 'shcore.dll', 'mmdevapi.dll', 'avrt.dll', 'dxgi.dll', 'd3d11.dll', 'd3d12.dll',
                 'dwrite.dll', 'uxtheme.dll', 'wininet.dll', 'winhttp.dll', 'mfplat.dll', 'powrprof.dll', 'rpcrt4.dll',
                 'opengl32.dll', 'glu32.dll', 'combase.dll', 'wldap32.dll', 'normaliz.dll', 'ncrypt.dll'}


# ---------------------------------------------------------------- PE import tables (no dependencies)
def pe_imports(path):
    """Names of the DLLs a PE file imports (normal + delay-load). Returns [] for anything that is not a PE."""
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except OSError:
        return []
    if len(data) < 0x40 or data[:2] != b'MZ':
        return []
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        return []
    nsec, = struct.unpack_from('<H', data, pe + 6)
    opt_size, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from('<H', data, opt)
    dirs = opt + (112 if magic == 0x20B else 96)
    sec = opt + opt_size
    sections = []
    for i in range(nsec):
        vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', data, sec + i * 40 + 8)
        sections.append((vaddr, max(vsize, rawsize), rawptr))

    def off(rva):
        for va, size, raw in sections:
            if va <= rva < va + size:
                return raw + (rva - va)
        return None

    def cstr(o):
        e = data.find(b'\0', o)
        return data[o:e].decode('ascii', 'replace') if e > o else ''

    names = []
    imp_rva, imp_size = struct.unpack_from('<II', data, dirs + 8)  # directory 1: import
    o = off(imp_rva) if imp_rva else None
    while o is not None and o + 20 <= len(data):
        fields = struct.unpack_from('<5I', data, o)
        if not any(fields):
            break
        n = off(fields[3])
        if n is not None:
            names.append(cstr(n))
        o += 20
    if dirs + 13 * 8 + 8 <= len(data):
        d_rva, d_size = struct.unpack_from('<II', data, dirs + 13 * 8)  # directory 13: delay-load import
        o = off(d_rva) if d_rva else None
        while o is not None and o + 32 <= len(data):
            attrs, name_rva = struct.unpack_from('<II', data, o)
            if not attrs and not name_rva:
                break
            n = off(name_rva) if attrs & 1 else None
            if n is not None:
                names.append(cstr(n))
            o += 32
    return [n for n in names if n]


def is_system(name):
    low = name.lower()
    return low in ALWAYS_SYSTEM or low.startswith(SYSTEM_PREFIXES)


def find_msys_bin(cli):
    cands = [cli, os.environ.get('MSYS_UCRT64_BIN'), r'C:\msys64\ucrt64\bin']
    for c in cands:
        if c and os.path.isdir(c):
            return c
    return None


def collect_dlls(exes, search_dirs, skip):
    """Transitive closure of the non-system DLLs `exes` import. -> ({lowname: path}, [missing names])"""
    found, missing, todo = {}, set(), list(exes)
    seen = set()
    while todo:
        cur = todo.pop()
        for name in pe_imports(cur):
            low = name.lower()
            if low in seen or is_system(low):
                continue
            seen.add(low)
            if low in skip:
                continue
            hit = next((os.path.join(d, n) for d in search_dirs if os.path.isdir(d)
                        for n in os.listdir(d) if n.lower() == low), None)
            if hit:
                found[low] = hit
                todo.append(hit)
            elif not os.path.exists(os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32', name)):
                missing.add(name)
    return found, sorted(missing)


# ---------------------------------------------------------------- copying
def is_link(p):
    return os.path.islink(p) or (hasattr(os.path, 'isjunction') and os.path.isjunction(p))


def drop_link(p):
    """Remove a link / junction WITHOUT touching what it points to (a previous run may have left one)."""
    if is_link(p):
        try:
            os.rmdir(p)
        except OSError:
            os.unlink(p)


def copy_tree(src, dst, desc):
    src = os.path.realpath(src)  # a junction / symlink -> the real folder
    if not os.path.isdir(src):
        return False
    drop_link(dst)  # never copy "through" a link into the original files
    os.makedirs(dst, exist_ok=True)
    print(f'  {desc} ...', flush=True)
    if os.name == 'nt' and shutil.which('robocopy'):
        # robocopy: fast, skips what is already there (same size + time), so a second run only copies what changed
        r = subprocess.run(['robocopy', src, dst, '/E', '/NFL', '/NDL', '/NJH', '/NP', '/R:1', '/W:1'],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        if r.returncode >= 8:
            sys.exit(f'robocopy failed copying {src} (code {r.returncode}). Is there enough free space?\n' + r.stdout[-1500:])
        return True
    shutil.copytree(src, dst, dirs_exist_ok=True)
    return True


def stats(root, follow):
    n = size = 0
    for dp, _, fs in os.walk(root, followlinks=follow):
        for f in fs:
            try:
                size += os.path.getsize(os.path.join(dp, f))
                n += 1
            except OSError:
                pass
    return n, size


def copy_game(out):
    """targets/ -> out/targets as REAL files (targets/exefs and targets/romfs are usually links to your own dump)."""
    # the SOURCE must really hold the game, otherwise "0 files == 0 files" would look fine
    for name, need in (('exefs', 3), ('romfs', 1000)):
        real = os.path.realpath(os.path.join('targets', name))
        n, b = stats(real, True)
        print(f'  source targets/{name}: {real}  ({n} files, {b / 2**30:.2f} GiB)')
        if n < need or (name == 'exefs' and not os.path.isfile(os.path.join(real, 'main'))):
            sys.exit(f'targets/{name} does not hold the game files (found {n} files at {real}).\n'
                     'The link is probably broken or points to a folder that moved/was unplugged.\n'
                     'Run "Build Monado.bat" and pick the folder with exefs and romfs again '
                     '(tools\\setup_game_files.ps1), then try again.')
    dst_root = os.path.join(out, 'targets')
    drop_link(dst_root)
    os.makedirs(dst_root, exist_ok=True)
    for name in sorted(os.listdir('targets')):
        src = os.path.join('targets', name)
        if os.path.isdir(os.path.realpath(src)):
            copy_tree(src, os.path.join(dst_root, name), f'targets/{name} (real copy of the game files)')
        elif os.path.isfile(src):
            shutil.copy2(src, os.path.join(dst_root, name))
    # verify: no links left behind, and the same number of files / bytes as the source
    left = [os.path.join(dp, d) for dp, ds, _ in os.walk(dst_root) for d in ds if is_link(os.path.join(dp, d))]
    if left:
        sys.exit('The copy still contains links (it would depend on the original folder): ' + ', '.join(left[:3]))
    n_src, b_src = stats('targets', True)
    n_dst, b_dst = stats(dst_root, False)
    if (n_src, b_src) != (n_dst, b_dst):
        sys.exit(f'The game copy is incomplete: source {n_src} files / {b_src} bytes, copy {n_dst} files / {b_dst} bytes.')
    print(f'  Game files verified: {n_dst} files, {b_dst / 2**30:.1f} GiB, all real (no links).')


def tree_size(path):
    return stats(path, True)[1]


PLAY_BAT = ('@echo off\r\n'
            'rem Monado: launcher (graphics, scaling, controls, language), then the game.\r\n'
            'rem Portable folder: every DLL is in build\\, nothing outside this folder is needed.\r\n'
            'cd /d "%~dp0"\r\n'
            'start "" "build\\xcde_launcher.exe"\r\n')


CLEAN_BAT = '@echo off\r\nrem Monado: borra lo que es de ESTA PC (cache de shaders/pipelines, reportes de crash, logs).\r\nrem Corre esto antes de mover o copiar la carpeta a otra PC. NO toca save\\, xcde.ini ni el juego.\r\ncd /d "%~dp0"\r\necho.\r\necho  Se borrara: cache\\  crash\\  y los .log de build\\\r\necho  (Las partidas en save\\ y los ajustes NO se tocan. En esta PC tendras que volver a pulsar "Precompile shaders".)\r\nchoice /m " Continuar"\r\nif errorlevel 2 exit /b 0\r\nif exist "cache" rmdir /s /q "cache"\r\nif exist "crash" rmdir /s /q "crash"\r\ndel /q "build\\*.log" 2>nul\r\necho.\r\necho  Listo.\r\npause\r\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out', help='destination folder (created if needed)')
    ap.add_argument('--no-saves', action='store_true', help='do not copy save/')
    ap.add_argument('--no-game', action='store_true', help='do not copy targets/ (exefs + romfs)')
    ap.add_argument('--with-cache', action='store_true',
                    help='also copy cache/spirv and cache/variants.bin (by default NO cache is copied: it is rebuilt on each PC)')
    ap.add_argument('--msys', help=r'MSYS2 UCRT64 bin folder (default C:\msys64\ucrt64\bin)')
    ap.add_argument('--bundle-vulkan', action='store_true',
                    help='also copy MSYS2\'s vulkan-1.dll (by default the GPU driver\'s own loader is used)')
    a = ap.parse_args()

    os.chdir(REPO)
    out = os.path.abspath(a.out)
    if os.path.realpath(out) == os.path.realpath(REPO) or os.path.realpath(out).startswith(os.path.realpath(REPO) + os.sep):
        sys.exit('The destination must be outside the repository folder.')
    exes = [p for p in ('build/xcde.exe', 'build/xcde_launcher.exe')]
    for e in exes:
        if not os.path.isfile(e):
            sys.exit(f'{e} not found. Build first ("Build Monado.bat").')
    if not a.no_game and not os.path.isdir('targets/exefs'):
        sys.exit('targets/exefs not found (your game files). Run "Build Monado.bat" first, or use --no-game.')

    msys = find_msys_bin(a.msys)
    build_dlls = [os.path.join('build', f) for f in os.listdir('build') if f.lower().endswith('.dll')]
    search = [d for d in (msys, 'build') if d]
    skip = set() if a.bundle_vulkan else {'vulkan-1.dll'}
    found, missing = collect_dlls(exes + build_dlls, search, skip)
    if missing and not msys:
        sys.exit(f'MSYS2 not found (looked in C:\\msys64\\ucrt64\\bin) and these DLLs are not in build\\: {", ".join(missing)}\n'
                 'Pass --msys <folder with the DLLs>.')

    if not a.no_game:
        need = tree_size('targets')
        os.makedirs(out, exist_ok=True)
        free = shutil.disk_usage(out).free
        print(f'Game files: {need / 2**30:.1f} GiB; free at destination: {free / 2**30:.1f} GiB')
        if free < need + 2**30:
            sys.exit('Not enough free space at the destination.')

    print(f'Packaging into {out}')
    os.makedirs(os.path.join(out, 'build'), exist_ok=True)
    for e in exes:
        shutil.copy2(e, os.path.join(out, 'build', os.path.basename(e)))
    for d in build_dlls:  # DLSS / FSR / Reflex / SDL...: loaded by name at run time
        shutil.copy2(d, os.path.join(out, 'build', os.path.basename(d)))
    if os.path.isdir(os.path.join('build', 'dlss-licenses')):
        copy_tree(os.path.join('build', 'dlss-licenses'), os.path.join(out, 'build', 'dlss-licenses'), 'NVIDIA runtime licences/')
    if os.path.isfile(os.path.join('build', 'dlss-runtime.json')):
        shutil.copy2(os.path.join('build', 'dlss-runtime.json'), os.path.join(out, 'build', 'dlss-runtime.json'))
    for low, p in sorted(found.items()):
        dst = os.path.join(out, 'build', os.path.basename(p))
        if not os.path.exists(dst):
            shutil.copy2(p, dst)
    print(f'  {len(os.listdir(os.path.join(out, "build")))} files in build/ ({len(found)} DLLs found through the import tables)')

    if not a.no_game:
        copy_game(out)
    copy_tree('data', os.path.join(out, 'data'), 'data/')
    if os.path.isdir('mods'):
        copy_tree('mods', os.path.join(out, 'mods'), 'mods/')
    if a.with_cache:
        if os.path.isdir('cache/spirv'):
            copy_tree('cache/spirv', os.path.join(out, 'cache', 'spirv'), 'cache/spirv (shader cache)')
        if os.path.isfile('cache/variants.bin'):
            os.makedirs(os.path.join(out, 'cache'), exist_ok=True)
            shutil.copy2('cache/variants.bin', os.path.join(out, 'cache', 'variants.bin'))
    if not a.no_saves and os.path.isdir('save'):
        copy_tree('save', os.path.join(out, 'save'), 'save/')
    if os.path.isfile('xcde.ini'):
        shutil.copy2('xcde.ini', os.path.join(out, 'xcde.ini'))
    with open(os.path.join(out, 'Play Monado.bat'), 'w', newline='') as f:
        f.write(PLAY_BAT)
    with open(os.path.join(out, 'Limpiar Monado.bat'), 'w', newline='') as f:
        f.write(CLEAN_BAT)

    # check: everything the copied programs import is either in build\ or a Windows system DLL
    got, still = collect_dlls([os.path.join(out, 'build', os.path.basename(e)) for e in exes] +
                              [os.path.join(out, 'build', os.path.basename(d)) for d in build_dlls],
                              [os.path.join(out, 'build')], skip)
    unresolved = sorted(set(still))
    if unresolved:
        print('WARNING: not found in the portable folder (and not in System32): ' + ', '.join(unresolved))
    else:
        print('Check: every DLL the programs import is inside build\\ or comes with Windows.')
    print(f'\nDone. Move or copy the folder anywhere and start "Play Monado.bat".\n{out}')
    print('First run on each PC: press "Precompile shaders" once in the launcher (a few minutes), then PLAY.')


if __name__ == '__main__':
    main()
