// (Ocarina Engine, RX_VK_TEX_DETAIL) How many of a texture's top mip levels stay out of VRAM on small cards (vk_texture.cpp's
// create), apart from Vulkan. tests/test_tex_detail.py
//
// A level left out is a quarter of what the texture keeps: one level off takes ~3/4 of it away, two ~15/16. The image is made
// with the levels from k on, its level 0 the game's level k (the "huge textures" path of create already does this: Texture::skip,
// level_geo, image_level, the views), and the upload stages only those. The game sampling at its usual coordinates gets what it
// would get at a distance; close up, a texture is at most half (one level) or a quarter (two) as sharp.
//
// Only decoded ASTC textures from the game's memory: the GPU never renders or stores into them, nothing copies into their top
// levels but the CPU (a buffer-to-texture copy of ASTC blocks goes into the storage, uploaded again), and the game's shaders read
// them as material textures (filtered, normalized coordinates), not with texelFetch or textureSize. 2D and 2D arrays, at least
// kMinSide texels on their longer side and kMinLevels levels (the UI's and the fonts' have one level); never cube maps (their
// explicit-LOD reads by roughness would come out a level blurrier), 3D textures, multisampled or linear ones.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace tex_detail {

constexpr int kAuto = -1;
constexpr uint32_t kMinSide = 512;     // (the longer side at level 0)
constexpr uint32_t kMinLevels = 4;     // (a mip chain: a world texture; the image keeps 2 levels at least)
constexpr uint32_t kBigSide = 2048;    // (8 GB cards: only these)
constexpr uint32_t kCoarseBlock = 36;  // (ASTC blocks of 6x6 texels and up: the world's; the finer ones are characters' and UI's)
constexpr uint64_t kSmallCard = 6656ull << 20;   // (allowed up to 6.5 GB: the 6 GB cards, ~5.1-5.2 GB allowed, and 4 GB ones)
constexpr uint64_t kMidCard = 8704ull << 20;     // (up to 8.5 GB: the 8 GB cards, as astc_bc_policy)

/// RX_VK_TEX_DETAIL: unset or "auto" -> kAuto; "0" or "full" -> 0; "1", "2" -> that many levels off
inline int parse(const char* e) {
    if (!e || !*e || !std::strcmp(e, "auto")) return kAuto;
    if (!std::strcmp(e, "full")) return 0;
    if (e[0] >= '0' && e[0] <= '2' && !e[1]) return e[0] - '0';
    return kAuto;
}

/// What auto takes off by the VRAM the driver allows: 1 level on small cards (2 only with the governor squeezing), the big
/// textures only on 8 GB ones, nothing above
inline const char* tier_name(uint64_t allowed) {
    return !allowed ? "VRAM unknown" : allowed <= kSmallCard ? "a small card" : allowed <= kMidCard ? "an 8 GB card" : "a large card";
}

/// The levels to leave out of a texture that may lose some (eligible: see above), `w` x `h` with `levels` levels at level 0,
/// in ASTC blocks of `block_texels` texels. `squeeze`: the VRAM governor is squeezing (auto only: the coarse-block textures made
/// meanwhile lose one more, up to 2)
inline uint32_t levels_off(int mode, uint64_t allowed, bool squeeze, uint32_t w, uint32_t h, uint32_t levels,
                           uint32_t block_texels) {
    const uint32_t side = std::max(w, h);
    if (side < kMinSide || levels < kMinLevels || mode == 0) return 0;
    uint32_t k = 0;
    if (mode > 0) {
        k = (uint32_t)mode;
    } else {
        if (!allowed || allowed > kMidCard) return 0;
        k = allowed <= kSmallCard ? 1 : side >= kBigSide ? 1 : 0;
        if (squeeze && block_texels >= kCoarseBlock) k++;
        k = std::min<uint32_t>(k, 2);
    }
    return std::min(k, levels - 2);
}

}   // namespace tex_detail
