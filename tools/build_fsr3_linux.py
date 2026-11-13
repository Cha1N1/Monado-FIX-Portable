#!/usr/bin/env python3
"""Build Monado's native FSR 3.1.4 Vulkan component from pinned AMD SDK sources.
Only Monado's LDR, render-resolution unjittered motion, normal-depth permutations
are built. FP32 by default; FP16 variants of the two pyramid passes on request
(XCDE_FSR3_FP16=1, GPU with shaderFloat16) and wave64 on request (XCDE_FSR3_WAVE64=1).
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'c6efa6bf7f2027b3ec94f28578bb5965eabb9e55'
URL = 'https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git'
PASSES = ['prepare_inputs', 'luma_pyramid', 'shading_change_pyramid', 'shading_change',
          'prepare_reactivity', 'luma_instability', 'accumulate', 'accumulate', 'rcas',
          'debug_view', 'autogen_reactive']

def run(args):
    subprocess.run(list(map(str, args)), check=True)

def reflect(data):
    """Read the descriptor names/types/bindings retained in glslang SPIR-V."""
    words = struct.unpack('<%dI' % (len(data)//4), data)
    names, decorations, types, variables = {}, {}, {}, []
    at = 5
    while at < len(words):
        count, op = words[at] >> 16, words[at] & 65535
        a = words[at+1:at+count]
        if op == 5:
            names[a[0]] = struct.pack('<%dI' % (len(a)-1), *a[1:]).split(b'\0')[0].decode()
        elif op == 71:
            decorations.setdefault(a[0], {})[a[1]] = a[2:]
        elif op in (25, 26, 28, 30, 32):
            types[a[0]] = (op, a[1:])
        elif op == 59:
            variables.append(a[:3])
        at += count
    groups = {k: [] for k in ('cb', 'srv', 'uav', 'sampler')}
    for ty, ident, storage in variables:
        dec = decorations.get(ident, {})
        if 33 not in dec:
            continue
        op, args = types[types[ty][1][1]]
        if op == 30 and storage == 2:
            kind = 'cb'
        elif op == 26:
            kind = 'sampler'
        elif op == 25:
            kind = 'uav' if args[5] == 2 else 'srv'
        else:
            raise RuntimeError(f'Unexpected descriptor type {op}: {names.get(ident)}')
        if dec.get(34, (0,))[0] != 0:
            raise RuntimeError('Only descriptor set zero is supported')
        # SDK binding tables use the GLSL instance name, including uniform blocks.
        name = names[ident]
        groups[kind].append((dec[33][0], name))
    return groups

def emit_blob(source, tag, data):
    """Append one FfxShaderBlob (SPIR-V bytes plus reflected binding tables) to the generated C++."""
    groups = reflect(data)
    source.append(f'alignas(4) static const uint8_t {tag}[] = {{'+','.join(map(str,data))+'};')
    fields = []
    for kind in ('cb','srv','uav','sampler'):
        group = groups[kind]
        if not group:
            fields.append('nullptr,nullptr,nullptr,nullptr'); continue
        prefix = tag+'_'+kind
        source.extend([f'static const char *{prefix}_names[] = {{'+','.join(json.dumps(n) for _,n in group)+'};',
            f'static const uint32_t {prefix}_slots[] = {{'+','.join(str(b) for b,_ in group)+'};',
            f'static const uint32_t {prefix}_counts[] = {{'+','.join('1' for _ in group)+'};',
            f'static const uint32_t {prefix}_spaces[] = {{'+','.join('0' for _ in group)+'};'])
        fields.append(','.join(prefix+s for s in ('_names','_slots','_counts','_spaces')))
    source.append(f'static const FfxShaderBlob b{tag} = {{ {tag}, sizeof({tag}), '+
        ','.join(str(len(groups[k])) for k in ('cb','srv','uav'))+',0,0,'+str(len(groups['sampler']))+',0,'+
        ','.join(fields[:3])+',nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,'+
        fields[3]+',nullptr,nullptr,nullptr,nullptr};')

# [XCDE] T-FSR3-AMD-SPIRV: the prebuilt amd_fidelityfx_vk.dll of the pinned SDK checkout
# (PrebuiltSignedDLL/, MIT like the SDK) and the SPIR-V modules its FSR3 host code creates
# for Monado's permutation on 32/64-lane GPUs (captured with tests/native/bench_fsr3_4k.cpp
# FFX_DUMP_SPIRV under Wine; identified by resource names and constant-buffer offsets).
AMD_DLL_SHA256 = 'a1624cc4238fef046f30c4d80ce3f47be63fc5f5373f49e3ee9edb9960f54c78'
AMD_MODULES = {  # pass id (PASSES order) -> (bytes, SHA-256) of the module
    0: (7316, '73db061afe2beb93b4ff5f984ec88006f2b18dd03cb5c38d681e2b4085eef7ad'),  # prepare inputs
    1: (42656, '30898c522a51ad76f583f61a52ebd5dd3c1bea37789d73312550a0cca3144bec'),  # luma pyramid
    2: (112236, '8ade57eb8a624bc8190959997522375fe326ebe17bc8241ab6953d6f4103a353'),  # shading change pyramid
    3: (4184, '2c6e39a084ddbef2827b33e353177a51a48a788a5c06246329ad8fd75d0000c8'),  # shading change
    4: (13704, '041c5684c37e7cf281cd37673e47762b8706578b8a3ea4ebf9d0a2edd4015fa1'),  # prepare reactivity
    5: (7352, 'b17658ce559db486f29e30f315db6a35f1ea333e18a651cf08e9d8a33e0bdff4'),  # luma instability
    6: (24352, '67ed0f306c940ad26c09db77e44dc0b6907132deb3c6525a5a67dbd74db48cf9'),  # accumulate (LUT, FP16)
    7: (24140, '83e802fbb8809b532e9ce8fb5eeeabf88fbd0b090971be9c8eb655f258d08236'),  # accumulate + sharpen
    8: (18988, '8391105381e3c5fdf981a6d8a23406ed8230348947148d10eb215144d147b649'),  # RCAS
}

def extract_amd_modules(path):
    """{pass id: SPIR-V bytes} from the pinned AMD DLL, or {} when it is absent or not the pinned file."""
    import hashlib
    if not path:
        return {}
    path = Path(path)
    if not path.is_file():
        print(f'FSR3: {path} not found; GLSL shaders only')
        return {}
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != AMD_DLL_SHA256:
        print(f'FSR3: {path} is not the pinned amd_fidelityfx_vk.dll ({AMD_DLL_SHA256[:16]}...); GLSL shaders only')
        return {}
    found, at = {}, 0
    magic = struct.pack('<I', 0x07230203)  # SPIR-V header word; modules are stored whole in .rdata
    while (at := data.find(magic, at)) >= 0:
        for pass_id, (size, digest) in AMD_MODULES.items():
            if pass_id not in found and hashlib.sha256(data[at:at + size]).hexdigest() == digest:
                found[pass_id] = data[at:at + size]
        at += 4
    if len(found) != len(AMD_MODULES):
        raise RuntimeError(f'FSR3: the pinned AMD DLL lacks {len(AMD_MODULES) - len(found)} expected modules')
    print(f'FSR3: using {len(found)} SPIR-V modules from {path.name}')
    return found

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdk', type=Path)
    default_amd = ROOT/'build/reference/ffx-sdk-linux/PrebuiltSignedDLL/amd_fidelityfx_vk.dll'
    ap.add_argument('--amd-dll', default=os.environ.get('MONADO_FSR3_AMD_DLL', str(default_amd) if default_amd.exists() else ''),
                    help='pinned AMD prebuilt amd_fidelityfx_vk.dll whose FSR3 SPIR-V is used where AMD uses it ("" = GLSL only)')
    ap.add_argument('--target', choices=('linux', 'windows'), default='linux')
    # CMake build trees pass their own paths; the scripts keep the defaults.
    ap.add_argument('--output', type=Path, help='component to write (default: build/monado_fsr3.dll or build/linux/libmonado_fsr3.so)')
    ap.add_argument('--work', type=Path, help='folder for generated sources/SPIR-V (default: build/fsr3[-windows])')
    args = ap.parse_args()
    default_sdk = ROOT/('build/reference/ffx-sdk-windows' if args.target == 'windows' else 'build/reference/ffx-sdk-1.1.4')
    checkout = (args.sdk or default_sdk).resolve()
    if not checkout.exists():
        run(['git','clone','--filter=blob:none','--depth','1','--branch','v1.1.4','--no-checkout',URL,checkout])
        run(['git','-C',checkout,'sparse-checkout','set','sdk/include',
             'sdk/src/backends/vk','sdk/src/backends/shared','sdk/src/shared',
             'sdk/src/components/fsr3upscaler'])
        run(['git','-C',checkout,'checkout',REVISION])
    actual = subprocess.check_output(['git','-C',checkout,'rev-parse','HEAD'],text=True).strip()
    if actual != REVISION:
        raise RuntimeError(f'Expected AMD SDK {REVISION}, got {actual}')
    sdk = checkout/'sdk'
    out = (args.work or ROOT/('build/fsr3-windows' if args.target == 'windows' else 'build/fsr3')).resolve()
    out.mkdir(parents=True,exist_ok=True)
    # Linux wchar_t is 32 bits: enlarge the opaque SDK context for Linux only.
    # Windows wchar_t is 16 bits and uses the SDK's normal context size.
    header = out/'include/FidelityFX/host/ffx_fsr3upscaler.h'
    header.parent.mkdir(parents=True, exist_ok=True)
    header_text = (sdk/'include/FidelityFX/host/ffx_fsr3upscaler.h').read_text()
    if args.target == 'linux':
        header_text = header_text.replace(
            '#define FFX_FSR3UPSCALER_CONTEXT_SIZE (FFX_SDK_DEFAULT_CONTEXT_SIZE)',
            '#define FFX_FSR3UPSCALER_CONTEXT_SIZE (2 * FFX_SDK_DEFAULT_CONTEXT_SIZE)')
    header.write_text(header_text)
    source = ['#include "ffx_shader_blobs.h"', '#include <FidelityFX/host/ffx_fsr3upscaler.h>',
              '#include <cstdlib>', '#include <cstring>']
    # Analytical Lanczos history filter (lut 0) for every pass, plus LUT accumulate variants
    # (lut 1) served where the SDK requests them (32/64-lane subgroups, AMD's own choice).
    # [XCDE] T-FSR3-LUT: the LUT path lost stationary detail only because the Linux build
    # filled the LUT with the C abs(int) (compat.h now provides the float overload); with the
    # correct LUT it matches the analytical path in the multi-frame Native AA regression.
    # Sharpening remains a pass permutation.
    lut = 0
    lut_passes = {PASSES.index('accumulate'), 7}  # (7: accumulate + sharpen)
    # [XCDE] T-FSR3-FP16: 16-bit variants where FSR 3.1.4 has PC half-precision
    # code (the two SPD pyramids). Compiled with FFX_HALF=1 the other passes emit
    # no float16 instructions (their half paths are Xbox-only) and RCAS is never
    # FP16 off Xbox, so they keep the FP32 blobs. Picked when the SDK requests
    # ALLOW_FP16, i.e. the GPU reports shaderFloat16 (Monado's device enables it).
    half_passes = {PASSES.index('luma_pyramid'), PASSES.index('shading_change_pyramid')}
    variants = ([(p, n, 0, 0) for p, n in enumerate(PASSES)] + [(p, PASSES[p], 1, 0) for p in sorted(half_passes)] +
                [(p, PASSES[p], 0, 1) for p in sorted(lut_passes)])
    for pass_id, name, half, use_lut in variants:
        sharp = int(pass_id == 7)
        tag = f'p{pass_id}_{use_lut}' + ('h' if half else '')
        spv = out/f'{tag}.spv'
        flags = {'FFX_GPU':1,'FFX_GLSL':1,'FFX_HALF':half,
                 'FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE':use_lut,
                 'FFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT':0,
                 'FFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS':1,
                 'FFX_FSR3UPSCALER_OPTION_JITTERED_MOTION_VECTORS':0,
                 'FFX_FSR3UPSCALER_OPTION_INVERTED_DEPTH':0,
                 'FFX_FSR3UPSCALER_OPTION_APPLY_SHARPENING':sharp}
        run(['glslangValidator','-V','-Os','--target-env','vulkan1.2','-S','comp',
             '-I'+str(sdk/'include/FidelityFX/gpu'),
             *[f'-D{k}={v}' for k,v in flags.items()],
             sdk/f'src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_{name}_pass.glsl','-o',spv])
        emit_blob(source, tag, spv.read_bytes())
    # [XCDE] T-FSR3-AMD-SPIRV: AMD's prebuilt amd_fidelityfx_vk.dll (same SDK revision) carries
    # faster SPIR-V for the permutation its host code picks on GPUs with 32/64-lane subgroups:
    # LUT history filter + FP16 (RCAS without FP16). Same resources and constant-buffer offsets as
    # ours. With the pinned DLL present, those nine modules are extracted by SHA-256 and served for
    # exactly that request; everything else keeps the GLSL builds. Nothing is extracted without it.
    amd = extract_amd_modules(args.amd_dll)
    for pass_id, data in amd.items():
        emit_blob(source, f'p{pass_id}_amd', data)
    source.append('extern "C" FfxErrorCode ffxGetPermutationBlobByIndex(FfxEffect effect, FfxPass pass, FfxBindStage, uint32_t options, FfxShaderBlob *out) {\n'
                  'if (effect != FFX_EFFECT_FSR3UPSCALER || !out || (options & 30) != 4) return FFX_ERROR_INVALID_ARGUMENT;\n'
                  'const FfxShaderBlob *table[] = {'+
                  ','.join('&bp%d_0'%p for p in range(len(PASSES)))+'};\n'
                  'const FfxShaderBlob *half[] = {'+
                  ','.join(('&bp%d_0h' if p in half_passes else '&bp%d_0')%p for p in range(len(PASSES)))+'};\n'
                  'const FfxShaderBlob *amd[] = {'+
                  ','.join(('&bp%d_amd' % p) if p in amd else 'nullptr' for p in range(len(PASSES)))+'};\n'
                  'const FfxShaderBlob *lut[] = {'+
                  ','.join(('&bp%d_1' if p in lut_passes else '&bp%d_0')%p for p in range(len(PASSES)))+'};\n'
                  'if (pass >= sizeof(table)/sizeof(table[0])) return FFX_ERROR_INVALID_ARGUMENT;\n'
                  '// [XCDE] T-FSR3-LUT: options bit 0 = LUT history; the SDK sets it with 32/64-lane subgroups (AMD RDNA).\n'
                  '// XCDE_FSR3_LUT=0: the analytical history everywhere (also disables AMD\'s LUT modules below).\n'
                  'static const bool use_lut = [] { const char *e = std::getenv("XCDE_FSR3_LUT"); return !(e && *e == \'0\'); }();\n'
                  'if (!use_lut) options &= ~1u;\n'
                  '// [XCDE] T-FSR3-AMD-SPIRV: AMD\'s modules exactly where its own host picks them: LUT (bit 0) + ALLOW_FP16\n'
                  '// (bit 7, GPU shaderFloat16); RCAS (pass 8) is FP32. They pass the stationary-detail regression with the\n'
                  '// corrected LUT (compat.h abs fix). XCDE_FSR3_AMD_SHADERS=0: our GLSL builds only.\n'
                  'static const bool use_amd = [] { const char *e = std::getenv("XCDE_FSR3_AMD_SHADERS"); return !(e && *e == \'0\'); }();\n'
                  'if (use_amd && amd[pass] && (pass == 8 ? (options & 129) == 1 : (options & 129) == 129)) {\n'
                  '  std::memcpy(out,amd[pass],sizeof(*out)); return FFX_OK; }\n'
                  'if (options & 1) { std::memcpy(out,lut[pass],sizeof(*out)); return FFX_OK; }\n'
                  '// [XCDE] T-FSR3-FP16: options bit 7 = FSR3UPSCALER_SHADER_PERMUTATION_ALLOW_FP16. Opt-in (XCDE_FSR3_FP16=1):\n'
                  '// 4K Balanced timing on an RX 6900 XT showed no difference (2.43 ms either way) and FP16 changes the output.\n'
                  'static const bool fp16 = [] { const char *e = std::getenv("XCDE_FSR3_FP16"); return e && *e == \'1\'; }();\n'
                  'std::memcpy(out,((options & 128) && fp16 ? half : table)[pass],sizeof(*out)); return FFX_OK; }\n'
                  '// [XCDE] T-FSR3-WAVE64: XCDE_FSR3_WAVE64=1 asks the Vulkan backend for 64-lane subgroups\n'
                  '// (requiredSubgroupSize, only where the GPU allows 64). AMD uses wave64 for FSR on RDNA under\n'
                  '// DX12; its Vulkan backend never requests it. Opt-in until measured; the shaders are unchanged.\n'
                  'extern "C" FfxErrorCode ffxIsWave64(FfxEffect, uint32_t, bool &wave) {\n'
                  'static const bool on = [] { const char *e = std::getenv("XCDE_FSR3_WAVE64"); return e && *e == \'1\'; }();\n'
                  'wave = on; return FFX_OK; }')
    blob = out/'blobs.cpp'; blob.write_text('\n'.join(source)+'\n')
    native = ROOT/'third_party/fsr3_native'
    # The SDK marks EffectContext alignas(32) but packs its scratch arrays at
    # four-byte boundaries. Align that array explicitly and reserve the padding.
    backend = out/'ffx_vk.cpp'
    backend.write_text('#include <mutex>\n'+(sdk/'src/backends/vk/ffx_vk.cpp').read_text().replace(
        'sizeof(BackendContext_VK) + extensionPropArraySize',
        'sizeof(BackendContext_VK) + 31 + extensionPropArraySize').replace(
        '// Map context array',
        '// Map context array\n        pMem = (uint8_t*)FFX_ALIGN_UP((uintptr_t)pMem, 32);'))
    files = [native/'adapter.cpp',blob,backend,
             sdk/'src/components/fsr3upscaler/ffx_fsr3upscaler.cpp',
             sdk/'src/shared/ffx_assert.cpp',sdk/'src/shared/ffx_breadcrumbs_list.cpp',
             sdk/'src/shared/ffx_message.cpp',sdk/'src/shared/ffx_object_management.cpp']
    if args.target == 'windows':
        target = args.output or ROOT/'build/monado_fsr3.dll'
        compiler = shlex.split(os.environ.get('MONADO_FSR3_CXX', '.venv/Scripts/python -m ziglang c++'))
        msys_root = os.environ.get('MSYS_UCRT_ROOT', 'C:/msys64/ucrt64').replace('\\', '/')
        if msys_root.lower().startswith('/c/'):
            msys_root = 'C:' + msys_root[2:]
        # zig spells the CPU level x86_64_v2; Clang (CMake build, MSYS2 UCRT64) x86-64-v2.
        march = 'x86_64_v2' if any('zig' in part for part in compiler) else 'x86-64-v2'
        # Headers: build/windows-sdk-include (prepare_windows_sdk_headers.sh), or the CMake
        # windows-cross sysroot via MONADO_FSR3_WIN_INCLUDE.
        win_include = os.environ.get('MONADO_FSR3_WIN_INCLUDE', 'build/windows-sdk-include')
        flags = ['-std=c++17','-O2','-march='+march,'-shared','-fvisibility=hidden',
                 '-I'+win_include,'-L'+msys_root+'/lib']
        if march != 'x86_64_v2':
            flags += ['-include',str(native/'compat_windows.h')]
        # MSYS2 UCRT64 names the loader's import library libvulkan-1.dll.a, mingw64 libvulkan.dll.a.
        vulkan = next((msys_root+'/lib/'+n for n in ('libvulkan-1.dll.a', 'libvulkan.dll.a')
                       if os.path.exists(msys_root+'/lib/'+n)), msys_root+'/lib/libvulkan-1.dll.a')
        libs = [vulkan,'-lwinpthread']
    else:
        target = args.output or ROOT/'build/linux/libmonado_fsr3.so'
        compiler = shlex.split(os.environ.get('CXX','clang++'))
        flags = ['-std=c++17','-O2','-march=x86-64-v2','-fPIC','-shared','-fvisibility=hidden',
                 '-include',str(native/'compat.h')]
        libs = ['-lvulkan','-Wl,--no-undefined']
    target.parent.mkdir(parents=True,exist_ok=True)
    run([*compiler,*flags,'-I'+str(ROOT/'third_party'),'-I'+str(out/'include'),'-I'+str(sdk/'include'),
         '-I'+str(sdk/'src/backends/shared'),'-I'+str(sdk/'src/shared'),
         '-I'+str(sdk/'src/components'),*files,'-o',str(target)+'.tmp',*libs])
    os.replace(str(target)+'.tmp',target)
    (out/'provenance.json').write_text(json.dumps({'sdk':'1.1.4','fsr':'3.1.4','commit':actual},indent=2)+'\n')
    import shutil
    shutil.copy2(checkout/'LICENSE.txt',out/'LICENSE-AMD.txt')
    print('Built',target)

if __name__ == '__main__':
    main()
