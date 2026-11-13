#!/usr/bin/env python3
"""Build FSR 4 Lite's on-PC preparation tool for a package: build/fsr4prep/ (the launcher's "Prepare FSR 4 Lite").

    python tools/build_fsr4prep.py [--dest build/fsr4prep] [--no-freeze]

It holds bbport's capture scripts (third_party/fsr4cap, GPL-2.0-or-later) and, in bin/, the tools they run:
  fsr4cap.exe, amdxc64.dll   bbport's recorder and AMD-extension stub (Windows programs: run natively on Windows,
                             under the player's Proton on Linux), built with zig
  dxil-spirv, spirv-as/-dis  dxil-spirv at bbport's pinned commit with its patch, and the SPIRV-Tools it bundles
                             (Linux: the system compiler, $ORIGIN run path; Windows: MSYS2 UCRT64 gcc)
  Windows only: d3d12.dll, d3d12core.dll (vkd3d-proton 3.1) and dxgi.dll (DXVK 3.1.1), SHA-256 pinned
and monado-fsr4prep[.exe] (tools/fsr4_prepare.py frozen with PyInstaller, unless --no-freeze). Nothing of AMD's is in
it: AMD's DLL is downloaded on the player's PC.
Needs: git, cmake, ninja, a C++ compiler (Windows: MSYS2 UCRT64 gcc on PATH), the ziglang package, network.
"""
import argparse
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from gamebuild_stage import find_zig  # noqa: E402

CAP = ROOT / 'third_party' / 'fsr4cap'
DXIL_SPIRV = ('https://github.com/HansKristian-Work/dxil-spirv.git', '7dc52786cdb1f53c54dd5c5c2698dff00ea5f0a3')
VKD3D = ('https://github.com/HansKristian-Work/vkd3d-proton/releases/download/v3.1/vkd3d-proton-3.1.tar.zst',
         'b85ea8219b71093a3924913dd7aae6172333dc0c069983ed510816d67f02120b', ('d3d12.dll', 'd3d12core.dll'))
DXVK = ('https://github.com/doitsujin/dxvk/releases/download/v3.1.1/dxvk-3.1.1.tar.gz',
        '40565b4a724aadc4433fa4e010b4b23916d9b1f1baeee64e17186db94f54e608', ('dxgi.dll',))
WINDOWS = os.name == 'nt'


def run(cmd, **kw):
    print('+', ' '.join(map(str, cmd)), flush=True)
    subprocess.run(list(map(str, cmd)), check=True, **kw)


def fetch(url, sha256):
    request = urllib.request.Request(url, headers={'User-Agent': 'Monado-build'})
    with urllib.request.urlopen(request, timeout=300) as response:
        data = response.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != sha256:
        raise SystemExit(f'{url}: SHA-256 {digest}, expected {sha256}')
    return data


def untar(data, names, dest, zstd=False):
    """The x64 builds of `names` from a release archive."""
    if zstd:
        try:
            from compression import zstd as z  # Python 3.14
            data = z.decompress(data)
        except ImportError:
            import zstandard
            data = zstandard.ZstdDecompressor().decompress(data, max_output_size=512 << 20)
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        for member in tar.getmembers():
            parts = Path(member.name).parts
            if member.isfile() and len(parts) >= 2 and parts[-2] == 'x64' and parts[-1] in names:
                (dest / parts[-1]).write_bytes(tar.extractfile(member).read())
    missing = [n for n in names if not (dest / n).is_file()]
    if missing:
        raise SystemExit(f'not in the archive: {missing}')


def build_recorder(zig, bin_):
    target = ['-target', 'x86_64-windows-gnu']
    inc = ['-I', CAP / 'ffx/api/include', '-I', CAP / 'ffx/upscalers/include']
    run([zig, 'cc', *target, '-std=c11', '-O1', *inc, CAP / 'fsr4cap.c', CAP / 'capture.c', CAP / 'rootsig.c',
         '-o', bin_ / 'fsr4cap.exe', '-ld3d12', '-ldxguid'])
    run([zig, 'cc', *target, '-O1', '-shared', CAP / 'amdxc64.c', '-o', bin_ / 'amdxc64.dll'])
    for leftover in ('fsr4cap.pdb', 'amdxc64.pdb', 'amdxc64.lib'):
        (bin_ / leftover).unlink(missing_ok=True)


def build_dxil_spirv(src, bin_):
    if not (src / 'CMakeLists.txt').is_file():
        run(['git', 'clone', '-q', DXIL_SPIRV[0], src])
        run(['git', '-C', src, 'checkout', '-q', DXIL_SPIRV[1]])
        run(['git', '-C', src, 'submodule', 'update', '-q', '--init', '--recursive'])
        run(['git', '-C', src, 'apply', CAP / 'dxil-spirv-class-bindings.patch'])
    build = src / 'build'
    config = ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release']
    if WINDOWS:
        config += ['-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++']
    else:
        config += ['-DCMAKE_BUILD_RPATH=$ORIGIN', '-DCMAKE_BUILD_RPATH_USE_ORIGIN=ON']
    run(['cmake', '-S', src, '-B', build, *config])
    run(['cmake', '--build', build, '--target', 'dxil-spirv', 'spirv-as', 'spirv-dis'])
    exe = '.exe' if WINDOWS else ''
    shutil.copy2(build / f'dxil-spirv{exe}', bin_)
    for name in ('spirv-as', 'spirv-dis'):
        shutil.copy2(build / 'third_party/SPIRV-Tools/tools' / f'{name}{exe}', bin_)
    library = 'libdxil-spirv-c-shared.dll' if WINDOWS else 'libdxil-spirv-c-shared.so.2'
    shutil.copy2(build / library, bin_)
    if WINDOWS:  # the MinGW runtime the tools link against
        gcc_bin = Path(shutil.which('g++')).parent
        for dll in ('libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll'):
            shutil.copy2(gcc_bin / dll, bin_)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--dest', type=Path, default=ROOT / 'build/fsr4prep')
    ap.add_argument('--src', type=Path, default=ROOT / 'build/fsr4prep-src', help='dxil-spirv checkout / build')
    ap.add_argument('--no-freeze', action='store_true', help='skip PyInstaller (run tools/fsr4_prepare.py instead)')
    a = ap.parse_args(argv)
    dest = a.dest.resolve()
    if dest.exists():
        shutil.rmtree(dest)
    bin_ = dest / 'bin'
    bin_.mkdir(parents=True)
    for item in CAP.iterdir():  # the scripts the preparation runs, with their licence
        if item.suffix in ('.py', '.sh') or item.name in ('LICENSE', 'README.md'):
            shutil.copy2(item, dest / item.name)
    zig = find_zig()
    build_recorder(zig, bin_)
    build_dxil_spirv(a.src.resolve(), bin_)
    if WINDOWS:
        untar(fetch(*VKD3D[:2]), VKD3D[2], bin_, zstd=True)
        untar(fetch(*DXVK[:2]), DXVK[2], bin_)
    if not a.no_freeze:
        work = ROOT / 'build/pyinstaller-fsr4prep'
        run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--name', 'monado-fsr4prep',
             '--distpath', dest, '--workpath', work, '--specpath', work,
             *[x for m in ('glob', 'math', 'struct', 'hashlib', 're', 'shutil') for x in ('--hidden-import', m)],
             ROOT / 'tools/fsr4_prepare.py'])
    print(f'FSR 4 Lite preparation tool: {dest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
