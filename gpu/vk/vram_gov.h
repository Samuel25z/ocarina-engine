// (Ocarina Engine, RX_VK_VRAM_GOV) The VRAM governor's rules (vk_texture.cpp's gov_look and compact_step), apart from Vulkan.
//
// One target for the driver's count of the process's VRAM (VK_EXT_memory_budget): RX_VK_VRAM_TARGET_PCT (88) % of what the
// driver allows it. A 6 GB RTX 4050 laptop in Hyrule Market sat at 5305 MB of the 5108 allowed, the driver putting the excess in
// system RAM, with 1005 MB of that count free space inside VMA's blocks: 352 MB in the textures' blocks, 477 MB in the render
// targets' (7 blocks of 128 MB holding 419 MB of images), 107 MB in the other images' one block. The textures in use then were
// what the texture budget's floor holds (1810 MB of 1906): what gives the most back is that free space, not the textures.
#pragma once

#include <algorithm>
#include <cstdint>

namespace vram_gov {

/// What a compaction leaves free in a pool of `blocks` blocks of `block` bytes: the empty block VMA may keep for the next image
/// while the count is under the driver's budget (until the pool's next free; a second one, and every empty one over the budget,
/// it frees at once), and a sixteenth of the pool that the moves leave between the images. A run for less gives back nothing
inline uint64_t slack(uint64_t block, uint32_t blocks) { return block + (uint64_t)blocks * block / 16; }

/// The free space a compaction of the pool can give back to the driver
inline uint64_t reclaimable(uint64_t free_bytes, uint64_t block, uint32_t blocks) {
    const uint64_t s = slack(block, blocks);
    return free_bytes > s ? free_bytes - s : 0;
}

/// The target: pct % of what the driver allows
inline uint64_t target(uint64_t allowed, uint64_t pct) { return allowed / 100 * pct; }

/// The state the governor holds between looks (every 10 presents): squeezing once the count has been over the target and a
/// percent of the allowance three looks in a row, and no longer once it has been two percent under the target three looks in a
/// row. In between, it stays as it was: what a run of the compaction gives back does not turn it off at once, and the
/// textures made again after it do not turn it on at once
struct Hysteresis {
    uint32_t over = 0, under = 0;
    bool squeeze = false;
    void look(uint64_t used, uint64_t allowed, uint64_t tgt) {
        const uint64_t band = allowed / 100;
        if (used > tgt + band) over++, under = 0;
        else if (used + 2 * band < tgt) under++, over = 0;
        else over = under = 0;
        if (over >= 3) squeeze = true;
        if (under >= 3) squeeze = false;
    }
    void reset() { over = under = 0, squeeze = false; }
};

/// The mirrors' lowest under the governor: RX_VK_VRAM_GOV_MIRROR_MIN_MB, else five sixths of their budget and never under
/// 1280 MB (at 1024 MB on a GTX 1660 Super the GPU's frame went from 13 to 21 ms, the game's memory read over PCIe); and the
/// most the governor may take off: none when their budget is the user's (RX_VK_MIRROR_MB)
inline uint64_t mirror_most_cut(uint64_t base, uint64_t env_min, bool user_budget) {
    if (user_budget || !base) return 0;
    const uint64_t floor = env_min ? env_min : std::max<uint64_t>(1280ull << 20, base / 6 * 5);
    return base > floor ? base - floor : 0;
}

}   // namespace vram_gov
