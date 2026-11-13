#!/usr/bin/env python3
"""[XCDE] T-FSR4-LITE: prepare AMD SDK 2.4.0's actual FSR 4.1.2 Lite INT8 assets.

Build-time only: public SDK, pinned capture/translation tools, MinGW and an installed
Proton that can run FidelityFX 4.1.2. No Proton or Windows DLL is used by Monado.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK_COMMIT = '7539ae8b6568ad29ed78a753404351daf3b5cca7'
TOOLS_COMMIT = '0fc2921c45f8ff3900f6bfa43a299e9a4f532af4'

def run(args, **kw):
    subprocess.run(list(map(str, args)), check=True, **kw)

def checkout(url, path, revision, sparse=()):
    if not (path / '.git').exists():
        run(['git', 'clone', '--filter=blob:none', '--no-checkout', url, path])
        if sparse:
            run(['git', '-C', path, 'sparse-checkout', 'set', *sparse])
        run(['git', '-C', path, 'checkout', revision])
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        raise RuntimeError(f'{path}: expected pinned {revision}, got {actual}')

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdk', type=Path, help='existing SDK checkout at the pinned v2.4.0 commit')
    ap.add_argument('--tools', type=Path, help='existing pinned bbport checkout')
    ap.add_argument('--proton', type=Path, help='installed Proton directory; otherwise tools autodetect')
    ap.add_argument('--work', type=Path, default=ROOT / 'build/fsr4')
    args = ap.parse_args()
    work = args.work.resolve(); work.mkdir(parents=True, exist_ok=True)
    sdk = args.sdk.resolve() if args.sdk else work / 'sdk-2.4.0'
    capture = args.tools.resolve() if args.tools else work / 'capture-tools'
    checkout('https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git', sdk, SDK_COMMIT,
             ['Kits/FidelityFX/signedbin', 'Kits/FidelityFX/docs'])
    checkout('https://github.com/deadinside28/bloodborne_pc.git', capture, TOOLS_COMMIT,
             ['tools/fsr4cap'])
    original = sdk / 'Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll'
    prepare = work / 'prepare'; prepare.mkdir(exist_ok=True)
    forced = prepare / original.name
    run(['python3', capture / 'tools/fsr4cap/driver_model.py', 'force', original, forced, 'lite'])
    # Fresh output prevents the upstream tool's preservation of existing model folders
    # colliding with a newly prepared Lite folder on a second invocation.
    with tempfile.TemporaryDirectory(prefix='.asset-stage-', dir=work) as staging:
        assets = Path(staging) / 'assets'
        env = dict(os.environ, BB_FSR4CAP_WORK=str(prepare), BB_FSR411_OUT=str(assets),
                   FSR4CAP_VERSION='4.1.2', FSR4CAP_MODEL='lite', BB_FSR4CAP_FP8='0',
                   BB_FSR4CAP_RUNNER=os.environ.get('BB_FSR4CAP_RUNNER', 'direct'))
        if args.proton:
            env['PROTONPATH'] = str(args.proton.resolve())
        run(['bash', capture / 'tools/fsr4cap/build_assets.sh', forced], env=env)
        for tier in ('t1080_m0', 't1080_m1', 't2160_m0', 't2160_m1'):
            directory = assets / 'lite' / tier
            if (directory / 'initializer.bin').stat().st_size != 27392 or len(list(directory.glob('*.spv'))) != 12:
                raise RuntimeError(f'incomplete Lite model: {directory}')
        provenance = {'sdk': {'version': '2.4.0', 'commit': SDK_COMMIT,
                             'upscaler': '4.1.2.4555', 'dll_sha256': hashlib.sha256(original.read_bytes()).hexdigest()},
                      'model': 'ffxProvider_FSR4Lite_Int8', 'native_tools_commit': TOOLS_COMMIT,
                      'translator_commit': '7dc52786cdb1f53c54dd5c5c2698dff00ea5f0a3',
                      'files': {str(p.relative_to(assets)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in sorted(assets.rglob('*')) if p.is_file()}}
        (assets / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        shutil.copy2(sdk / 'Kits/FidelityFX/docs/license.md', assets / 'LICENSE-AMD-SDK.md')
        destination = work / 'assets'
        previous = Path(staging) / 'previous'
        if destination.exists():
            destination.rename(previous)
        try:
            assets.rename(destination)
        except BaseException:
            if previous.exists():
                previous.rename(destination)
            raise
        print(destination)

if __name__ == '__main__':
    main()
