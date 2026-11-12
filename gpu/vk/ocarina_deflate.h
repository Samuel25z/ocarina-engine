// (ocarina) Deflate (RFC 1951) without a library, for the light performance reports (ocarina_report.cpp): LZ77 over a 32 KB
// window (hash chains, one step of lazy matching), each block with Huffman codes of its own (length-limited, canonical). Text
// (the log, the frame CSV) comes out at about a tenth of its size, near zlib's level 6. deflate_raw gives the raw stream: what
// a ZIP's method 8 entry holds. tests/test_ocarina_deflate.py checks it against Python's zlib
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mkwvk::ocarina_deflate {

namespace detail {

struct Bits {
    std::string out;
    uint64_t acc = 0;
    int n = 0;
    void put(uint32_t v, int bits) {   // (LSB first)
        acc |= (uint64_t)v << n;
        n += bits;
        while (n >= 8) out += (char)(acc & 0xFF), acc >>= 8, n -= 8;
    }
    void flush() {
        if (n > 0) out += (char)(acc & 0xFF);
        acc = 0, n = 0;
    }
};

inline const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131,
                                      163, 195, 227, 258};
inline const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
inline const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
                                       2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
inline const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
inline const uint8_t kClOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

inline int len_code(int len) {   // (3..258: 0..28)
    int c = 28;
    while (c > 0 && kLenBase[c] > len) c--;
    return c;
}
inline int dist_code(int d) {   // (1..32768: 0..29)
    int c = 29;
    while (c > 0 && kDistBase[c] > d) c--;
    return c;
}

/// Huffman code lengths for freq[0..n), none longer than limit, the code complete (Kraft sum 1: zlib's inflate refuses an
/// incomplete one); at least two symbols get a length (a symbol with no count gets one when fewer than two have)
inline void lengths(const uint32_t* freq, int n, int limit, uint8_t* len) {
    std::memset(len, 0, n);
    std::vector<int> syms;
    for (int i = 0; i < n; i++)
        if (freq[i]) syms.push_back(i);
    for (int i = 0; syms.size() < 2 && i < n; i++)
        if (std::find(syms.begin(), syms.end(), i) == syms.end()) syms.push_back(i);
    struct Node {
        uint64_t w;
        int parent;
    };
    std::vector<Node> nodes;
    std::vector<std::pair<uint64_t, int>> heap;   // (weight, node), a min-heap
    for (int s : syms) {
        nodes.push_back({std::max<uint64_t>(freq[s], 1), -1});
        heap.push_back({nodes.back().w, (int)nodes.size() - 1});
    }
    const auto greater = [](const std::pair<uint64_t, int>& a, const std::pair<uint64_t, int>& b) { return a > b; };
    std::make_heap(heap.begin(), heap.end(), greater);
    while (heap.size() > 1) {
        std::pop_heap(heap.begin(), heap.end(), greater);
        const auto a = heap.back();
        heap.pop_back();
        std::pop_heap(heap.begin(), heap.end(), greater);
        const auto b = heap.back();
        heap.pop_back();
        nodes.push_back({a.first + b.first, -1});
        const int p = (int)nodes.size() - 1;
        nodes[a.second].parent = nodes[b.second].parent = p;
        heap.push_back({nodes[p].w, p});
        std::push_heap(heap.begin(), heap.end(), greater);
    }
    for (size_t i = 0; i < syms.size(); i++) {
        int d = 0;
        for (int j = (int)i; nodes[j].parent >= 0; j = nodes[j].parent) d++;
        len[syms[i]] = (uint8_t)std::min(d, 255);
    }
    // (too long: clamped, then longer codes elsewhere until the Kraft sum is 1 or less; then shorter ones while it is under 1)
    const uint64_t one = 1ull << limit;
    uint64_t kraft = 0;
    for (int s : syms) {
        if (len[s] > limit) len[s] = (uint8_t)limit;
        kraft += 1ull << (limit - len[s]);
    }
    while (kraft > one) {   // (the deepest code under the limit one longer: the smallest loss)
        int best = -1;
        for (int s : syms)
            if (len[s] < limit && (best < 0 || len[s] > len[best])) best = s;
        kraft -= 1ull << (limit - len[best] - 1);
        len[best]++;
    }
    while (kraft < one) {   // (the deepest code that fits one shorter)
        int best = -1;
        for (int s : syms)
            if (len[s] > 1 && kraft + (1ull << (limit - len[s])) <= one && (best < 0 || len[s] > len[best])) best = s;
        if (best < 0) break;
        kraft += 1ull << (limit - len[best]);
        len[best]--;
    }
}

/// Canonical codes (RFC 1951 3.2.2), bit-reversed for the LSB-first stream
inline void codes(const uint8_t* len, int n, uint16_t* code) {
    uint16_t count[16] = {}, next[16] = {};
    for (int i = 0; i < n; i++) count[len[i]]++;
    count[0] = 0;
    uint16_t c = 0;
    for (int b = 1; b < 16; b++) c = (uint16_t)((c + count[b - 1]) << 1), next[b] = c;
    for (int i = 0; i < n; i++) {
        if (!len[i]) {
            code[i] = 0;
            continue;
        }
        uint16_t v = next[len[i]]++, r = 0;
        for (int k = 0; k < len[i]; k++) r = (uint16_t)(r << 1 | (v >> k & 1));
        code[i] = r;
    }
}

struct Sym {
    uint16_t lit_or_len;   // (0..255 a byte; 257..285 a length's code)
    uint16_t dist;         // (0 for a byte; else the distance, 1..32768)
    uint16_t len;          // (the match's length, 3..258)
};

inline void block(Bits& bw, const std::vector<Sym>& syms, bool last) {
    uint32_t lf[286] = {}, df[30] = {};
    for (const Sym& s : syms) {
        lf[s.lit_or_len]++;
        if (s.dist) df[dist_code(s.dist)]++;
    }
    lf[256] = 1;
    uint8_t ll[286], dl[30];
    lengths(lf, 286, 15, ll);
    lengths(df, 30, 15, dl);
    int hlit = 286, hdist = 30;
    while (hlit > 257 && !ll[hlit - 1]) hlit--;
    while (hdist > 1 && !dl[hdist - 1]) hdist--;
    // (the code lengths, run-length coded: 16 repeats the last 3-6 times, 17 zeros 3-10, 18 zeros 11-138)
    std::vector<uint8_t> all(ll, ll + hlit);
    all.insert(all.end(), dl, dl + hdist);
    std::vector<std::pair<uint8_t, uint8_t>> rle;   // (symbol, extra value)
    for (size_t i = 0; i < all.size();) {
        size_t j = i;
        while (j < all.size() && all[j] == all[i]) j++;
        size_t run = j - i;
        if (all[i] == 0) {
            while (run >= 11) {
                const size_t k = std::min<size_t>(run, 138);
                rle.push_back({18, (uint8_t)(k - 11)}), run -= k;
            }
            if (run >= 3) rle.push_back({17, (uint8_t)(run - 3)}), run = 0;
            while (run--) rle.push_back({0, 0});
        } else {
            rle.push_back({all[i], 0}), run--;
            while (run >= 3) {
                const size_t k = std::min<size_t>(run, 6);
                rle.push_back({16, (uint8_t)(k - 3)}), run -= k;
            }
            while (run--) rle.push_back({all[i], 0});
        }
        i = j;
    }
    uint32_t cf[19] = {};
    for (const auto& r : rle) cf[r.first]++;
    uint8_t cl[19];
    lengths(cf, 19, 7, cl);
    int hclen = 19;
    while (hclen > 4 && !cl[kClOrder[hclen - 1]]) hclen--;
    uint16_t lc[286], dc[30], cc[19];
    codes(ll, 286, lc);
    codes(dl, 30, dc);
    codes(cl, 19, cc);
    bw.put(last ? 1 : 0, 1);
    bw.put(2, 2);
    bw.put((uint32_t)(hlit - 257), 5), bw.put((uint32_t)(hdist - 1), 5), bw.put((uint32_t)(hclen - 4), 4);
    for (int i = 0; i < hclen; i++) bw.put(cl[kClOrder[i]], 3);
    for (const auto& r : rle) {
        bw.put(cc[r.first], cl[r.first]);
        if (r.first == 16) bw.put(r.second, 2);
        else if (r.first == 17) bw.put(r.second, 3);
        else if (r.first == 18) bw.put(r.second, 7);
    }
    for (const Sym& s : syms) {
        bw.put(lc[s.lit_or_len], ll[s.lit_or_len]);
        if (!s.dist) continue;
        const int lcode = s.lit_or_len - 257;
        if (kLenExtra[lcode]) bw.put((uint32_t)(s.len - kLenBase[lcode]), kLenExtra[lcode]);
        const int d = dist_code(s.dist);
        bw.put(dc[d], dl[d]);
        if (kDistExtra[d]) bw.put((uint32_t)(s.dist - kDistBase[d]), kDistExtra[d]);
    }
    bw.put(lc[256], ll[256]);
}

} // namespace detail

/// The raw deflate stream of in (no zlib or gzip header); max_chain: how many earlier places are tried for each match
inline std::string deflate_raw(const std::string& in, int max_chain = 48) {
    using namespace detail;
    Bits bw;
    const size_t n = in.size();
    const unsigned char* p = (const unsigned char*)in.data();
    if (!n) {   // (one final block, fixed codes, only its end)
        bw.put(1, 1), bw.put(1, 2), bw.put(0, 7);
        bw.flush();
        return bw.out;
    }
    constexpr int kHashBits = 15, kWindow = 32768, kMaxLen = 258, kGood = 32;
    std::vector<int32_t> head(1u << kHashBits, -1), prev(kWindow, -1);
    const auto hash = [&](size_t i) { return ((uint32_t)p[i] << 10 ^ (uint32_t)p[i + 1] << 5 ^ p[i + 2]) & ((1u << kHashBits) - 1); };
    const auto insert = [&](size_t i) {
        if (i + 2 >= n) return;
        const uint32_t h = hash(i);
        prev[i & (kWindow - 1)] = head[h];
        head[h] = (int32_t)i;
    };
    const auto longest = [&](size_t i, int* dist) {
        int best = 0;
        if (i + 2 >= n) return 0;
        const int most = (int)std::min<size_t>(kMaxLen, n - i);
        int chain = max_chain;
        for (int32_t c = head[hash(i)]; c >= 0 && i - (size_t)c <= (size_t)kWindow - 1 && chain-- > 0; c = prev[c & (kWindow - 1)]) {
            if (p[c + best] != p[i + best] || p[c] != p[i]) continue;
            int l = 0;
            while (l < most && p[c + l] == p[i + l]) l++;
            if (l > best) {
                best = l, *dist = (int)(i - c);
                if (l >= most) break;
                if (l >= kGood) chain >>= 1;
            }
        }
        return best >= 3 ? best : 0;
    };
    std::vector<Sym> syms;
    syms.reserve(1u << 16);
    size_t i = 0;
    while (i < n) {
        int d = 0;
        int l = longest(i, &d);
        if (l && l < kMaxLen && i + 1 < n) {   // (lazy: a longer match one byte on wins, this byte a literal)
            int d2 = 0;
            const int l2 = longest(i + 1, &d2);
            if (l2 > l) l = 0;
        }
        if (l) {
            syms.push_back({(uint16_t)(257 + len_code(l)), (uint16_t)d, (uint16_t)l});
            for (int k = 0; k < l; k++) insert(i + k);
            i += l;
        } else {
            syms.push_back({p[i], 0, 0});
            insert(i);
            i++;
        }
        if (syms.size() >= (1u << 16) - 1) block(bw, syms, false), syms.clear();
    }
    block(bw, syms, true);
    bw.flush();
    return bw.out;
}

} // namespace mkwvk::ocarina_deflate
