"""Compile and run the rule for the top levels small cards leave out of their textures (gpu/vk/tex_detail.h; no game/GPU)."""
import argparse
import subprocess
import tempfile
from pathlib import Path

RUN_ALL_ARGS = ["--cxx", "g++"]

ROOT = Path(__file__).resolve().parents[1]


def check_source():
    """The texture setup still asks the rule, the governor still tells it, and the copies still refuse it"""
    src = (ROOT / "gpu/vk/vk_texture.cpp").read_text(encoding="utf-8")
    for needle in ('#include "tex_detail.h"', "tex_detail::levels_off(", 'getenv("RX_VK_TEX_DETAIL")',
                   "detail_squeeze.store(gov.h.squeeze", "detail_forget(tex);", "!(x.skip > x.detail && x.layers == 1)"):
        if needle not in src:
            raise SystemExit("vk_texture.cpp no longer has: " + needle)
    xfer = (ROOT / "gpu/vk/vk_xfer.cpp").read_text(encoding="utf-8")
    if "texture_detail_refused(s.tex);" not in xfer:
        raise SystemExit("vk_xfer.cpp no longer refuses the cut on a copy into a level left out")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    check_source()
    with tempfile.TemporaryDirectory(prefix="tex-detail-") as tmp:
        exe = Path(tmp) / "tex-detail"
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/test_tex_detail.cpp"), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
