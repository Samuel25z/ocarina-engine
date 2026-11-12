// tex_detail (gpu/vk/tex_detail.h): how many top levels small cards leave out of the textures that may lose them (RX_VK_TEX_DETAIL).
// tests/test_tex_detail.py
#include "../gpu/vk/tex_detail.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

int main() {
    const uint64_t MB = 1ull << 20;
    using tex_detail::kAuto;
    using tex_detail::levels_off;
    using tex_detail::parse;

    expect(parse(nullptr) == kAuto && parse("") == kAuto && parse("auto") == kAuto, "unset or auto: auto");
    expect(parse("0") == 0 && parse("full") == 0, "0 or full: every level");
    expect(parse("1") == 1 && parse("2") == 2, "1, 2: that many levels");
    expect(parse("3") == kAuto && parse("12") == kAuto && parse("x") == kAuto, "anything else: auto");

    const uint64_t laptop4050 = 5108 * MB, gtx1660s = 5300 * MB, rx7700s = 7133 * MB, card12 = 11200 * MB;
    // (a 1024 x 1024 world texture in ASTC 8x8, 11 levels; a 1024 character texture in ASTC 4x4)
    expect(levels_off(kAuto, laptop4050, false, 1024, 1024, 11, 64) == 1, "6 GB laptop: one level off");
    expect(levels_off(kAuto, gtx1660s, false, 1024, 1024, 11, 64) == 1, "GTX 1660 Super: one level off");
    expect(levels_off(kAuto, laptop4050, true, 1024, 1024, 11, 64) == 2, "6 GB, squeezing: two off the world's");
    expect(levels_off(kAuto, laptop4050, true, 1024, 1024, 11, 16) == 1, "6 GB, squeezing: still one off a 4x4 one");
    expect(levels_off(kAuto, laptop4050, false, 256, 256, 9, 64) == 0, "under 512 texels: kept");
    expect(levels_off(kAuto, laptop4050, false, 512, 128, 10, 64) == 1, "512 on its longer side: cut");
    expect(levels_off(kAuto, laptop4050, false, 1024, 1024, 1, 64) == 0, "no mip chain (UI): kept");
    expect(levels_off(kAuto, laptop4050, false, 1024, 1024, 3, 64) == 0, "three levels: kept");
    expect(levels_off(kAuto, laptop4050, true, 1024, 1024, 4, 64) == 2, "four levels, two off: two kept");
    expect(levels_off(2, laptop4050, false, 1024, 1024, 4, 64) == 2, "RX_VK_TEX_DETAIL=2, four levels: two kept");
    expect(levels_off(kAuto, rx7700s, false, 1024, 1024, 11, 64) == 0, "8 GB: a 1024 one kept");
    expect(levels_off(kAuto, rx7700s, false, 2048, 2048, 12, 64) == 1, "8 GB: a 2048 one, one level off");
    expect(levels_off(kAuto, rx7700s, true, 1024, 1024, 11, 64) == 1, "8 GB squeezing: a 1024 world one, one off");
    expect(levels_off(kAuto, rx7700s, true, 1024, 1024, 11, 16) == 0, "8 GB squeezing: a 1024 4x4 one kept");
    expect(levels_off(kAuto, card12, true, 4096, 4096, 13, 64) == 0, "12 GB: every level, squeezing or not");
    expect(levels_off(kAuto, 0, false, 4096, 4096, 13, 64) == 0, "VRAM unknown: every level");
    expect(levels_off(0, laptop4050, true, 4096, 4096, 13, 64) == 0, "RX_VK_TEX_DETAIL=0: every level");
    expect(levels_off(1, card12, false, 1024, 1024, 11, 16) == 1, "RX_VK_TEX_DETAIL=1 on any card");
    expect(levels_off(2, card12, false, 1024, 1024, 11, 16) == 2, "RX_VK_TEX_DETAIL=2 on any card");
    expect(levels_off(1, laptop4050, true, 1024, 1024, 11, 64) == 1, "RX_VK_TEX_DETAIL=1: the governor adds nothing");
    if (failures) return EXIT_FAILURE;
    std::printf("tex detail: all checks passed\n");
    return EXIT_SUCCESS;
}
