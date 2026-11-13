"""GAMELIB_ID: a fingerprint of the generator, the guest headers and the guest compile flags.

Release packages build the game code library (`lasgame.so` / `lasgame.dll`) on the
player's machine from their own dump (tools/gamebuild.py). The runtime refuses a
library built by a different generator, header set or flag set, so this value is
baked into the runtime at package time (-DGAMELIB_ID) and into the library at build
time. Keep it in one place: the packagers and the builder both read it here.
"""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Sources whose change makes older game code libraries invalid: the lifter, the
# patches it applies, the imports it replaces and the whole guest-facing ABI.
SOURCES = (
    'tools/gen_all.py',
    'tools/gamelib_entry.c',
    'games/xcde/profile.json',
    'games/xcde/patches.py',
    'games/xcde/hle_symbols.txt',
    'runtime/cpu.h',
    'runtime/simd.h',
    'runtime/simd_fp.h',
)

# Guest translation units are plain C. They are compiled to PIC objects and linked
# into a shared library that the runtime loads; no C++ or host headers are involved.
# The same flags as the source builds' game code (scripts/build_boot.sh, build_linux.sh): -O1 matters, the guest
# memory helpers in runtime/cpu.h rely on it. zig spells the CPU level x86_64_v2.
GUEST_FLAGS = (
    # -g0: zig emits debug info by default; the player does not need it and it
    # multiplies the object files (3.7 MB instead of 0.6 MB per chunk).
    '-g0', '-O1', '-march=x86_64_v2', '-ffp-contract=off', '-fno-strict-aliasing',
    '-fvisibility=hidden', '-D_GNU_SOURCE', '-w',
)


def sources():
    """Every input of the fingerprint, sorted, as paths relative to the repository root."""
    files = {name for name in SOURCES if (ROOT / name).is_file()}
    files.update(p.relative_to(ROOT).as_posix() for p in (ROOT / 'tools/lifter').glob('*.py'))
    files.add('tools/game_id.py')
    return sorted(files)


def fingerprint():
    """16 hex digits: stable for a given generator, header and flag set."""
    h = hashlib.sha256()
    for name in sources():
        h.update(name.encode())
        h.update(b'\0')
        h.update((ROOT / name).read_bytes())
        h.update(b'\0')
    h.update(' '.join(GUEST_FLAGS).encode())
    return h.hexdigest()[:16]


if __name__ == '__main__':
    print(fingerprint())
