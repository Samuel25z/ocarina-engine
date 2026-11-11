// (ocarina) Ocarina Engine's own update, from the Insert menu (vk_menu.cpp, the Gameplay tab's "Ocarina Engine" rows): the latest
// release of github.com/Samuel25z/ocarina-engine asked for once per start, in the background (ocarina_update.cpp), and on the
// player's "Procurar atualização"; a newer one's hookshot_vk.dll downloaded, checked (a Windows DLL with mkwvk_init and the
// same MKWVK_API_VERSION as the game's, read from its ocarina_engine_info export without running it) and put in place of the
// running one, which is renamed to hookshot_vk.dll.old (removed at the next start). The new one runs from the next start.
// OCARINA_NO_UPDATE_CHECK=1: no update at all; HOOKSHOT_NO_UPDATE_CHECK=1 (the launchers set it): no check at the start, the
// button still asks. Windows only; elsewhere the rows say so
#pragma once

#include <string>

namespace mkwvk::ocarina_update {

enum Phase {
    kOff,          // (OCARINA_NO_UPDATE_CHECK=1, or not Windows)
    kIdle,         // (not asked yet: HOOKSHOT_NO_UPDATE_CHECK=1 left the start's check out)
    kChecking,
    kUpToDate,
    kAvailable,    // (a newer release, with a hookshot_vk.dll)
    kDownloading,
    kInstalled,    // (in place: from the next start)
    kError,
};

struct View {
    Phase phase = kOff;
    std::string status;   // (the "Versão" row's value)
    std::string button;   // (the button row's label)
    std::string detail;   // (the line under the help while one of the rows is selected)
    bool can_press = false;
};

/// Once, as the window's renderer starts: the last update's leftovers removed, then the check (unless switched off)
void begin();
/// What the menu shows now (any thread; copies)
View view();
/// The button (Enter, A, a click): Atualizar when a newer release is known, else Procurar atualização; nothing while busy
void press();
/// (the help box's height, vk_menu.cpp measure_help) the longest lines detail can be, k from 0 until nullptr
const char* sample(int k);

} // namespace mkwvk::ocarina_update
