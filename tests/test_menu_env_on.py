"""A Debug row read before the menu loads (gpu/vk/vk_draw.cpp menu_env_on): Descriptor reuse and Sampler cache on by default from options_version 5.

The real menu_env_on from gpu/vk/vk_draw.cpp is compiled over a stub game path (the fixture's folder); each case writes a
hookshot_options.ini (or none) and checks the answer for descriptor_reuse and sampler_cache (both on by default since
options_version 5: an older file wrote its 0 as the off default it then was, or was a bench's):

- no file: the default; a file without the line: the default;
- options_version 3 or 4 with a row at 0: on (the old default, moved); 5 with 0: off (chosen); 5 with 1: on;
- RX_VK_DESC_SLOTGEN over all of it.

python -I -B tests/test_menu_env_on.py [--cxx clang++]
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

RUN_ALL_ARGS = ["--cxx", "g++"]

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


FIXTURE = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using DWORD = unsigned long;
constexpr int MAX_PATH = 260;
static DWORD game_module_path(char *path, DWORD size) { return snprintf(path, size, "./hookshot.exe"); }
BODY
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const bool reuse = menu_env_on("RX_VK_DESC_SLOTGEN", "descriptor_reuse", true, 5);
    const bool sampler = menu_env_on("RX_VK_SAMPLER_CACHE", "sampler_cache", true, 5);
    if (reuse != (atoi(argv[1]) != 0) || sampler != (atoi(argv[2]) != 0)) {
        fprintf(stderr, "FAIL: descriptor_reuse %d (want %s), sampler_cache %d (want %s)\n", reuse, argv[1], sampler, argv[2]);
        return 1;
    }
    return 0;
}
"""

# (hookshot_options.ini's text or None for no file, RX_VK_DESC_SLOTGEN or None, descriptor reuse, sampler cache)
CASES = (
    (None, None, 1, 1),
    ("output_res=1\noptions_version=4\n", None, 1, 1),
    ("output_res=1\noptions_version=5\n", None, 1, 1),
    ("descriptor_reuse=0\nsampler_cache=0\noptions_version=3\n", None, 1, 1),
    ("descriptor_reuse=0\nsampler_cache=0\n", None, 1, 1),
    ("descriptor_reuse=0\nsampler_cache=0\noptions_version=4\n", None, 1, 1),
    ("descriptor_reuse=0\nsampler_cache=0\noptions_version=5\n", None, 0, 0),
    ("descriptor_reuse=0\nsampler_cache=1\noptions_version=5\n", None, 0, 1),
    ("options_version=5\ndescriptor_reuse=1\nsampler_cache=0\n", None, 1, 0),
    ("descriptor_reuse=0\noptions_version=5\n", "1", 1, 1),
    ("descriptor_reuse=1\noptions_version=5\n", "0", 0, 1),
    (None, "0", 0, 1),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    draw = (ROOT / "gpu/vk/vk_draw.cpp").read_text(encoding="utf-8")
    body = block(draw, "bool menu_env_on(const char* env_name, const char* ini_key_name, bool def = false, int since = 0) {")
    with tempfile.TemporaryDirectory(prefix="menu-env-on-") as tmp:
        work = Path(tmp)
        source, exe = work / "fixture.cpp", work / "fixture.exe"
        source.write_text(FIXTURE.replace("BODY", body), encoding="utf-8")
        subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(exe)], check=True)
        for text, env_value, reuse, sampler in CASES:
            ini = work / "hookshot_options.ini"
            if ini.exists():
                ini.unlink()
            if text is not None:
                ini.write_text(text, encoding="ascii")
            env = os.environ.copy()
            env.pop("RX_VK_DESC_SLOTGEN", None)
            env.pop("RX_VK_SAMPLER_CACHE", None)
            if env_value is not None:
                env["RX_VK_DESC_SLOTGEN"] = env_value
            subprocess.run([str(exe), str(reuse), str(sampler)], cwd=work, env=env, check=True)
    print("PASS: Descriptor reuse and Sampler cache on by default and for options files before version 5; a version 5 choice "
          "and the environment kept")


if __name__ == "__main__":
    main()
