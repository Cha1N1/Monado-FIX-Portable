"""Patch format for the generator (contract: docs/contracts/patch-format.md).  [XCDE]

A patch replaces one guest instruction by hand-written C. It must name the original instruction word it expects, so
a patch written for one build can never be applied silently to another, and it must cite a requirement or task.
"""
import importlib.util
import os


class PatchMismatch(Exception):
    pass


class Patch:
    def __init__(self, expected, emit, req, note=''):
        if not req:
            raise ValueError('a patch must cite a requirement or task (req)')
        self.expected = expected
        self.emit = emit
        self.req = req
        self.note = note

    def apply(self, gen, addr):
        found = gen.word(addr)
        if found != self.expected:
            raise PatchMismatch(f'patch at {addr:#x}: expected word {self.expected:#010x}, found {found:#010x} '
                                f'(req {self.req})')
        return self.emit(gen, addr)


class JumpTable:
    """A verified byte-offset switch whose cases may cross lifted function boundaries."""

    def __init__(self, build_id, instruction, table_offset, case_base, targets, req):
        self.build_id = build_id
        self.instruction = instruction
        self.table_offset = table_offset
        self.case_base = case_base
        self.targets = tuple(targets)
        self.req = req

    def resolve(self, module, addr):
        lo, size = module.nso.rodata
        end = self.table_offset + len(self.targets)
        valid = (module.nso.build_id == self.build_id and module.word(addr) == self.instruction
                 and lo <= self.table_offset < end <= lo + size)
        if valid:
            targets = tuple(self.case_base + 4 * value for value in module.nso.image[self.table_offset:end])
            if targets == self.targets and all(module.in_text(target) for target in targets):
                return sorted(set(targets))
        raise PatchMismatch(f'jump table at {addr:#x} does not match the verified XCDE build (req {self.req})')


def load_patches(path):
    """Load PATCHES = {addr: Patch} from a python file; a missing file means no patches."""
    if not path or not os.path.exists(path):
        return {}
    spec = importlib.util.spec_from_file_location('xcde_patches', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    patches = getattr(mod, 'PATCHES', {})
    for a, p in patches.items():
        if a % 4:
            raise ValueError(f'patch address {a:#x} is not 4-byte aligned')
        if not isinstance(p, (Patch, JumpTable)):
            raise ValueError(f'patch at {a:#x} is not a Patch or JumpTable')
    return patches
