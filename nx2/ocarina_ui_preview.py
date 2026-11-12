"""Pictures of the Ocarina Engine's two screens without the game (gpu/vk/ocarina_ui.h): the start-up screen at a few moments of
the shaders' preparation, in Portuguese and English, and a likeness of the Insert menu drawn with the same pieces (the menu's own
rows and help come from the game: in it, Insert or F1 shows the real one). Dear ImGui draws them and a small rasterizer here
turns its triangles into PNG files, on the CPU: no GPU, no game, no Vulkan.

    python nx2/ocarina_ui_preview.py [out_dir] [--cxx path/to/clang++] [--size 1920x1080]

out_dir defaults to work/ocarina_ui_preview. The C++ compiler defaults to the Hookshot builder's llvm-mingw clang++, else clang++.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
VK = os.path.join(ROOT, "gpu", "vk")
IMGUI = os.path.join(ROOT, "third_party", "imgui")

STUB_IMGUI_H = r'''#pragma once
// (the preview's stand-in for gpu/vk/vk_imgui.h: the fonts only, no Vulkan)
struct ImFont;
struct ImTextureData;
namespace mkwvk::ui {
struct Fonts {
    ImFont* body = nullptr;
    ImFont* strong = nullptr;
    ImFont* heading = nullptr;
};
}
'''

PREVIEW_CPP = r'''#define IMGUI_DEFINE_MATH_OPERATORS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include "imgui.h"
#include "imgui_internal.h"
#include "ocarina_ui.h"
#include "vk_progress.h"
#include "ocarina_brand.h"
#include "ocarina_logo.h"

extern const unsigned hookshot_font_body_size, hookshot_font_strong_size, hookshot_font_heading_size;
extern const unsigned char hookshot_font_body[], hookshot_font_strong[], hookshot_font_heading[];

using namespace mkwvk;
namespace u = mkwvk::ui;

static ImTextureData* logo = nullptr;
static u::Fonts fonts;

static ImFont* font(const unsigned char* data, unsigned size) {
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)data, (int)size, 18.0f, &cfg);
}

// ---- a small rasterizer for ImGui's draw data: triangles, bilinear texture, colour per vertex, blended over ----
struct Image {
    int w, h;
    std::vector<float> px;   // (RGB, 0 to 1)
};
static void sample(ImTextureData* t, float u0, float v0, float out[4]) {
    const float x = u0 * t->Width - 0.5f, y = v0 * t->Height - 0.5f;
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    const float fx = x - x0, fy = y - y0;
    float acc[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; k++) {
        const int xi = std::clamp(x0 + (k & 1), 0, t->Width - 1), yi = std::clamp(y0 + (k >> 1), 0, t->Height - 1);
        const float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
        const unsigned char* p = (const unsigned char*)t->GetPixelsAt(xi, yi);
        for (int c = 0; c < 4; c++) acc[c] += p[c] / 255.0f * wgt;
    }
    for (int c = 0; c < 4; c++) out[c] = acc[c];
}
static void raster(Image& im, ImDrawData* dd) {
    for (ImDrawList* list : dd->CmdLists) {
        for (const ImDrawCmd& cmd : list->CmdBuffer) {
            if (cmd.UserCallback) continue;
            ImTextureData* tex = cmd.TexRef._TexData;
            const int cx0 = std::max(0, (int)cmd.ClipRect.x), cy0 = std::max(0, (int)cmd.ClipRect.y);
            const int cx1 = std::min(im.w, (int)std::ceil(cmd.ClipRect.z)), cy1 = std::min(im.h, (int)std::ceil(cmd.ClipRect.w));
            for (unsigned e = 0; e + 2 < cmd.ElemCount; e += 3) {
                const ImDrawVert* v[3];
                for (int k = 0; k < 3; k++) v[k] = &list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + e + k]];
                const double area = ((double)v[1]->pos.x - v[0]->pos.x) * (v[2]->pos.y - v[0]->pos.y) - (v[1]->pos.y - v[0]->pos.y) * (v[2]->pos.x - v[0]->pos.x);
                if (std::fabs(area) < 1e-8f) continue;
                const int bx0 = std::max(cx0, (int)std::floor(std::min({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x})));
                const int by0 = std::max(cy0, (int)std::floor(std::min({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y})));
                const int bx1 = std::min(cx1, (int)std::ceil(std::max({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x})));
                const int by1 = std::min(cy1, (int)std::ceil(std::max({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y})));
                float col[3][4];
                for (int k = 0; k < 3; k++)
                    for (int c = 0; c < 4; c++) col[k][c] = (v[k]->col >> (8 * c) & 0xFF) / 255.0f;
                for (int y = by0; y < by1; y++) {
                    for (int x = bx0; x < bx1; x++) {
                        const double px = x + 0.5, py = y + 0.5;
                        double w0 = (((double)v[1]->pos.x - px) * (v[2]->pos.y - py) - (v[1]->pos.y - py) * (v[2]->pos.x - px)) / area;
                        double w1 = (((double)v[2]->pos.x - px) * (v[0]->pos.y - py) - (v[2]->pos.y - py) * (v[0]->pos.x - px)) / area;
                        double w2 = 1 - w0 - w1;
                        if (w0 < -1e-9 || w1 < -1e-9 || w2 < -1e-9) continue;
                        float c[4];
                        for (int k = 0; k < 4; k++) c[k] = col[0][k] * w0 + col[1][k] * w1 + col[2][k] * w2;
                        if (tex) {
                            const float uu = v[0]->uv.x * w0 + v[1]->uv.x * w1 + v[2]->uv.x * w2, vv = v[0]->uv.y * w0 + v[1]->uv.y * w1 + v[2]->uv.y * w2;
                            float t[4];
                            sample(tex, uu, vv, t);
                            for (int k = 0; k < 4; k++) c[k] *= t[k];
                        }
                        float* d = &im.px[((size_t)y * im.w + x) * 3];
                        for (int k = 0; k < 3; k++) d[k] = c[k] * c[3] + d[k] * (1 - c[3]);
                    }
                }
            }
        }
    }
}
static void shoot(const char* path, int w, int h, const std::function<void(u::Ctx&)>& draw) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DeltaTime = 1.0f / 60;
    ImGui::NewFrame();
    u::Ctx c{ImGui::GetForegroundDrawList(), fonts, 1, (float)w, (float)h, 0, false};
    c.s = u::unit((float)h, 2, &c.safe);
    draw(c);
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    if (dd->Textures)
        for (ImTextureData* t : *dd->Textures) {
            if (t->Status == ImTextureStatus_WantDestroy) {
                t->SetTexID(ImTextureID_Invalid), t->BackendUserData = nullptr, t->SetStatus(ImTextureStatus_Destroyed);
                continue;
            }
            if (t->Status != ImTextureStatus_OK) t->SetTexID((ImTextureID)(intptr_t)t), t->SetStatus(ImTextureStatus_OK);
        }
    Image im{w, h, std::vector<float>((size_t)w * h * 3, 0.0f)};
    raster(im, dd);
    FILE* f = fopen(path, "wb");
    std::vector<unsigned char> out((size_t)w * h * 3);
    for (size_t i = 0; i < out.size(); i++) out[i] = (unsigned char)std::clamp((int)(im.px[i] * 255 + 0.5f), 0, 255);
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
}

// ---- the start-up screen at a moment ----
static void startup(u::Ctx& c, StartupState st, bool pt, double t) {
    StartupView v = startup_view(st);
    if (st.left >= 0) v.right = left_text(st.left);
    const BrandStartupText words = brand_startup_text(v, pt);
    u::OcarinaStartup o;
    o.line = words.line, o.eta = words.eta, o.hint = words.hint;
    o.version = std::string(kBrandName) + " v" + OCARINA_VERSION;
    o.staff = v.finished ? !v.title.empty() : v.fraction >= 0;
    o.fraction = v.finished ? 1.0 : v.fraction;
    u::ocarina_startup(c, o, logo, 1.0f, t, true);
}

// ---- a likeness of the menu: the frame and pieces vk_menu.cpp draw_options_ocarina uses, with rows of its Graphics tab ----
struct FakeRow { const char* name; int kind; const char* value; int permille; bool heading; };   // (kind: 0 choice, 1 halves, 2 slider, 3 page)
static void fake_game(u::Ctx& c) {   // (a stand-in for the game behind: a warm sky over a green field)
    c.d->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(c.w, c.h * 0.55f), u::rgba(120, 160, 205), u::rgba(150, 180, 210), u::rgba(232, 200, 150), u::rgba(220, 190, 150));
    c.d->AddRectFilledMultiColor(ImVec2(0, c.h * 0.55f), ImVec2(c.w, c.h), u::rgba(88, 130, 70), u::rgba(96, 140, 76), u::rgba(52, 84, 44), u::rgba(48, 80, 40));
    c.d->AddCircleFilled(ImVec2(c.w * 0.9f, c.h * 0.16f), c.h * 0.06f, u::rgba(250, 236, 200), 64);
}
static void menu(u::Ctx& c, int tab, int sel, bool pending) {
    fake_game(c);
    u::use_ocarina_style();
    u::menu_backdrop(c);
    u::Ctx k = c;
    k.w = std::min(c.w, std::floor(c.h * 16.0f / 9.0f + 0.5f));
    const float ox = std::floor((c.w - k.w) * 0.5f);
    const int vtx0 = c.d->VtxBuffer.Size;
    const float s = k.s;
    const char* const titles[5] = {"Graphics", "Audio", "Gameplay", "Controls", "Debug"};
    const u::MenuFrame f = u::menu_frame(k, std::max(58 * s, k.safe + 40 * s), titles, 5);
    const std::string sub = std::string("v") + OCARINA_VERSION + ", " + kBrandCredit;
    u::menu_header(k, f, logo, 1.0f, kBrandName, sub.c_str());
    float row_x = 0, gap = 34 * s;
    if (!f.side) {
        float total = -gap;
        for (int i = 0; i < 5; i++) total += u::size_of(k.f.heading, 22 * s, titles[i]).x + gap;
        row_x = (f.rx0 + f.rx1 - total) * 0.5f;
    }
    for (int i = 0; i < 5; i++) {
        ImVec2 a, b;
        if (f.side) u::menu_section(k, f, f.sec_x0, f.sec_top + f.sec_pitch * (i + 0.5f), titles[i], i == tab, i == 2, &a, &b);
        else {
            const float tw = u::size_of(k.f.heading, 22 * s, titles[i]).x;
            u::menu_section(k, f, row_x + tw * 0.5f, f.hh + 30 * s, titles[i], i == tab, false, &a, &b);
            row_x += tw + gap;
        }
    }
    static const FakeRow rows[] = {
        {"Display", 0, nullptr, 0, true},
        {"Output resolution", 0, "3840 x 2160", 0, false},
        {"Fullscreen", 1, nullptr, 1, false},
        {"Upscaler", 0, "DLSS", 0, false},
        {"Render resolution", 0, "67% (2560 x 1440)", 0, false},
        {"Sharpness", 2, "0.35", 350, false},
        {"Frame generation", 0, "Off", 0, false},
        {"Performance", 0, nullptr, 0, true},
        {"FPS cap", 0, "Unlimited", 0, false},
        {"Show FPS", 1, nullptr, 0, false},
        {"Anisotropic filtering", 0, "16x", 0, false},
        {"Motion controls", 3, nullptr, 0, false},
    };
    const char* help = "What scales the 3D picture up to the output. DLSS uses NVIDIA's network and looks closest to the full size; "
                       "the game's own scaling costs the least.";
    const char* hint = "DLSS runs here: the render resolution below sets how much it scales.";
    const float hx0 = f.side ? f.sec_x0 : f.rx0, hx1 = f.rx1, hw = hx1 - hx0;
    const float bar_top = u::prompt_bar_top(k), room = bar_top - f.top;
    const char* gold = pending ? "Restart needed: Output resolution" : nullptr;
    const float kept = std::min(u::help_strip_h(k, hw, help, hint, pending, gold, 0), room * 0.4f);
    const float y1 = bar_top - 4 * s, y0 = y1 - kept, bottom = y1 - kept - 14 * s;
    u::menu_section_rule(k, f, bottom);
    const float rh = 40 * s, pitch = 48 * s, split = f.rx0 + (f.rx1 - f.rx0) * 0.55f;
    const float rows_bottom = f.top + std::max(1.0f, std::floor((bottom - f.top + pitch - rh) / pitch)) * pitch;
    k.d->PushClipRect(ImVec2(f.rx0 - 20 * s, f.top - 8 * s), ImVec2(f.rx1 + 8 * s, rows_bottom));
    for (int i = 0; i < (int)(sizeof rows / sizeof *rows); i++) {
        const float y = f.top + i * pitch;
        const FakeRow& r = rows[i];
        if (r.heading) { u::row_heading(k, f.rx0, f.rx1, y, rh, r.name); continue; }
        const bool on = i == sel;
        const ImVec2 a(f.rx0, y), m0(split, y + rh), va(split, y), vb(f.rx1, y + rh);
        u::row_name(k, a, m0, r.name, on, pending && i == 1, 18.5f * s);
        float t0, t1;
        if (r.kind == 1) u::row_halves(k, va, vb, "Off", "On", r.permille == 1, on, false);
        else if (r.kind == 2) u::row_slider(k, va, vb, r.value, r.permille, on, &t0, &t1);
        else if (r.kind == 3) {
            u::row_choice(k, va, vb, "", on, false, false, false);
            u::triangle(k, ImVec2(vb.x - 15 * s, y + rh * 0.5f), 8 * s, 1, on ? u::kFairy : u::kInk);
        } else u::row_choice(k, va, vb, r.value, on, true, true, false);
    }
    k.d->PopClipRect();
    u::help_strip(k, hx0, hx1, y0, y1, help, hint, pending, gold);
    const u::Prompt prompts[] = {{u::kUpDown, "Choose"}, {u::kLeftRight, "Change"}, {u::kTabs, "Tabs"}, {u::kBack, "Close"}};
    u::prompt_bar(k, prompts, 4);
    for (int i = vtx0; i < c.d->VtxBuffer.Size; i++) c.d->VtxBuffer[i].pos.x += ox;
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    const int W = argc > 2 ? atoi(argv[2]) : 1920, H = argc > 3 ? atoi(argv[3]) : 1080;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
    fonts.body = font(hookshot_font_body, hookshot_font_body_size);
    fonts.strong = font(hookshot_font_strong, hookshot_font_strong_size);
    fonts.heading = font(hookshot_font_heading, hookshot_font_heading_size);
    logo = IM_NEW(ImTextureData)();
    logo->Create(ImTextureFormat_RGBA32, (int)ocarina_logo_w, (int)ocarina_logo_h);
    memcpy(logo->GetPixels(), ocarina_logo_rgba, (size_t)ocarina_logo_w * ocarina_logo_h * 4);
    logo->UseColors = true;
    ImGui::RegisterUserTexture(logo);
    u::use_ocarina_style();
    auto at = [&](const char* name) { return out + "/" + name; };
    {   // (warm-up: the glyphs baked once, as the game bakes them ahead)
        StartupState st;
        st.secs = 30, st.prepared = true, st.done = 73638, st.total = 120566, st.left = 236, st.whole = 0.37;
        shoot(at("warmup.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, st, true, 0); startup(c, st, false, 0); menu(c, 0, 3, false); });
    }
    StartupState st;
    st.secs = 30, st.prepared = true, st.done = 73638, st.total = 120566, st.left = 236, st.whole = 0.37;
    shoot(at("startup-pt-37.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, st, true, 1.2); });
    shoot(at("startup-en-37.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, st, false, 1.2); });
    st.stage = 2, st.done = 6120, st.total = 10340, st.left = 40, st.whole = 0.86;
    shoot(at("startup-pt-86.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, st, true, 0.2); });
    StartupState first;
    first.secs = 0.4;
    shoot(at("startup-first-second.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, first, true, 0); });
    StartupState done = st;
    done.finished = true;
    shoot(at("startup-done.raw").c_str(), W, H, [&](u::Ctx& c) { startup(c, done, true, 0); });
    shoot(at("menu-graphics.raw").c_str(), W, H, [&](u::Ctx& c) { menu(c, 0, 3, false); });
    shoot(at("menu-graphics-restart.raw").c_str(), W, H, [&](u::Ctx& c) { menu(c, 0, 5, true); });
    shoot(at("menu-narrow-4x3.raw").c_str(), H * 4 / 3, H, [&](u::Ctx& c) { menu(c, 0, 3, false); });
    std::printf("%d %d\n", W, H);
    return 0;
}
'''


def png(raw_path, w, h, out_path):
    data = open(raw_path, "rb").read()
    rows = b"".join(b"\x00" + data[y * w * 3:(y + 1) * w * 3] for y in range(h))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(out_path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


def default_cxx():
    tools = os.path.join(os.environ.get("LOCALAPPDATA", ""), "HookshotBuilder", "versions")
    if os.path.isdir(tools):
        for v in sorted(os.listdir(tools), reverse=True):
            c = os.path.join(tools, v, "HookshotBuilder", "tools", "llvm-mingw", "bin", "clang++.exe")
            if os.path.exists(c):
                return c
    return shutil.which("clang++") or shutil.which("g++") or "c++"


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("out", nargs="?", default=os.path.join(ROOT, "work", "ocarina_ui_preview"))
    p.add_argument("--cxx", default=None)
    p.add_argument("--size", default="1920x1080")
    a = p.parse_args()
    w, h = (int(x) for x in a.size.lower().split("x"))
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    cxx = a.cxx or default_cxx()
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import ui_fonts
    with tempfile.TemporaryDirectory(prefix="ocarina-ui-") as tmp:
        for name in ("vk_ui.h", "ocarina_ui.h", "vk_progress.h", "ocarina_brand.h", "ocarina_logo.h"):
            shutil.copy(os.path.join(VK, name), tmp)
        with open(os.path.join(tmp, "vk_imgui.h"), "w", encoding="utf-8") as f:
            f.write(STUB_IMGUI_H)
        with open(os.path.join(tmp, "preview.cpp"), "w", encoding="utf-8") as f:
            f.write(PREVIEW_CPP)
        ui_fonts.write(os.path.join(tmp, "ui_fonts.cpp"))
        exe = os.path.join(tmp, "preview.exe")
        srcs = [os.path.join(tmp, "preview.cpp"), os.path.join(tmp, "ui_fonts.cpp")] + \
               [os.path.join(IMGUI, n) for n in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")]
        subprocess.run([cxx, "-std=c++20", "-O2", "-static", "-I", tmp, "-I", IMGUI] + srcs + ["-o", exe], check=True)
        subprocess.run([exe, tmp, str(w), str(h)], check=True)
        for name in sorted(os.listdir(tmp)):
            if not name.endswith(".raw") or name == "warmup.raw":
                continue
            iw = h * 4 // 3 if "4x3" in name else w
            png(os.path.join(tmp, name), iw, h, os.path.join(out, name[:-4] + ".png"))
            print(os.path.join(out, name[:-4] + ".png"))


if __name__ == "__main__":
    main()
