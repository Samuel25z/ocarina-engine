// (ocarina) The Ocarina Engine branding of the Vulkan back end: the window's title (vk_main.cpp start), the Insert menu's banner
// and the start-up screen (vk_menu.cpp, the icon from vk_imgui.cpp brand_logo). It lives here, in hookshot_vk.dll, as players
// replace that DLL alone. RX_VK_BRAND=0 keeps Hookshot's own title, banner and start-up screen.
#pragma once

#include <cstdlib>

// (ocarina) This DLL's release, as its GitHub tag without the "v" (ocarina_update.cpp compares the latest release's tag with it,
// and fetches this tag's hookshot_env.txt to know whether the player changed theirs): raised with every release
#ifndef OCARINA_VERSION
#define OCARINA_VERSION "0.4.1"
#endif

namespace mkwvk {

constexpr const char* kBrandName = "Ocarina Engine";
constexpr const wchar_t* kBrandNameW = L"Ocarina Engine";
constexpr const char* kBrandCredit = "baseado no Hookshot";   // (GPL: the work it is based on, shown in the menu's banner)
// (ocarina) Where the problem reports are sent (ocarina_upload.cpp): the receiver, a Cloudflare Worker
// (C:\Zelda Radar\ocarina-engine\receptor-relatorios). OCARINA_REPORT_URL=<url> sends elsewhere. While this has "<" in it (the
// placeholder before the deploy) nothing is sent
constexpr const char* kReportUrl = "https://ocarina-relatorios.ocarina-relatorios.workers.dev/v1/relatorio";

/// Whether the branding is on: RX_VK_BRAND unset or not 0 (read once)
inline bool brand_on() {
    static const bool on = [] {
        const char* e = getenv("RX_VK_BRAND");
        return !e || atoi(e) != 0;
    }();
    return on;
}

} // namespace mkwvk
