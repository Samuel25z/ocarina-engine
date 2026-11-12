// (ocarina) The Ocarina Engine's own look for the two screens it draws over the game (vk_menu.cpp): the start-up screen, where
// the shaders' preparation is a staff of ocarina notes lighting up one by one, and the Insert menu, its sections in a column
// at the left, its rows with no boxes, the selected one marked by a fairy's light. Night blue, the ocarina's blue, fairy gold,
// lantern cream and stone grey (vk_ui.h use_ocarina_style); Libertinus Serif for titles and sections, Libertinus Sans for the
// rest (vk_imgui.cpp: the game's own font is not used while the branding is on). Nothing here is the game's: no logo, font,
// icon or picture of it; the notes are plain shapes. Only vk_ui.h and Dear ImGui: nx2/ocarina_ui_preview.py draws these on
// the CPU, without the game, for pictures of them. RX_VK_BRAND=0 keeps Hookshot's own screens.
#pragma once

#include <cmath>
#include <string>

#include "vk_ui.h"

namespace mkwvk::ui {

/// a to b by t (0 to 1), each channel and alpha
inline ImU32 mix(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto ch = [&](int sh) {
        const float x = (float)(a >> sh & 0xFF), y = (float)(b >> sh & 0xFF);
        return (ImU32)std::clamp((int)(x + (y - x) * t + 0.5f), 0, 255) << sh;
    };
    return ch(IM_COL32_R_SHIFT) | ch(IM_COL32_G_SHIFT) | ch(IM_COL32_B_SHIFT) | ch(IM_COL32_A_SHIFT);
}

/// Text wrapped at wrap pixels, each line centred on cx, 1.35 em apart; its height
inline float para_centre(Ctx& c, ImFont* font, float px, float cx, float y0, ImU32 col, const char* t, float wrap) {
    const char* end = t + strlen(t);
    float y = 0;
    while (t < end) {
        const char* cut = font->CalcWordWrapPosition(px, t, end, wrap);
        if (cut == t) cut = t + 1;
        const char* line_end = cut;
        while (line_end > t && line_end[-1] == ' ') line_end--;
        const float lw = font->CalcTextSizeA(px, FLT_MAX, 0, t, line_end).x;
        c.d->AddText(font, px, ImVec2(cx - lw * 0.5f, y0 + y), col, t, line_end);
        y += px * 1.35f;
        t = cut;
        while (t < end && (*t == ' ' || *t == '\n')) t++;
    }
    return y > 0 ? y - px * 0.35f : 0;
}

// ---- the start-up screen ----

/// An ocarina note, a plain shape: 0 up, 1 left, 2 right, 3 down (triangles), 4 a round note; m its middle, r its half size
inline void ocarina_note(Ctx& c, ImVec2 m, float r, int kind, ImU32 col) {
    if (kind == 4) {
        c.d->AddCircleFilled(m, r * 0.86f, col, 24);
        return;
    }
    const float a = r, b = r * 0.86f;   // (a: half the base, b: from the middle to the tip and to the base)
    switch (kind) {
    case 0: c.d->AddTriangleFilled(m + ImVec2(0, -b), m + ImVec2(a, b * 0.8f), m + ImVec2(-a, b * 0.8f), col); break;
    case 3: c.d->AddTriangleFilled(m + ImVec2(0, b), m + ImVec2(-a, -b * 0.8f), m + ImVec2(a, -b * 0.8f), col); break;
    case 1: c.d->AddTriangleFilled(m + ImVec2(-b, 0), m + ImVec2(b * 0.8f, -a), m + ImVec2(b * 0.8f, a), col); break;
    default: c.d->AddTriangleFilled(m + ImVec2(b, 0), m + ImVec2(-b * 0.8f, a), m + ImVec2(-b * 0.8f, -a), col); break;
    }
}

/// The progress as a staff: five fine lines from x0 to x1, the first at y_top, gap apart, and ocarina notes along it, each on
/// its own line (the round note on the lowest, the up note on the highest), in a fixed order. The lines and the notes up to the
/// fraction are fairy gold, the notes with a soft glow; the rest stone, dimmed. The next note to light (the tip) pulses
/// slowly when animate; nothing else moves. fraction < 0: an empty staff
inline void ocarina_staff(Ctx& c, float x0, float x1, float y_top, float gap, double fraction, double t, bool animate) {
    static const int kTune[] = {1, 0, 2, 1, 0, 2, 3, 2, 1, 3, 2, 1, 4, 3, 0, 4, 3, 0, 2, 1, 4, 2, 1, 4};   // (the notes' order)
    constexpr int kTuneLen = sizeof kTune / sizeof *kTune;
    const float s = c.s, line_w = std::max(1.0f, 0.9f * s), f = (float)std::clamp(fraction, 0.0, 1.0);
    const float xp = fraction < 0 ? x0 : x0 + (x1 - x0) * f;   // (how far the gold has come)
    const ImU32 stone = alpha(kStone, 0.55f), lit_line = alpha(kFairy, 0.62f);
    for (int i = 0; i < 5; i++) {
        const float y = y_top + i * gap;
        if (xp > x0) c.d->AddLine(ImVec2(x0, y), ImVec2(xp, y), lit_line, line_w);
        if (xp < x1) c.d->AddLine(ImVec2(xp, y), ImVec2(x1, y), stone, line_w);
    }
    const bool full = fraction >= 1;
    const float bottom = y_top + 4 * gap;
    // (a bar line at the start, a double one at the end, as a staff closes)
    c.d->AddLine(ImVec2(x0, y_top), ImVec2(x0, bottom), fraction > 0 ? lit_line : stone, line_w);
    c.d->AddLine(ImVec2(x1 - 4.5f * s, y_top), ImVec2(x1 - 4.5f * s, bottom), full ? lit_line : stone, line_w);
    c.d->AddRectFilled(ImVec2(x1 - 2.2f * s, y_top), ImVec2(x1, bottom), full ? alpha(kFairy, 0.8f) : stone);
    // (the notes: as many as fit about 30 points apart, kept off the bar lines)
    const float pad = 26 * s, room = x1 - x0 - 2 * pad;
    const int n = std::clamp((int)(room / (30 * s)) + 1, 8, 40);
    const float r = gap * 0.95f;
    int tip = -1;
    for (int i = 0; i < n; i++) {
        const float x = x0 + pad + room * i / (float)(n - 1);
        if (fraction >= 0 && x > xp && tip < 0) tip = i;
    }
    for (int i = 0; i < n; i++) {
        const float x = x0 + pad + room * i / (float)(n - 1);
        const int kind = kTune[i % kTuneLen];
        const ImVec2 m(x, y_top + (kind == 0 ? 0 : kind == 1 ? 1 : kind == 2 ? 2 : kind == 3 ? 3 : 4) * gap);
        const bool lit = fraction >= 0 && x <= xp;
        if (lit) {
            for (int k = 10; k >= 1; k--) c.d->AddCircleFilled(m, r * (1.0f + k * 0.22f), alpha(kFairy, 0.022f), 24);
            ocarina_note(c, m, r, kind, kFairy);
        } else if (i == tip) {   // (the tip: from stone to gold and back, every 2.4 s)
            const float p = animate ? 0.5f - 0.5f * std::cos((float)(t * 6.2831853 / 2.4)) : 0.6f;
            for (int k = 10; k >= 1; k--) c.d->AddCircleFilled(m, r * (1.0f + k * 0.22f), alpha(kFairy, 0.022f * p), 24);
            ocarina_note(c, m, r, kind, mix(alpha(kStone, 0.75f), kFairyHi, p));
        } else {
            ocarina_note(c, m, r, kind, alpha(kStone, 0.5f));
        }
    }
}

/// What the start-up screen says (vk_menu.cpp makes it from StartupView, vk_progress.h brand_startup_text)
struct OcarinaStartup {
    std::string line;      // ("Preparing 73,638 of 120,566 shaders, just this once"; empty: the name alone)
    std::string eta;       // ("about 4 min left"; empty: not known yet)
    std::string hint;      // (what closing the window does, small)
    std::string version;   // ("Ocarina Engine v0.3.1", at the foot)
    double fraction = -1;  // (the staff, 0 to 1; negative: an empty staff)
    bool staff = false;    // (the staff is shown)
};

/// The background of both screens: night blue, lighter in the middle, darker toward the edges (not black)
inline void night(Ctx& c, ImVec2 a, ImVec2 b, ImU32 base, ImVec2 light_at, float light_r) {
    c.d->AddRectFilled(a, b, base);
    for (int i = 48; i >= 1; i--) c.d->AddCircleFilled(light_at, light_r * i / 48.0f, alpha(kNightLift, 0.024f), 96);
    vignette(c, a, b, kNightDeep, 0.22f);
}

/// The start-up screen: the ocarina, the name, the staff, a line of what is being done and the time left, a small hint, the
/// version at the foot. A column in the middle, a little above the centre
inline void ocarina_startup(Ctx& c, const OcarinaStartup& v, ImTextureData* logo, float aspect, double t, bool animate) {
    const float s = c.s, w = c.w, h = c.h, cx = w * 0.5f;
    const float lh = 92 * s, top = h * 0.5f - 200 * s;
    night(c, ImVec2(0, 0), ImVec2(w, h), kNight, ImVec2(cx, top + 210 * s), std::max(w, h) * 0.55f);
    if (logo) c.d->AddImage(logo->GetTexRef(), ImVec2(cx - lh * aspect * 0.5f, top), ImVec2(cx + lh * aspect * 0.5f, top + lh));
    float y = top + lh + 18 * s;
    spaced(c, c.f.heading, 30 * s, ImVec2(cx, y), kLantern, "OCARINA ENGINE", 7 * s);
    y += 30 * s + 52 * s;
    const float half = std::min(300 * s, (w - 2 * c.safe) * 0.5f - 20 * s), gap = 7.5f * s;
    if (v.staff) ocarina_staff(c, cx - half, cx + half, y, gap, v.fraction, t, animate);
    y += 4 * gap + 40 * s;
    if (!v.line.empty()) {
        const float px = 18 * s;
        text_mid_centre(c, c.f.body, px, cx, y, kLantern, v.line.c_str());
        y += 28 * s;
        if (!v.eta.empty()) text_mid_centre(c, c.f.body, 15.5f * s, cx, y, kStoneLight, v.eta.c_str());
        y += 34 * s;
        if (!v.hint.empty()) para_centre(c, c.f.body, 13.5f * s, cx, y, kStone, v.hint.c_str(), 2 * half);
    }
    if (!v.version.empty()) text_mid_centre(c, c.f.body, 13 * s, cx, h - c.safe - 10 * s, kStone, v.version.c_str());
}

// ---- the Insert menu ----

/// The menu's backdrop: the game behind it dimmed to night blue, darker toward the edges
inline void menu_backdrop(Ctx& c) {
    c.d->AddRectFilled(ImVec2(0, 0), ImVec2(c.w, c.h), alpha(kNight, 0.80f));
    vignette(c, ImVec2(0, 0), ImVec2(c.w, c.h), kNightDeep, 0.2f);
}

/// The menu's frame: the header's height, the sections' column (side: at the left, else a row under the header), the rows'
/// left and right ends and their top. One place for vk_menu.cpp and the preview
struct MenuFrame {
    float hh;              // (the header)
    bool side;             // (the sections in a column at the left; false: a row of them under the header, a narrow window)
    float sec_x0, sec_x1;  // (the sections' column, its hairline at sec_x1)
    float sec_top, sec_pitch, sec_pt;
    float rx0, rx1, top;   // (the rows)
};
inline MenuFrame menu_frame(const Ctx& c, float header_h, const char* const* titles, int n) {
    const float s = c.s;
    MenuFrame f{};
    f.hh = header_h;
    f.sec_pt = 26;
    float widest = 0;
    for (int i = 0; i < n; i++) widest = std::max(widest, size_of(c.f.heading, f.sec_pt * s, titles[i]).x);
    f.sec_x0 = c.safe + 18 * s;
    f.sec_x1 = f.sec_x0 + 18 * s + widest + 30 * s;
    f.rx1 = c.w - c.safe - 18 * s;
    f.side = f.rx1 - (f.sec_x1 + 40 * s) >= 600 * s;
    f.sec_pitch = 50 * s;
    if (f.side) {
        f.sec_top = f.hh + 34 * s;
        f.rx0 = f.sec_x1 + 40 * s;
        f.top = f.hh + 30 * s;
    } else {
        f.sec_top = f.hh + 8 * s;
        f.rx0 = c.safe + 18 * s;
        f.top = f.hh + 62 * s;
    }
    return f;
}

/// The header: the ocarina, "Ocarina Engine", and after it, smaller, the version and the credit; a fine line at its foot
inline void menu_header(Ctx& c, const MenuFrame& f, ImTextureData* icon, float aspect, const char* name, const char* sub) {
    const float s = c.s, mid = f.hh * 0.5f + 2 * s;
    float x = c.safe + 18 * s;
    if (icon) {
        const float ih = 32 * s;
        c.d->AddImage(icon->GetTexRef(), ImVec2(x, mid - ih * 0.5f), ImVec2(x + ih * aspect, mid + ih * 0.5f));
        x += ih * aspect + 12 * s;
    }
    x += text_mid(c, c.f.heading, 23 * s, x, mid, kLantern, name) + 14 * s;
    const float room = c.w - c.safe - 18 * s - x;
    if (sub && *sub && size_of(c.f.body, 13.5f * s, sub).x <= room) text_mid(c, c.f.body, 13.5f * s, x, mid + 1 * s, kStoneLight, sub);
    const float ly = f.hh - 0.5f * s;
    const ImU32 line = alpha(kStone, 0.35f);
    c.d->AddRectFilledMultiColor(ImVec2(c.safe, ly), ImVec2(c.w * 0.5f, ly + std::max(1.0f, s)), alpha(kStone, 0), line, line, alpha(kStone, 0));
    c.d->AddRectFilledMultiColor(ImVec2(c.w * 0.5f, ly), ImVec2(c.w - c.safe, ly + std::max(1.0f, s)), line, alpha(kStone, 0), alpha(kStone, 0), line);
}

/// One section's title: the shown one cream with a fine gold line beside it, the others stone (hover: lighter). Its middle at
/// (x, y) for the side column, or its centre for the row; its box in *a, *b for the mouse
inline void menu_section(Ctx& c, const MenuFrame& f, float x, float y, const char* title, bool shown, bool hover, ImVec2* a, ImVec2* b) {
    const float s = c.s, px = (f.side ? f.sec_pt : 22) * s, tw = size_of(c.f.heading, px, title).x;
    const ImU32 ink = shown ? kLantern : hover ? kStoneLight : kStone;
    if (f.side) {
        if (shown) c.d->AddRectFilled(ImVec2(x, y - 14 * s), ImVec2(x + std::max(1.5f, 2 * s), y + 14 * s), kFairy);
        text_mid(c, c.f.heading, px, x + 18 * s, y, ink, title);
        *a = ImVec2(x - 8 * s, y - f.sec_pitch * 0.5f), *b = ImVec2(f.sec_x1, y + f.sec_pitch * 0.5f);
    } else {
        text_mid(c, c.f.heading, px, x - tw * 0.5f, y, ink, title);
        if (shown) c.d->AddRectFilled(ImVec2(x - tw * 0.5f, y + 15 * s), ImVec2(x + tw * 0.5f, y + 15 * s + std::max(1.5f, 2 * s)), kFairy);
        *a = ImVec2(x - tw * 0.5f - 14 * s, y - 22 * s), *b = ImVec2(x + tw * 0.5f + 14 * s, y + 22 * s);
    }
}
/// The fine stone line between the sections' column and the rows
inline void menu_section_rule(Ctx& c, const MenuFrame& f, float bottom) {
    if (!f.side) return;
    const float s = c.s;
    c.d->AddRectFilledMultiColor(ImVec2(f.sec_x1, f.top), ImVec2(f.sec_x1 + std::max(1.0f, s), bottom), alpha(kStone, 0.3f), alpha(kStone, 0.3f),
                                 alpha(kStone, 0), alpha(kStone, 0));
}

// (the rows: a name at the left, its value at the right of the row; no boxes. The selected row: a fairy's light before its
// name, a fine gold line under the whole row; the others a faint stone line)
constexpr float kNameInset = 28;   // (points from the row's left end to its name: the fairy's place)
/// The room a name has in its cell (a to b), and where it starts
inline float row_name_room(const Ctx& c, ImVec2 a, ImVec2 b, bool pending) { return b.x - a.x - (kNameInset + 12 + (pending ? 14 : 0)) * c.s; }
inline void row_rule(Ctx& c, float x0, float x1, float y, bool sel) {
    const float s = c.s;
    if (sel) c.d->AddRectFilled(ImVec2(x0, y - 0.6f * s), ImVec2(x1, y + 0.7f * s), alpha(kFairy, 0.9f));
    else c.d->AddLine(ImVec2(x0, y), ImVec2(x1, y), alpha(kStone, 0.16f), std::max(1.0f, 0.8f * s));
}
/// A row's name cell; shown: the name as it fits (vk_menu.cpp fit); pending: its change applies after a restart (a small
/// gold diamond at its right end)
inline void row_name(Ctx& c, ImVec2 a, ImVec2 b, const char* shown, bool sel, bool pending, float px) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f;
    if (sel) fairy(c, ImVec2(a.x + 11 * s, mid), 3.2f * s);
    row_rule(c, a.x + kNameInset * s, b.x, b.y, sel);
    if (pending) diamond(c, ImVec2(b.x - 12 * s, mid), 3.5f * s, kFairy);
    text_mid(c, sel ? c.f.strong : c.f.body, px, a.x + kNameInset * s, mid, sel ? kInkBright : kLantern, shown);
}
/// The room a value has in its cell
inline float row_value_room(const Ctx& c, ImVec2 a, ImVec2 b, bool arrows) { return b.x - a.x - (arrows ? 44 : 12) * c.s; }
/// A value: the choice in the middle of its cell, small arrows at its ends where it can go that way
inline void row_choice(Ctx& c, ImVec2 a, ImVec2 b, const char* shown, bool sel, bool left, bool right, bool fixed) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f;
    row_rule(c, a.x, b.x, b.y, sel);
    const ImU32 arrow = sel && !fixed ? kFairy : alpha(kStone, 0.8f);
    if (left) triangle(c, ImVec2(a.x + 12 * s, mid), 8 * s, 0, arrow);
    if (right) triangle(c, ImVec2(b.x - 12 * s, mid), 8 * s, 1, arrow);
    const ImU32 ink = fixed ? kStone : sel ? kFairyHi : kLantern;
    text_mid_centre(c, sel && !fixed ? c.f.strong : c.f.body, 18 * s, (a.x + b.x) * 0.5f, mid, ink, shown);
}
/// A value of two (Off and On): both shown, the chosen one bright (gold on the selected row), the other stone
inline void row_halves(Ctx& c, ImVec2 a, ImVec2 b, const char* lo, const char* hi, bool high, bool sel, bool fixed) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f, xm = (a.x + b.x) * 0.5f;
    row_rule(c, a.x, b.x, b.y, sel);
    const ImU32 on = fixed ? kStoneLight : sel ? kFairyHi : kLantern, off = alpha(kStone, 0.85f);
    text_mid_centre(c, high ? c.f.body : c.f.strong, 18 * s, (a.x + xm) * 0.5f, mid, high ? off : on, lo);
    text_mid_centre(c, high ? c.f.strong : c.f.body, 18 * s, (xm + b.x) * 0.5f, mid, high ? on : off, hi);
    const float cx = high ? (xm + b.x) * 0.5f : (a.x + xm) * 0.5f;   // (a small mark under the chosen one)
    c.d->AddCircleFilled(ImVec2(cx, b.y - 7 * s), 1.6f * s, sel && !fixed ? kFairy : alpha(kLantern, 0.6f), 10);
}
/// A slider: the value, a fine track under it with the part done in gold and a small knob; arrows at its ends. *t0, *t1 the
/// track's ends (for the mouse)
inline void row_slider(Ctx& c, ImVec2 a, ImVec2 b, const char* value, int permille, bool sel, float* t0, float* t1) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f;
    row_rule(c, a.x, b.x, b.y, sel);
    const float x0 = a.x + 28 * s, x1 = b.x - 28 * s, xv = x0 + (x1 - x0) * permille / 1000.0f, ty = b.y - 8 * s;
    c.d->AddLine(ImVec2(x0, ty), ImVec2(x1, ty), alpha(kStone, 0.45f), std::max(1.0f, 1.2f * s));
    c.d->AddLine(ImVec2(x0, ty), ImVec2(xv, ty), sel ? kFairy : alpha(kLantern, 0.7f), std::max(1.0f, 1.6f * s));
    c.d->AddCircleFilled(ImVec2(xv, ty), 3.6f * s, sel ? kFairyHi : kLantern, 14);
    text_mid_centre(c, sel ? c.f.strong : c.f.body, 17 * s, (a.x + b.x) * 0.5f, mid - 4 * s, sel ? kFairyHi : kLantern, value);
    const ImU32 arrow = sel ? kFairy : alpha(kStone, 0.8f);
    triangle(c, ImVec2(a.x + 12 * s, mid), 8 * s, 0, arrow), triangle(c, ImVec2(b.x - 12 * s, mid), 8 * s, 1, arrow);
    *t0 = x0, *t1 = x1;
}
/// A section's heading among the rows: its title in serif, stone light, a fine line after it
inline void row_heading(Ctx& c, float x0, float x1, float y, float rh, const char* title) {
    const float s = c.s, ty = y + rh * 0.62f, tw = size_of(c.f.heading, 19 * s, title).x;
    text_mid(c, c.f.heading, 19 * s, x0 + kNameInset * s, ty, kStoneLight, title);
    if (x0 + kNameInset * s + tw + 16 * s < x1)
        c.d->AddLine(ImVec2(x0 + kNameInset * s + tw + 16 * s, ty), ImVec2(x1, ty), alpha(kStone, 0.3f), std::max(1.0f, 0.8f * s));
}

/// The help strip's height for this text at width (the same layout as help_strip): the help, the hint under it set off by a
/// blue bar, the restart note above, the pending line below; min_text: the text's height at least (the tallest help the page
/// has, so the strip keeps its height whichever row is chosen)
/// The help's line length in the strip: its width, at most 820 points (a long line is hard to read across a wide screen)
inline float help_wrap(const Ctx& c, float width) { return std::min(width - 4 * c.s, 820 * c.s); }
inline float help_strip_h(Ctx& c, float width, const char* help, const char* hint, bool after_restart, const char* gold_line, float min_text) {
    const float s = c.s, w = help_wrap(c, width);
    const float th = help ? para(c, c.f.body, 18 * s, ImVec2(), 0, help, w, false) : 0,
                hh = hint ? para(c, c.f.body, 16 * s, ImVec2(), 0, hint, w - 18 * s, false) : 0;
    const float text = std::max(min_text, th + (hint ? 12 * s + hh : 0));
    return 18 * s + (after_restart ? 28 * s : 0) + text + (gold_line ? 36 * s : 0) + 8 * s;
}
/// The help strip from (x0, y0) to (x1, y1): a fine gold line fading out at its top, then the text, no box
inline void help_strip(Ctx& c, float x0, float x1, float y0, float y1, const char* help, const char* hint, bool after_restart,
                       const char* gold_line) {
    const float s = c.s, w = help_wrap(c, x1 - x0);
    c.d->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y0 + std::max(1.0f, s)), alpha(kFairy, 0.7f), alpha(kFairy, 0), alpha(kFairy, 0),
                                 alpha(kFairy, 0.7f));
    c.d->PushClipRect(ImVec2(x0 - 4 * s, y0), ImVec2(x1 + 4 * s, y1), true);
    float y = y0 + 18 * s;
    if (after_restart) {
        diamond(c, ImVec2(x0 + 4 * s, y + 9 * s), 3.5f * s, kFairy);
        text_mid(c, c.f.strong, 15.5f * s, x0 + 16 * s, y + 9 * s, kFairyHi, c.restart_note);
        y += 28 * s;
    }
    float th = 0;
    if (help) th = para(c, c.f.body, 18 * s, ImVec2(x0, y), kLantern, help, w);
    if (hint) {
        const float hy = y + th + (help ? 12 * s : 0), hh = para(c, c.f.body, 16 * s, ImVec2(), 0, hint, w - 18 * s, false);
        c.d->AddRectFilled(ImVec2(x0, hy + 2 * s), ImVec2(x0 + std::max(1.5f, 2 * s), hy + hh - 2 * s), kOcarina);
        para(c, c.f.body, 16 * s, ImVec2(x0 + 16 * s, hy), kStoneLight, hint, w - 18 * s);
    }
    if (gold_line) {
        const float gy = y1 - 8 * s - 18 * s;
        diamond(c, ImVec2(x0 + 4 * s, gy), 3.5f * s, kFairy);
        text_mid(c, c.f.strong, 15.5f * s, x0 + 16 * s, gy, kFairyHi, gold_line);
    }
    c.d->PopClipRect();
}

} // namespace mkwvk::ui
