"""(ocarina) The Insert menu in Portuguese (gpu/vk/vk_menu_pt.h, shown through vk_menu.cpp T): every text of the menu's own,
as gpu/vk/vk_menu.cpp has it (the rows' names and helps, the values and hints of the tx_ and hn_ functions and of MENU_HINTS,
the prompts, the Controls page's buttons, the texts drawn through T("...")), has its Portuguese; the texts the menu builds
from parts (a hint of two sentences, a resolution with its mode, a size and "(native)", a microphone "(not connected)") are
translated part by part; and nothing is too long for its place: a row's name at most NAME_MAX characters, a value at most
VALUE_MAX (a longer one slides in its cell when selected, and ends with "..." otherwise: allowed only where the English is
as long). The real translate() compiled with a small reader; no ImGui, GPU or game. Prints every name and value in both
languages with their lengths.

    python -I -B tests/test_menu_pt.py [--cxx g++] [--quiet]
"""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

RUN_ALL_ARGS = ["--cxx", "{cxx}", "--quiet"]

ROOT = Path(__file__).resolve().parents[1]
NAME_MAX = 28    # (a row's name: the name cell's room at the narrowest window the menu lays out for, 18.5-point type)
CONTROL_MAX = 20   # (a control's name on the Controls page: its cell is narrower, a third of the row, 17-point type)
VALUE_MAX = 26   # (a value: the value cell's room, 18-point type)

# (texts that stay as they are in Portuguese: names of things, words in both languages, the rows already in Portuguese, the
# Developer rows (technical), fragments the code builds into whole texts checked in COMPOSED)
KEEP = {
    "Idioma / Language", "Português", "English", "Catmull-Rom", "Lanczos", "Switch", "Xbox", "PlayStation 4",
    "PlayStation 5", "Steam Deck", "Xbox / PC", "Transformer (K)", "Transformer 2 (M)", "Transformer 2 (L)", "NVIDIA DLSS",
    "NVIDIA DLAA", "AMD FSR 3.1", "AMD FSR 4", "Descriptor reuse", "Sampler cache",
    " (not connected)", " (native)", " (game's)", "One made frame", "Two made frames", "Three made frames",
    "Four made frames", "Five made frames", "Six made frames", "Seven made frames",
    "Del",   # (a key's name on its cap)
}
# (whole texts the code makes from parts, and what they must become)
COMPOSED = {
    "Native (DLAA, 3840x2160)": "Nativa (DLAA, 3840x2160)",
    "Native (FSR 4, 2560x1440)": "Nativa (FSR 4, 2560x1440)",
    "Quality 67% (2560x1440)": "Qualidade 67% (2560x1440)",
    "Ultra Performance 33% (1280x720)": "Ultra Desempenho 33% (1280x720)",
    "2560x1440 (native)": "2560x1440 (nativa)",
    "2048x1152 (game's)": "2048x1152 (do jogo)",
    "USB Audio Device (not connected)": "USB Audio Device (desconectado)",   # (the name as it is)
    "200 nits": "200 nits",
    "3: Device 3": "3: Dispositivo 3",
    "The game's own network. DLSS and DLAA: need an NVIDIA RTX GPU. AMD FSR 4: not in this build (see the readme).":
        "A rede do próprio jogo. DLSS e DLAA: precisam de uma placa NVIDIA RTX. AMD FSR 4: não vem nesta versão (veja o leia-me).",
    "Two made frames between each two of the game's.": "Dois quadros criados entre cada dois do jogo.",
    "Up, Space": "Seta para cima, Espaço",
    "Key 233": "Tecla 233",
}
# (texts the table must leave alone: names it cannot know, texts already in Portuguese)
UNTOUCHED = ["NVIDIA GeForce RTX 4050 Laptop GPU", "Microfone (Realtek(R) Audio)", "Gerar relatório", "Num 4",
             "Ocarina Engine v0.4.0"]

READER = r'''#include "gpu/vk/vk_menu_pt.h"
#include <cstdio>
#include <iostream>
#include <string>
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string pt;
        const bool ok = mkwvk::menu_lang::translate(line, &pt);
        std::printf("%s\t%s\n", ok ? "1" : "0", ok ? pt.c_str() : line.c_str());
    }
}
'''


def literal_runs(text):
    """The C string literals of text, adjacent ones joined (as the compiler does), escapes \\" and \\\\ undone"""
    out, i = [], 0
    pat = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
    while True:
        m = pat.search(text, i)
        if not m:
            return out
        s, j = m.group(1), m.end()
        while True:
            n = re.match(r'\s*"((?:[^"\\\n]|\\.)*)"', text[j:])
            if not n:
                break
            s += n.group(1)
            j += n.end()
        out.append(s.replace('\\"', '"').replace("\\\\", "\\"))
        i = j


def strip_comments(text):
    """text without its // comments (a quote in a comment is no string), strings kept whole"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif c == "'":   # (a character literal: '"' is no string)
            j = text.index("'", i + 2 if text[i + 1] == "\\" else i + 1) if i + 1 < n else n
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            i = text.find("\n", i)
            i = n if i < 0 else i
        else:
            out.append(c)
            i += 1
    return "".join(out)


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def collect(src):
    """(kind, English) of every text the menu shows through T: rows' names (kind "name"), values ("value"), the rest ("text")"""
    found = []
    rows = block(src, "Row rows[] = {")
    for entry in re.findall(r'\{TAB_\w+,\s*(.*?)\},?\s*(?=\{TAB_|\}\s*$)', rows, re.S):
        texts = literal_runs(entry)
        if not texts:
            continue   # (the update's and the report's rows: their names are constants, in Portuguese)
        if not entry.lstrip().startswith('"'):   # (the update's and the report's rows: their names are constants, below)
            found += [("text", t) for t in texts]
            continue
        if texts[0] in KEEP:
            continue   # (the Developer rows: its help as it is)
        found.append(("name", texts[0]))
        found += [("text", t) for t in texts[1:]]
    for name in re.findall(r'\nconst char k(?:Update|Report)\w*Row\[\] = "([^"]+)";', src):
        found.append(("name", name))
    for m in re.finditer(r'\nconst char\* (tx_\w+|hn_\w+)\(int v[^)]*\) \{', src):
        kind = "value" if m.group(1).startswith("tx_") else "text"
        found += [(kind, t) for t in literal_runs(block(src, m.group(0).lstrip("\n")))]
    for m in re.finditer(r'\nMENU_HINTS\(\w+, -?\d+, (.*?)\)\n', src, re.S):
        found += [("text", t) for t in literal_runs(m.group(1))]
    for t in re.findall(r'\{ui::k\w+, "([^"]+)"', src):
        found.append(("value", t))
    for t in literal_runs(block(src, "const char* cheat_confirm(int i) {")):
        found.append(("value", t))
    for t in literal_runs(block(src, "const Control controls[] = {")):
        found.append(("control", t))
    for m in re.finditer(r'\bT\(((?:"(?:[^"\\\n]|\\.)*"\s*)+)\)', src):
        found += [("text", t) for t in literal_runs(m.group(1))]
    for m in re.finditer(r'\bT\(([^()]*\?[^()]*)\)', src):   # (T(cond ? "a" : "b"))
        found += [("text", t) for t in literal_runs(m.group(1))]
    seen, out = set(), []
    for kind, t in found:
        if (kind, t) not in seen:
            seen.add((kind, t))
            out.append((kind, t))
    return out


def needs_words(t):
    """A text with words to translate (not only numbers, sizes, signs, capitals, a format)"""
    return "%" not in t and re.search(r"[a-z]", re.sub(r"\d+x\d*", "", t)) is not None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--quiet", action="store_true", help="print only the failures")
    a = p.parse_args()
    src = strip_comments((ROOT / "gpu/vk/vk_menu.cpp").read_text(encoding="utf-8"))
    texts = collect(src)
    asked = [t for _, t in texts] + list(COMPOSED) + UNTOUCHED
    with tempfile.TemporaryDirectory(prefix="menu-pt-") as tmp:
        cpp, exe = Path(tmp) / "reader.cpp", Path(tmp) / "reader.exe"
        cpp.write_text(READER, encoding="utf-8")
        subprocess.run([a.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT), str(cpp), "-o", str(exe)],
                       check=True)
        r = subprocess.run([str(exe)], input="\n".join(asked) + "\n", capture_output=True, encoding="utf-8", check=True)
    answers = {}
    for en, line in zip(asked, r.stdout.splitlines()):
        ok, pt = line.split("\t", 1)
        answers[en] = (ok == "1", pt)
    fails = []
    for kind, en in texts:
        ok, pt = answers[en]
        if en in KEEP or not needs_words(en):
            continue
        if not ok:
            fails.append("no Portuguese for %s %r" % (kind, en))
            continue
        limit = {"name": NAME_MAX, "control": CONTROL_MAX, "value": VALUE_MAX}.get(kind)
        if limit and len(pt) > limit and len(pt) > len(en):
            fails.append("%s too long in Portuguese (%d > %d characters): %r -> %r" % (kind, len(pt), limit, en, pt))
    for en, want in COMPOSED.items():
        if answers[en] != (True, want):
            fails.append("composed text %r: %r, not %r" % (en, answers[en][1], want))
    for en in UNTOUCHED:
        if answers[en][0]:
            fails.append("a name or a Portuguese text was changed: %r -> %r" % (en, answers[en][1]))
    # (the table itself: no English text twice with two translations)
    table = (ROOT / "gpu/vk/vk_menu_pt.h").read_text(encoding="utf-8")
    pairs = re.findall(r'\{"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\}', table)
    firsts = {}
    for en, pt in pairs:
        if en in firsts and firsts[en] != pt:
            fails.append("the table has %r twice: %r and %r" % (en, firsts[en], pt))
        firsts.setdefault(en, pt)
    if not a.quiet:
        print("%-6s %4s %4s  %-40s %s" % ("kind", "en", "pt", "English", "Português"))
        for kind, en in texts:
            if kind in ("name", "control", "value"):
                ok, pt = answers[en]
                print("%-6s %4d %4d  %-40s %s" % (kind, len(en), len(pt), en, pt))
    # (the update's and the report's own texts, ocarina_update.cpp and ocarina_report.cpp: in the menu's language, English with
    # Portuguese beside it; the Auto language: the game's or Windows')
    if not re.search(r"bool menu_pt\(\) \{[^}]*game_pt\(\) \|\| windows_pt\(\)", src):
        fails.append("menu_pt: Auto is not the game's or Windows' language")
    if "GetUserDefaultUILanguage" not in src:
        fails.append("windows_pt does not ask Windows' display language")
    for module, words in (("ocarina_update.cpp", ("Check for updates", "Procurar atualização", "Update now", "Atualizar")),
                          ("ocarina_report.cpp", ("Make report", "Gerar relatório", "Open folder", "Abrir pasta"))):
        text = (ROOT / "gpu/vk" / module).read_text(encoding="utf-8")
        for w in words:
            if '"%s"' % w not in text:
                fails.append("%s has no %r" % (module, w))
        if "game_pt()" in text:
            fails.append("%s speaks the game's language, not the menu's (menu_portuguese)" % module)
    if fails:
        raise SystemExit("FAIL:\n  " + "\n  ".join(fails))
    names = sum(1 for k, _ in texts if k == "name")
    print("PASS: %d texts of the menu (%d names) in Portuguese, %d composed ones, none too long" % (len(texts), names,
                                                                                                    len(COMPOSED)))


if __name__ == "__main__":
    main()
