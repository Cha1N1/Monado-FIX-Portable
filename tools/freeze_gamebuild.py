"""Freeze the first-start builder into a single executable for release packages.

PyInstaller is only needed on the build machine (pip install pyinstaller); players
run the frozen binary. The result is `build/gamebuild/monado-gamebuild[.exe]`, which
the packagers place beside the resources staged by tools/gamebuild_stage.py.

    python3 tools/freeze_gamebuild.py [--python .venv/bin/python]
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAME = 'monado-gamebuild'
HIDDEN = ('gen', 'analyze', 'lifter', 'nso', 'patchfmt', 'simd_c', 'sym', 'check_dump', 'manifest')
COLLECT = ('unicorn', 'capstone', 'lz4')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--python', default=sys.executable, help='interpreter with PyInstaller, capstone, unicorn and lz4')
    parser.add_argument('--output', default=str(ROOT / 'build/gamebuild'))
    args = parser.parse_args(argv)
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    work = ROOT / 'build/pyinstaller'
    command = [args.python, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--name', NAME,
               '--distpath', str(output), '--workpath', str(work), '--specpath', str(work),
               '--paths', str(ROOT / 'tools/lifter'), '--paths', str(ROOT / 'tools/extract')]
    for module in HIDDEN:
        command += ['--hidden-import', module]
    for package in COLLECT:  # native wheels (capstone, unicorn) load their own libraries
        command += ['--collect-all', package]
    command.append(str(ROOT / 'tools/gamebuild_entry.py'))
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError:
        raise SystemExit('freeze: PyInstaller is not installed for this interpreter (pip install pyinstaller)')
    except subprocess.CalledProcessError as error:
        raise SystemExit(f'freeze: PyInstaller failed ({error.returncode})')
    binary = output / (NAME + ('.exe' if os.name == 'nt' else ''))
    if not binary.is_file():
        raise SystemExit(f'freeze: PyInstaller did not produce {binary}')
    binary.chmod(0o755)
    print(f'frozen builder: {binary}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
