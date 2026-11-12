// (ocarina) Ocarina Engine's problem report (ocarina_report.cpp): a .zip in the game folder with what is needed to understand a
// tester's problem, made on a thread of its own, then sent to the Ocarina Engine's receiver unless turned off (ocarina_upload.h). From the Insert menu (vk_menu.cpp, the Gameplay tab's
// "Ocarina Engine" rows: Gerar relatório, Abrir pasta), and by itself at the start after a session that did not end cleanly (its
// log, hookshot_log.1.txt, has no "window closed" line, or has CRASH / FATAL / device lost lines): a note on screen for ~10 s, at
// most 5 automatic reports kept. OCARINA_AUTO_REPORT=0 or RX_VK_AUTO_REPORT=0 (hookshot_env.txt takes RX_ names): none by itself.
// In it: the logs, the last ~3 minutes of frame times (a ring in memory, filled by vk_replay.cpp status_tick at every game
// frame), the frames around each bad stall of the session (also kept in ocarina-travadas.csv, for the next start's report),
// hookshot_options.ini, hookshot_env.txt and sistema.txt (the GPU, CPU, memory, Windows, the window, RX_/HOS_ switches, the log's
// last VRAM, texture detail, replay and GPU time lines), a PNG of the last frame shown (the manual one: there is no frame yet at the
// start), and the bug marks. The user's name, profile folder, computer name and e-mails are taken out.
// The bug key (F9; RX_VK_BUG_KEY=<F1..F24, a letter or digit, a virtual-key number>, 0 or off: none), read by vk_menu.cpp menu_tick
// whether the menu is open or not: a PNG of the frame, the hour, the frame's number and the log's last lines in ocarina-marcas beside
// the game (the newest 30 kept), "Bug marcado (N)" on screen for a moment
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mkwvk::ocarina_report {

constexpr int kPhases = 13;   // (vk_internal.h PROF_COUNT: the replay's phases, as the log's "a frame's replay time" line)

/// One game frame, present to present (vk_replay.cpp status_tick)
struct Frame {
    int64_t at_ns = 0;           // (steady clock)
    float ms = 0;                // (since the previous present)
    float gpu_ms = 0;            // (GPU time of the recordings read back since the previous frame)
    float behind_ms = 0;         // (the replay's wait for a recording slot: the GPU behind)
    float phase_ms[kPhases]{};   // (the replay's phases, sampled as the log's line)
    uint8_t flags = 0;           // (set here: kStallBig, kStallMany, kSlow)
};
constexpr uint8_t kSlow = 1, kStallBig = 2, kStallMany = 4, kMark = 8;

/// Each game frame, on the replay thread: cheap (a copy into the ring under an uncontended lock)
void frame(Frame f);

/// Once, as the window's renderer starts (vk_main.cpp start): the last session's stalls file moved aside, and the automatic report
/// of the last session when it did not end cleanly (in the background)
void begin();

struct View {
    std::string button;        // ("Gerar relatório", "Gerando...")
    std::string folder;        // ("Abrir pasta", or "Nenhum ainda")
    std::string detail;        // (the line under the help: "Relatório salvo: ...", the automatic one, an error)
    bool can_press = false, can_open = false;
};
View view();
/// Gerar relatório (Enter, A, a click): nothing while one is being made
void press();
/// Abrir pasta: Explorer on the game folder, the last report selected
void open_folder();
/// (measure_help) the longest lines detail can be, k from 0 until nullptr
const char* sample(int k);
/// The note on screen: after an automatic report (~10 s once the start-up screen is over), after a bug mark (~2.5 s); *text its line,
/// *sub a second line or ""
bool toast(std::string* text, std::string* sub);

/// The bug key's virtual-key code (0: none)
int bug_key();
/// The bug key pressed: a mark made on a thread of its own (nothing while the last one is being saved)
void mark_bug();

/// The scene flicker detector (on unless RX_VK_FLICKER=0): vk_replay.cpp measures the mean brightness of the middle of each
/// presented picture (a tiny blit read back a few presents later, never waited for) and hands it here; a frame, or two in a row,
/// brighter than the two frames on each side by more than RX_VK_FLICKER_THRESH percent (2.5), the scene the same before and after,
/// is a flicker. Every 600 presents the log has "vk: scene flicker: N in 600 presents ..." with the CPU time measuring took; the
/// report has the count, the events and the brightness of the last ~3 minutes (brilho.csv); a bug mark has the series around it
bool flicker_on();
/// (vk_replay.cpp, the replay thread) the scene's mean brightness, 0..255, of present number `present`, in present order
void scene_brightness(int64_t present, float luma);
/// (vk_replay.cpp) the CPU time measuring took at a present
void flicker_cost(int64_t ns);

/// (vk_replay.cpp, at each present with a picture) whether a picture of the game's frame is wanted: one taken serves every ask
bool shot_wanted();
/// (vk_replay.cpp, from its reading thread) the picture asked for: RGBA8, w x h; empty when it could not be taken
void shot_taken(std::vector<uint8_t> rgba, uint32_t w, uint32_t h);

} // namespace mkwvk::ocarina_report
