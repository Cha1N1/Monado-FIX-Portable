"""Source-level checks for the launcher / in-game menu English translations."""
from pathlib import Path
import ast
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / "tools/launcher/launcher.cpp"
MENU = ROOT / "runtime/ui/menu_vk.cpp"

# Intentional no-ops: the Spanish and English text are identical (or it is a format string).
UNTRANSLATED_OK = {"Audio", "%s"}


def translation_keys(source: str) -> set[str]:
    """The Spanish keys of the `static const char *const k_en[][2]` table."""
    start = source.index("static const char *const k_en[][2] = {")
    i = source.index("{", start)
    depth = 0
    in_string = False
    escaped = False
    for j in range(i, len(source)):
        c = source[j]
        if in_string:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                in_string = False
            continue
        if c == '"':
            in_string = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                break
    else:
        raise AssertionError("unterminated k_en table")

    tokens = []
    k = i + 1
    while k < j:
        c = source[k]
        if c == '"':
            e = k + 1
            escaped = False
            while e < j:
                if escaped:
                    escaped = False
                elif source[e] == "\\":
                    escaped = True
                elif source[e] == '"':
                    break
                e += 1
            tokens.append(("string", ast.literal_eval(source[k:e + 1])))
            k = e + 1
        elif c in "{},":
            tokens.append(("punct", c))
            k += 1
        else:
            k += 1

    keys = set()
    entry = []
    field = []
    for kind, value in tokens:
        if kind == "string":
            field.append(value)
        elif value == ",":
            if field:
                entry.append("".join(field))
                field = []
        elif value == "}":
            if field:
                entry.append("".join(field))
                field = []
            if len(entry) == 2:
                keys.add(entry[0])
            entry = []
    return keys


def direct_t_calls(source: str) -> set[str]:
    """String literals passed directly to T("..."), with adjacent literals joined."""
    calls = set()
    for match in re.finditer(r'T\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)\)', source):
        literals = re.findall(r'"(?:\\.|[^"\\])*"', match.group(1))
        calls.add("".join(ast.literal_eval(lit) for lit in literals))
    return calls


class LauncherTranslations(unittest.TestCase):
    def test_every_direct_t_call_has_an_english_entry(self):
        for path in (LAUNCHER, MENU):
            source = path.read_text(encoding="utf-8")
            missing = sorted(direct_t_calls(source) - translation_keys(source) - UNTRANSLATED_OK)
            self.assertEqual([], missing, f"missing English translations in {path.name}: {missing}")

    def test_gpu_compatibility_labels_are_english_in_english_mode(self):
        launcher = LAUNCHER.read_text(encoding="utf-8")
        self.assertIn('g.ui_lang ? "AMD flicker fix (restart)" : "Fix de parpadeo AMD (al reiniciar)"',
                      launcher)
        self.assertIn('g.ui_lang ? "Long-session compatibility (restart)" : '
                      '"Compatibilidad de sesión larga (al reiniciar)"', launcher)


if __name__ == "__main__":
    unittest.main()
