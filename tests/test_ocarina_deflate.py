"""(ocarina) The light performance reports' deflate (gpu/vk/ocarina_deflate.h): what it writes, Python's zlib reads back byte for
byte (raw deflate, as a ZIP's method 8), for text like the log and the frame CSV, for random bytes, runs, an empty input and
inputs longer than a block; and the text comes out small (the log at most ~1.3x zlib level 6's size). Prints the sizes.

    python -I -B tests/test_ocarina_deflate.py [--cxx g++] [--quiet]
"""
import argparse
import os
import random
import subprocess
import tempfile
import zlib
from pathlib import Path

RUN_ALL_ARGS = ["--cxx", "{cxx}", "--quiet"]

ROOT = Path(__file__).resolve().parents[1]

READER = r'''#include "gpu/vk/ocarina_deflate.h"
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i += 2) {
        FILE* f = std::fopen(argv[i], "rb");
        std::string in;
        char b[65536];
        for (size_t n; f && (n = std::fread(b, 1, sizeof b, f)) > 0;) in.append(b, n);
        if (f) std::fclose(f);
        const std::string out = mkwvk::ocarina_deflate::deflate_raw(in);
        FILE* o = std::fopen(argv[i + 1], "wb");
        std::fwrite(out.data(), 1, out.size(), o);
        std::fclose(o);
    }
}
'''


def samples():
    rnd = random.Random(7)
    log_lines = []
    for k in range(6000):
        kind = k % 5
        if kind == 0:
            log_lines.append("[hos Presentation Thread] vk: a frame's replay time (ms): submit %.2f, present %.2f, draw %.2f, memory "
                             "sync %.2f, queue submit %.2f" % (rnd.uniform(0, 4), rnd.uniform(0, 1), rnd.uniform(0, 2),
                                                               rnd.uniform(0, 20), rnd.uniform(0, 1)))
        elif kind == 1:
            log_lines.append("[hos Presentation Thread] vk: %d presents, %.1f fps; a frame: %d draws (0 skipped), %d dispatches"
                             % (600 * k, rnd.uniform(20, 60), rnd.randint(1000, 3000), rnd.randint(10, 200)))
        elif kind == 2:
            log_lines.append("[hos sead::MainThread] vk: slow submit: %.1f ms: memory sync %.1f" % (rnd.uniform(15, 300),
                                                                                                 rnd.uniform(15, 300)))
        elif kind == 3:
            log_lines.append("[hos Presentation Thread] vk: VRAM: %d MB used of 5152 MB allowed (%d MB of it free in the blocks)"
                             % (rnd.randint(1000, 5000), rnd.randint(0, 500)))
        else:
            log_lines.append("[hos Presentation Thread] vk: RAM guard: level %d (commit %d); commit left %d of 35279 MB"
                             % (rnd.randint(0, 2), rnd.randint(0, 2), rnd.randint(300, 9000)))
    frames = []
    for k in range(18000):
        ms = 16.6 + rnd.gauss(0, 0.6) + (rnd.random() < 0.01) * rnd.uniform(50, 400)
        frames.append("%d,%.1f,%.1f,,%.1f,%.1f,,,,,,,,,%.1f,%.1f,," % (100000 + k, ms, rnd.uniform(8, 12), rnd.uniform(0.5, 1.2),
                                                                   rnd.uniform(0, .3), rnd.uniform(0, 1), rnd.uniform(0, .3)))
    return {
        "empty": b"",
        "one": b"x",
        "two": b"ab",
        "run": b"a" * 100000,
        "zeros": bytes(70000),
        "random": bytes(rnd.getrandbits(8) for _ in range(200000)),
        "short text": "Relatório de desempenho do Ocarina Engine: travadas e FPS.\r\n".encode("utf-8") * 3,
        "log": ("\n".join(log_lines) + "\n").encode("utf-8"),
        "frames": ("\n".join(frames) + "\n").encode("utf-8"),
        "mixed": b"".join(bytes([rnd.choice(b"ab\x00\xff")]) * rnd.randint(1, 300) for _ in range(3000)),
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--quiet", action="store_true")
    a = p.parse_args()
    data = samples()
    fails = []
    with tempfile.TemporaryDirectory(prefix="ocarina-deflate-") as tmp:
        t = Path(tmp)
        cpp, exe = t / "d.cpp", t / "d.exe"
        cpp.write_text(READER, encoding="utf-8")
        subprocess.run([a.cxx, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT), str(cpp), "-o", str(exe)],
                       check=True)
        args = []
        for k, (name, body) in enumerate(data.items()):
            (t / ("in%d" % k)).write_bytes(body)
            args += [str(t / ("in%d" % k)), str(t / ("out%d" % k))]
        subprocess.run([str(exe)] + args, check=True)
        for k, (name, body) in enumerate(data.items()):
            out = (t / ("out%d" % k)).read_bytes()
            try:
                back = zlib.decompress(out, -15)
            except zlib.error as e:
                fails.append("%s: zlib could not read it (%s)" % (name, e))
                continue
            if back != body:
                fails.append("%s: read back %d bytes, not the %d written" % (name, len(back), len(body)))
                continue
            z6 = len(zlib.compress(body, 6)) - 6
            if not a.quiet:
                print("%-11s %8d bytes -> %7d (zlib level 6: %7d, %.2fx)" % (name, len(body), len(out), z6,
                                                                              len(out) / max(z6, 1)))
            if name in ("log", "frames") and len(out) > 1.3 * z6:
                fails.append("%s: %d bytes, more than 1.3x zlib's %d" % (name, len(out), z6))
    if fails:
        raise SystemExit("FAIL:\n  " + "\n  ".join(fails))
    print("PASS: %d inputs deflated and read back by zlib" % len(data))


if __name__ == "__main__":
    main()
