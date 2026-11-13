"""T253: build/test modified components without linking the game.

python tools/validate_port.py [--build-ui]
Uses project-local build/toolchain/ziglang and build/deps/ucrt64, or XCDE_ZIG / XCDE_SDK.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--build-ui', action='store_true')
    parser.add_argument('--compile-only', action='store_true', help='Build native tests without executing unsigned binaries')
    parser.add_argument('--clang', type=Path, help='Optional standalone Clang driver (same Windows GNU ABI); default remains Zig')
    parser.add_argument('--skip-test', action='append', default=[], help='Report a native test as skipped, for an external execution restriction')
    args = parser.parse_args()
    os.chdir(ROOT)
    output = ROOT / ('build/validation-clang' if args.clang else 'build/validation')
    output.mkdir(parents=True, exist_ok=True)
    temp = ROOT / 'build/tmp'
    temp.mkdir(parents=True, exist_ok=True)
    os.environ.update(TEMP=str(temp), TMP=str(temp), ZIG_GLOBAL_CACHE_DIR=str(ROOT / 'build/zig-cache-validated'), ZIG_LOCAL_CACHE_DIR=str(ROOT / 'build/zig-local'))
    zig = os.environ.get('XCDE_ZIG', str(ROOT / 'build/toolchain/ziglang/zig.exe'))
    sdk = Path(os.environ.get('XCDE_SDK', ROOT / 'build/deps/ucrt64'))
    if not (args.clang.is_file() if args.clang else Path(zig).is_file()) or not (sdk / 'include/vulkan/vulkan.h').is_file():
        parser.error('Missing Zig/SDK. See docs/PORT_VALIDATION.md for project-local setup.')
    inc = ['-Iruntime', '-Iruntime/nvn', '-Ithird_party', '-Ithird_party/imgui',
           '-Ithird_party/imgui/backends', '-Ithird_party/streamline/include']
    def includes(cpp=False):
        # Zig's libc++ ships its own C headers (string.h, math.h, ...). They have to be found before the SDK's CRT
        # headers, or libc++ mixes two libc header sets and fails to compile; -idirafter still finds SDL3/Vulkan/VMA.
        sdk_inc = ['-idirafter', str(sdk / 'include')] if cpp else [f'-I{sdk / "include"}']
        return [*inc, *sdk_inc]
    failures = []
    skipped = []
    def compiler(cpp=False):
        if args.clang:
            prefix=args.clang.resolve().parent.parent
            gcc_versions=list((prefix/'lib/gcc/x86_64-w64-mingw32').glob('*'))
            gcc=max(gcc_versions,key=lambda p: tuple(int(v) for v in p.name.split('.'))) if gcc_versions else None
            flags=[str(args.clang.resolve()), '--driver-mode=g++' if cpp else '--driver-mode=gcc',
                   f'--sysroot={sdk}', f'-L{sdk / "lib"}', '-fuse-ld=lld', '-march=x86-64-v2']
            if gcc:
                flags += [f'-L{gcc}', f'-L{prefix / "lib"}']
                if cpp:
                    include=prefix/'include/c++'/gcc.name
                    flags += ['-isystem', str(include), '-isystem', str(include/'x86_64-w64-mingw32'),
                              '-isystem', str(include/'backward')]
            return flags
        return [zig, 'c++' if cpp else 'cc', '-march=x86_64_v2']
    for dll in ['SDL3.dll', 'vulkan-1.dll', 'libwinpthread-1.dll']:
        if (sdk / 'bin' / dll).is_file():
            shutil.copy2(sdk / 'bin' / dll, output / dll)

    def run(name, command):
        with (output / f'{name}.log').open('w', encoding='utf-8') as log:
            try:
                result = subprocess.run([str(a) for a in command], stdout=log, stderr=subprocess.STDOUT, timeout=300)
                code = result.returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                log.write(f'Execution unavailable: {error}\n')
                code = 1
        print(f'{name}: {"PASS" if not code else "FAIL"}', flush=True)
        if code:
            failures.append(name)
            print((output / f'{name}.log').read_text(encoding='utf-8', errors='replace')[-3000:])
        return code == 0

    sources = ['host/settings_ini.c', 'hle/framepace.c', 'hle/party.c', 'hle/bindings.c', 'services/fs.c', 'diag/diag.c', 'nvn/objects.c', 'services/romfs_virtual.c', 'nvn/output_scale.c',
               'nvn/vulkan/device.c', 'nvn/vulkan/exec.c', 'nvn/vulkan/shaders.c',
               'nvn/vulkan/textures.c', 'nvn/vulkan/precompile.c', 'nvn/vulkan/temporal.c', 'nvn/vulkan/vkq.c',
               'nvn/vulkan/imgui_glue.cpp', 'ui/menu_vk.cpp', 'nvn/vulkan/dlss.cpp']
    for source in sources:
        cpp = source.endswith('.cpp')
        run(source.replace('/', '_'), [*compiler(cpp), '-c', '-w',
            '-std=c++17' if cpp else '-std=gnu11', *includes(cpp), ROOT / 'runtime' / source,
            '-o', output / (source.replace('/', '_') + '.o')])
    extra_sources = {
        'test_settings_roundtrip': ['runtime/host/settings_ini.c'],
        'test_output_scale': ['runtime/nvn/output_scale.c'],
        'test_texture_index': ['-O1', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections'],
        'test_romfs_cache': ['-O1', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', str(sdk / 'lib/libwinpthread.dll.a')],
        'test_translate_tex': ['runtime/nvn/maxwell/translate.c'],
        'test_translate_ldc': ['runtime/nvn/maxwell/translate.c'],
        'test_translate_xfb': ['runtime/nvn/maxwell/translate.c'],
        'test_tegra_copy': ['runtime/nvn/tegra.c', 'runtime/nvn/formats.c'],
        'test_tegra_block_height': ['runtime/nvn/tegra.c', 'runtime/nvn/formats.c'],
        'test_fibers': ['runtime/hle/fibers.c'],
        'test_condvar': ['runtime/platform/windows.c', 'runtime/diag/diag.c', 'runtime/diag/sampler.c', '-lwinmm', '-ldbghelp'],
    }
    for test in ['test_render_policy', 'test_display_options', 'test_port_core', 'test_gpu_probe', 'test_settings_roundtrip', 'test_time_math', 'test_movie_buf', *[k for k in extra_sources if k != 'test_settings_roundtrip']]:
        if test in args.skip_test:
            skipped.append(test)
            print(f'{test}: SKIPPED (explicit --skip-test)', flush=True)
            continue
        exe = output / f'{test}.exe'
        extra = extra_sources.get(test, [])
        if test in ('test_tegra_copy', 'test_tegra_block_height', 'test_condvar'):
            extra = [*extra, str(sdk / 'lib/libwinpthread.dll.a')]
        warnings = ['-Wall', '-Wextra', '-Werror'] if test in ('test_display_options', 'test_port_core', 'test_gpu_probe', 'test_settings_roundtrip') else ['-w']
        if run(test + '_build', [*compiler(), '-std=gnu11', *warnings, *includes(), '-Iruntime/nvn/maxwell',
                                ROOT / 'tests/native' / f'{test}.c', *extra, '-o', exe]):
            if args.compile_only:
                skipped.append(test)
                print(f'{test}: NOT EXECUTED (--compile-only)', flush=True)
            else:
                run(test, [exe, output / 'test-settings.ini'] if test in ('test_port_core', 'test_settings_roundtrip') else [exe, output] if test=='test_romfs_cache' else [exe])
    libs = [sdk / 'lib/libSDL3.dll.a', sdk / 'lib/libvulkan-1.dll.a']
    signature_test = output / 'test_nvidia_signature.exe'
    if run('test_nvidia_signature_build', [*compiler(True), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                                          'tests/native/test_nvidia_signature.cpp', '-o', signature_test]):
        if not args.compile_only and (ROOT / 'build/sl.interposer.dll').is_file():
            run('test_nvidia_signature', [signature_test, ROOT / 'build/sl.interposer.dll'])
        else:
            skipped.append('test_nvidia_signature')
            print('test_nvidia_signature: NOT EXECUTED (compile-only or official DLL absent)', flush=True)
    run('gpu_diagnostics_build', [*compiler(), '-std=c11', *includes(), 'tools/gpu_diagnostics.c', *libs,
                                  '-o', output / 'gpu_diagnostics.exe'])
    if args.build_ui:
        files = ['imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp',
                 'backends/imgui_impl_sdl3.cpp', 'backends/imgui_impl_sdlrenderer3.cpp']
        objects = []
        for source in [*(str(ROOT / 'third_party/imgui' / f) for f in files), str(ROOT / 'tools/launcher/launcher.cpp')]:
            obj = output / (Path(source).stem + '.o')
            if run(Path(source).stem + '_build', [*compiler(True), '-O1', '-std=c++17', '-w', *includes(True), '-c', source, '-o', obj]):
                objects.append(obj)
        if len(objects) == len(files) + 1:
            run('launcher_link', [*compiler(True), *objects, *libs, '-static-libstdc++', '-static-libgcc',
                                  '-o', output / 'xcde_launcher.exe', '-Wl,--subsystem,windows'])
    print(f'Logs/artifacts: {output}; failures: {len(failures)}; skipped: {len(skipped)}', flush=True)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
