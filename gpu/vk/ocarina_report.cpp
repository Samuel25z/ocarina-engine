// (ocarina) Ocarina Engine's problem report: ocarina_report.h. A ZIP written by hand ("stored": no compression, so no library),
// on a thread of its own at below-normal priority, then sent to the receiver unless that is turned off (ocarina_upload.h). What goes in is read with every sharing flag (the log is open
// for writing) and cleaned of the user's name, profile folder, computer name and e-mails (Scrub) before it is written
#include "ocarina_report.h"
#include "ocarina_brand.h"
#include "ocarina_upload.h"
#include "vk_internal.h"
#include "vk_menu.h"   // (opts.auto_report, the notice's flag)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include "vk_paths.h"
#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif
#endif

#define OCARINA_REPORT_STR2(x) #x
#define OCARINA_REPORT_STR(x) OCARINA_REPORT_STR2(x)

namespace mkwvk {
#ifdef _WIN32
HWND app_window();   // (vk_main.cpp)
#endif
bool write_png(const char* path, const uint8_t* rgba, uint32_t w, uint32_t h);   // (vk_replay.cpp: stored deflate, no library)
}

namespace mkwvk::ocarina_report {
namespace {

std::string fmt(const char* f, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, f);
    vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}

int64_t steady_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ---- the frame ring and the stalls (vk_replay.cpp status_tick, each game frame) ----
constexpr size_t kRing = 1u << 15;            // (~9 min at 60 fps, ~2.3 min at 240: the report takes the last 3 minutes of it)
constexpr int64_t kKeepNs = 180'000'000'000;  // (3 minutes)
constexpr uint64_t kWarmup = 300;             // (the first frames, the start-up screen's: no stalls counted)
constexpr int kAround = 300;                  // (a stall's snapshot: these frames before it and after it)
constexpr size_t kMaxSnaps = 6, kMaxStalls = 200;

struct Stall {
    uint64_t seq;
    int64_t at_ns;
    float ms;
    uint8_t kind;
};
struct Snapshot {
    Stall why;
    std::vector<Frame> frames;
    uint64_t first_seq;
};

std::mutex ring_mu;
Frame ring[kRing];
uint64_t ring_n = 0;
int64_t clock0_ns = 0;   // (the first frame's steady time, and the wall clock then: a frame's hour from its steady time)
#ifdef _WIN32
FILETIME clock0_ft{};
#endif
int64_t slow_at[8];   // (the last frames over 250 ms: 5 of them within 10 s is a stall)
int slow_n = 0;
std::vector<Stall> stalls;
std::vector<Snapshot> snaps;
bool snap_pending = false;
uint64_t snap_seq = 0;
Stall snap_why{};

std::string kind_text(uint8_t k) {
    std::string t = k & kStallBig ? "quadro acima de 1 s" : k & kStallMany ? "5 quadros acima de 250 ms em 10 s" : k & kSlow ? "quadro acima de 250 ms" : "";
    if (k & kMark) t += t.empty() ? "bug marcado" : "; bug marcado";
    return t;
}

/// "HH:MM:SS.mmm" of a frame's steady time (local hour)
std::string hour_of(int64_t at_ns) {
#ifdef _WIN32
    if (!clock0_ns) return "";
    ULARGE_INTEGER u;
    u.LowPart = clock0_ft.dwLowDateTime, u.HighPart = clock0_ft.dwHighDateTime;
    u.QuadPart += (at_ns - clock0_ns) / 100;
    FILETIME ft;
    ft.dwLowDateTime = u.LowPart, ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME utc, loc;
    if (!FileTimeToSystemTime(&ft, &utc) || !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &loc)) return "";
    return fmt("%02d:%02d:%02d.%03d", loc.wHour, loc.wMinute, loc.wSecond, loc.wMilliseconds);
#else
    (void)at_ns;
    return "";
#endif
}

const char* const kPhaseNames[kPhases] = {"submit", "present", "draw", "descriptors", "texture_lookups", "uploads", "pipelines",
                                          "programs", "commands", "dispatch", "memory_sync", "queue_submit", "slot_wait"};
std::string frame_header(const char* first) {
    std::string h = first;
    h += "quadro,hora,ms,gpu_ms,gpu_atrasada_ms";
    for (const char* n : kPhaseNames) h += ",replay_", h += n, h += "_ms";
    h += ",marca\n";
    return h;
}
void frame_row(std::string& out, const char* first, uint64_t seq, const Frame& f) {
    out += first;
    out += fmt("%llu,%s,%.2f,%.2f,%.2f", (unsigned long long)seq, hour_of(f.at_ns).c_str(), f.ms, f.gpu_ms, f.behind_ms);
    for (float p : f.phase_ms) out += fmt(",%.2f", p);
    out += ",", out += kind_text(f.flags), out += "\n";
}

// ---- the scene flicker detector (vk_replay.cpp measures each present's brightness; scene_brightness below finds the flickers) ----
struct Lum {
    int64_t present = 0, at_ns = 0;
    float luma = 0;   // (0..255: the mean of the middle of the picture, read back 2-3 presents after)
    float pct = 0;    // (a flicker's rise over its neighbours, % of their brightness; 0 none)
};
struct FlickerEvent {
    int64_t present, at_ns;
    float pct;
    int frames;   // (1 or 2: how long it stayed brighter)
};
std::mutex lum_mu;
Lum lums[kRing];
uint64_t lum_n = 0;
std::vector<FlickerEvent> flickers;   // (the session's, the newest 500)
uint64_t flickers_total = 0;
// (the log's line every 600 presents)
int64_t win_present0 = -1, win_ns0 = 0;
int win_count = 0;
float win_largest = 0;
std::string win_frames;
std::atomic<int64_t> win_cost_ns{0};
std::atomic<int64_t> win_cost_n{0};
int64_t last_hit = -10;

float flicker_thresh() {   // (RX_VK_FLICKER_THRESH: percent of the neighbours' brightness, 2.5 when not said)
    static const float t = [] {
        const char* e = getenv("RX_VK_FLICKER_THRESH");
        const float v = e ? (float)atof(e) : 0.0f;
        return v > 0.05f && v < 100.0f ? v : 2.5f;
    }();
    return t;
}

/// The brightness of the last `ns` (all of them: 0), oldest first (lum_mu not held)
std::vector<Lum> lums_since(int64_t ns, size_t most = kRing) {
    std::lock_guard lk(lum_mu);
    std::vector<Lum> out;
    if (!lum_n) return out;
    const uint64_t have = std::min<uint64_t>({lum_n, (uint64_t)kRing, (uint64_t)most});
    const int64_t newest = lums[(lum_n - 1) % kRing].at_ns;
    uint64_t k = 0;
    while (k < have && (!ns || newest - lums[(lum_n - 1 - k) % kRing].at_ns <= ns)) k++;
    out.reserve(k);
    for (uint64_t s = lum_n - k; s < lum_n; s++) out.push_back(lums[s % kRing]);
    return out;
}
std::string lums_csv(const std::vector<Lum>& v) {
    std::string t = fmt("# Brilho médio do meio da cena (0 a 255) em cada quadro apresentado; piscada_pct: quanto o quadro ficou mais claro que os "
                        "vizinhos (limiar %.1f%%, RX_VK_FLICKER_THRESH)\npresent,hora,brilho,piscada_pct\n", flicker_thresh());
    t.reserve(t.size() + v.size() * 36);
    for (const Lum& l : v)
        t += fmt("%lld,%s,%.2f,%s\n", (long long)l.present, hour_of(l.at_ns).c_str(), l.luma, l.pct > 0 ? fmt("%.1f", l.pct).c_str() : "");
    return t;
}
std::string flicker_text() {
    std::vector<FlickerEvent> ev;
    uint64_t total = 0;
    {
        std::lock_guard lk(lum_mu);
        ev = flickers, total = flickers_total;
    }
    const int64_t now = steady_ns();
    int recent = 0;
    for (const FlickerEvent& e : ev) recent += now - e.at_ns <= kKeepNs;
    std::string t = fmt("  %llu nesta sessão, %d nos últimos 3 minutos (limiar +%.1f%% sobre os 2 quadros de cada lado)\n",
                        (unsigned long long)total, recent, flicker_thresh());
    for (size_t i = ev.size() > 30 ? ev.size() - 30 : 0; i < ev.size(); i++)
        t += fmt("  %s  present %lld  +%.1f%% (%d quadro%s)\n", hour_of(ev[i].at_ns).c_str(), (long long)ev[i].present, ev[i].pct,
                 ev[i].frames, ev[i].frames > 1 ? "s" : "");
    return t;
}

#ifdef _WIN32
// ---- files ----
std::wstring dir_of(const std::wstring& p) {
    const size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"" : p.substr(0, k);
}
/// The game folder (vk_paths.h's rule, as ocarina_update.cpp game_dir): the logs, the options and the reports are there
const std::wstring& game_dir() {
    static const std::wstring d = [] {
        std::wstring exe(32768, L'\0');
        const DWORD n = GetModuleFileNameW(nullptr, exe.data(), (DWORD)exe.size());
        if (!n || n >= exe.size()) return std::wstring();
        exe.resize(n);
        const std::wstring dir = dir_of(exe), up = dir_of(dir);
        const std::wstring last = dir.substr(up.size() + (up.empty() ? 0 : 1));
        if (!up.empty() && !_wcsicmp(last.c_str(), L"bin") &&
            GetFileAttributesW((up + L"\\image.bin").c_str()) != INVALID_FILE_ATTRIBUTES)
            return up;
        return dir;
    }();
    return d;
}
std::wstring in_dir(const wchar_t* name) { return game_dir() + L"\\" + name; }
bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string env_utf8(const wchar_t* name) {
    wchar_t b[2048];
    const DWORD n = GetEnvironmentVariableW(name, b, 2048);
    return n && n < 2048 ? utf8(std::wstring(b, n)) : "";
}
/// The file's bytes (shared with its writer: the log is open); a long one keeps its first `head` and last `tail` bytes
bool read_file(const std::wstring& p, std::string* out, uint64_t head = 4u << 20, uint64_t tail = 20u << 20) {
    const HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    const uint64_t total = (uint64_t)size.QuadPart;
    const auto read_at = [&](uint64_t at, uint64_t n, std::string& to) {
        LARGE_INTEGER pos;
        pos.QuadPart = (LONGLONG)at;
        SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
        const size_t base = to.size();
        to.resize(base + n);
        DWORD got = 0;
        uint64_t have = 0;
        while (have < n && ReadFile(h, to.data() + base + have, (DWORD)std::min<uint64_t>(n - have, 1u << 24), &got, nullptr) && got)
            have += got;
        to.resize(base + have);
    };
    out->clear();
    if (total <= head + tail) {
        read_at(0, total, *out);
    } else {
        read_at(0, head, *out);
        *out += fmt("\n\n[... %llu MB do meio cortados pelo relatório ...]\n\n", (unsigned long long)((total - head - tail) >> 20));
        read_at(total - tail, tail, *out);
    }
    CloseHandle(h);
    return true;
}
bool write_file(const std::wstring& p, const std::string& body) {
    const HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = body.empty() || (WriteFile(h, body.data(), (DWORD)body.size(), &put, nullptr) && put == body.size());
    CloseHandle(h);
    return ok;
}

// ---- what is personal, taken out ----
char low(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
bool sep(char c) { return c == '\\' || c == '/'; }
bool word_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (unsigned char)c >= 0x80; }
bool eq_at(const std::string& s, size_t i, const std::string& p) {
    if (p.empty() || i + p.size() > s.size()) return false;
    for (size_t k = 0; k < p.size(); k++) {
        const char a = s[i + k], b = p[k];
        if (sep(a) && sep(b)) continue;
        if (low(a) != low(b)) return false;
    }
    return true;
}
struct Scrub {
    std::string profile, user, computer;
    Scrub() : profile(env_utf8(L"USERPROFILE")), user(env_utf8(L"USERNAME")), computer(env_utf8(L"COMPUTERNAME")) {
        while (!profile.empty() && sep(profile.back())) profile.pop_back();
    }
    /// The profile folder as %USERPROFILE% (either slash, any case); then any other X:\Users\<name> the same way
    static std::string paths(const std::string& s, const std::string& profile) {
        std::string o;
        o.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            if (profile.size() > 3 && eq_at(s, i, profile) && (i + profile.size() == s.size() || !word_char(s[i + profile.size()]))) {
                o += "%USERPROFILE%", i += profile.size();
                continue;
            }
            // (<sep>Users<sep><name>: the drive before it too)
            if (sep(s[i]) && eq_at(s, i + 1, "users") && i + 6 < s.size() && sep(s[i + 6])) {
                size_t j = i + 6;
                while (j < s.size() && sep(s[j])) j++;
                size_t e = j;
                while (e < s.size() && !sep(s[e]) && !strchr("\"'<>|,;\r\n\t", s[e])) e++;
                if (e < s.size() && !sep(s[e])) {   // (the name ends the path: a space ends it too)
                    const size_t sp = s.find(' ', j);
                    if (sp != std::string::npos && sp < e) e = sp;
                }
                const std::string name = s.substr(j, e - j);
                std::string ln;
                for (char ch : name) ln += low(ch);
                if (!name.empty() && ln != "public" && ln != "default" && ln != "all users" && ln != "default user" && name[0] != '%') {
                    size_t start = o.size(), seps = start;
                    while (seps > 0 && sep(o[seps - 1])) seps--;   // (an escaped path's doubled separators: C:\\Users)
                    if (seps >= 2 && o[seps - 1] == ':' && ((low(o[seps - 2]) >= 'a' && low(o[seps - 2]) <= 'z'))) start = seps - 2;
                    o.resize(start);
                    o += "%USERPROFILE%";
                    i = e;
                    continue;
                }
            }
            o += s[i++];
        }
        return o;
    }
    static std::string word(const std::string& s, const std::string& w, const char* as) {
        if (w.size() < 3) return s;
        std::string o;
        o.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            if (eq_at(s, i, w) && (i == 0 || !word_char(s[i - 1])) && (i + w.size() == s.size() || !word_char(s[i + w.size()]))) {
                o += as, i += w.size();
                continue;
            }
            o += s[i++];
        }
        return o;
    }
    static bool mail_char(char c) { return word_char(c) || c == '.' || c == '-' || c == '+' || c == '%'; }
    static std::string mails(const std::string& s) {
        std::string o;
        o.reserve(s.size());
        size_t from = 0;
        for (size_t at = s.find('@'); at != std::string::npos; at = s.find('@', at + 1)) {
            size_t l = at;
            while (l > from && mail_char(s[l - 1])) l--;
            size_t r = at + 1;
            while (r < s.size() && (word_char(s[r]) || s[r] == '.' || s[r] == '-')) r++;
            while (r > at + 1 && s[r - 1] == '.') r--;
            const size_t dot = s.rfind('.', r - 1);
            bool tld = dot != std::string::npos && dot > at + 1 && r - dot >= 3;
            for (size_t k = dot + 1; tld && k < r; k++) tld = (low(s[k]) >= 'a' && low(s[k]) <= 'z');
            if (l == at || !tld) continue;
            o.append(s, from, l - from), o += "<email>";
            from = r;
            at = r - 1;
        }
        o.append(s, from, std::string::npos);
        return o;
    }
    /// IPv4 addresses (four numbers to 255 with dots, nothing number-like around them) as <ip>; this PC's own (127.0.0.1) and
    /// 0.0.0.0 stay. A version number with four parts that all fit (1.2.3.4) goes too: the report loses nothing that matters
    static std::string ips(const std::string& s) {
        const auto digit = [](char c) { return c >= '0' && c <= '9'; };
        std::string o;
        o.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            if (digit(s[i]) && (i == 0 || (!word_char(s[i - 1]) && s[i - 1] != '.'))) {
                size_t j = i;
                int parts = 0;
                bool ok = true;
                for (; parts < 4; parts++) {
                    size_t k = j;
                    int v = 0;
                    while (k < s.size() && digit(s[k]) && k - j < 4) v = v * 10 + (s[k] - '0'), k++;
                    if (k == j || k - j > 3 || v > 255) {
                        ok = false;
                        break;
                    }
                    j = k;
                    if (parts < 3) {
                        if (j >= s.size() || s[j] != '.') {
                            ok = false;
                            break;
                        }
                        j++;
                    }
                }
                ok = ok && parts == 4 && !(j < s.size() && (word_char(s[j]) || (s[j] == '.' && j + 1 < s.size() && digit(s[j + 1]))));
                if (ok) {
                    const std::string ip = s.substr(i, j - i);
                    o += ip == "127.0.0.1" || ip == "0.0.0.0" ? ip : "<ip>";
                    i = j;
                    continue;
                }
                while (i < s.size() && (digit(s[i]) || s[i] == '.')) o += s[i++];   // (the rest of that number as it is)
                continue;
            }
            o += s[i++];
        }
        return o;
    }
    std::string operator()(const std::string& s) const {
        std::string o = paths(s, profile);
        o = word(o, user, "<usuario>");
        o = word(o, computer, "<computador>");
        return ips(mails(o));
    }
};

// ---- the ZIP ("stored": PKWARE APPNOTE 4.3; names in UTF-8, bit 11) ----
uint32_t crc32_of(const std::string& d) {
    static uint32_t table[256];
    static std::once_flag once;
    std::call_once(once, [] {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    });
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : d) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
struct Zip {
    FILE* f = nullptr;
    struct Entry {
        std::string name;
        uint32_t crc, size, offset;
    };
    std::vector<Entry> entries;
    uint16_t dtime = 0, ddate = 0;
    uint64_t at = 0;
    bool ok = true;
    void u16(std::string& b, uint16_t v) { b += (char)(v & 0xFF), b += (char)(v >> 8); }
    void u32(std::string& b, uint32_t v) { u16(b, (uint16_t)(v & 0xFFFF)), u16(b, (uint16_t)(v >> 16)); }
    void put(const std::string& b) {
        if (f && fwrite(b.data(), 1, b.size(), f) != b.size()) ok = false;
        at += b.size();
    }
    bool open(const std::wstring& path) {
        f = _wfopen(path.c_str(), L"wb");
        SYSTEMTIME t;
        GetLocalTime(&t);
        dtime = (uint16_t)(t.wHour << 11 | t.wMinute << 5 | t.wSecond / 2);
        ddate = (uint16_t)((t.wYear - 1980) << 9 | t.wMonth << 5 | t.wDay);
        return f != nullptr;
    }
    void add(const std::string& name, const std::string& data) {
        if (!f || at + data.size() + 4096 > 0xFFFF0000u) {   // (no ZIP64: a report stays under 4 GB)
            ok = ok && f;
            return;
        }
        Entry e{name, crc32_of(data), (uint32_t)data.size(), (uint32_t)at};
        std::string h;
        u32(h, 0x04034b50), u16(h, 20), u16(h, 0x0800), u16(h, 0), u16(h, dtime), u16(h, ddate);
        u32(h, e.crc), u32(h, e.size), u32(h, e.size), u16(h, (uint16_t)name.size()), u16(h, 0);
        h += name;
        put(h);
        put(data);
        entries.push_back(e);
    }
    bool close() {
        if (!f) return false;
        const uint32_t cd = (uint32_t)at;
        std::string c;
        for (const Entry& e : entries) {
            u32(c, 0x02014b50), u16(c, 20), u16(c, 20), u16(c, 0x0800), u16(c, 0), u16(c, dtime), u16(c, ddate);
            u32(c, e.crc), u32(c, e.size), u32(c, e.size), u16(c, (uint16_t)e.name.size()), u16(c, 0), u16(c, 0), u16(c, 0), u16(c, 0);
            u32(c, 0), u32(c, e.offset);
            c += e.name;
        }
        put(c);
        std::string end;
        u32(end, 0x06054b50), u16(end, 0), u16(end, 0), u16(end, (uint16_t)entries.size()), u16(end, (uint16_t)entries.size());
        u32(end, (uint32_t)c.size()), u32(end, cd), u16(end, 0);
        put(end);
        const bool closed = fclose(f) == 0;
        f = nullptr;
        return ok && closed;
    }
};

// ---- sistema.txt ----
std::string cpu_text() {
    std::string name;
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    unsigned r[12] = {};
    unsigned a, b, c, d;
    if (__get_cpuid(0x80000000u, &a, &b, &c, &d) && a >= 0x80000004u) {
        for (unsigned k = 0; k < 3; k++) __get_cpuid(0x80000002u + k, &r[k * 4], &r[k * 4 + 1], &r[k * 4 + 2], &r[k * 4 + 3]);
        name.assign((const char*)r, strnlen((const char*)r, sizeof r));
        while (!name.empty() && name.front() == ' ') name.erase(name.begin());
        while (!name.empty() && name.back() == ' ') name.pop_back();
    }
#endif
    if (name.empty()) name = "(não identificado)";
    DWORD len = 0;
    int cores = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    std::vector<char> buf(len);
    if (len && GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf.data(), &len))
        for (DWORD at = 0; at < len; cores++) at += ((PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf.data() + at))->Size;
    const DWORD threads = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return fmt("%s; %d núcleos, %lu threads", name.c_str(), cores, (unsigned long)threads);
}
std::string reg_text(const wchar_t* key, const wchar_t* value, DWORD* number = nullptr) {
    typedef LONG(WINAPI * GetValue)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
    static const GetValue get = (GetValue)(void*)GetProcAddress(LoadLibraryW(L"advapi32.dll"), "RegGetValueW");
    if (!get) return "";
    if (number) {
        DWORD size = sizeof *number;
        return get(HKEY_LOCAL_MACHINE, key, value, 0x10 /* RRF_RT_REG_DWORD */, nullptr, number, &size) == ERROR_SUCCESS ? "1" : "";
    }
    wchar_t b[256];
    DWORD size = sizeof b;
    if (get(HKEY_LOCAL_MACHINE, key, value, 0x2 /* RRF_RT_REG_SZ */, nullptr, b, &size) != ERROR_SUCCESS) return "";
    return utf8(b);
}
std::string windows_text() {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW v{sizeof v};
    if (const auto rgv = (RtlGetVersionFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")) rgv(&v);
    const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    DWORD ubr = 0;
    const bool has_ubr = !reg_text(key, L"UBR", &ubr).empty();
    const std::string display = reg_text(key, L"DisplayVersion"), edition = reg_text(key, L"EditionID");
    const char* name = v.dwMajorVersion == 10 ? (v.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10") : "Windows";
    std::string t = fmt("%s %s%s(versão %lu.%lu, build %lu", name, edition.c_str(), edition.empty() ? "" : " ", (unsigned long)v.dwMajorVersion,
                        (unsigned long)v.dwMinorVersion, (unsigned long)v.dwBuildNumber);
    if (has_ubr) t += fmt(".%lu", (unsigned long)ubr);
    t += ")";
    if (!display.empty()) t += " " + display;
    return t;
}
std::string driver_version(uint32_t vendor, uint32_t v) {
    if (vendor == 0x10DE) return fmt("%u.%u.%u.%u", v >> 22 & 0x3FF, v >> 14 & 0xFF, v >> 6 & 0xFF, v & 0x3F);
    if (vendor == 0x8086) return fmt("%u.%u", v >> 14, v & 0x3FFF);
    return fmt("%u.%u.%u", v >> 22, v >> 12 & 0x3FF, v & 0xFFF);
}
std::string gpus_text(bool now) {
    std::string t;
    if (!dev.instance) return "  (o Vulkan ainda não tinha começado)\n";
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(dev.instance, &n, nullptr);
    std::vector<VkPhysicalDevice> all(n);
    if (n) vkEnumeratePhysicalDevices(dev.instance, &n, all.data());
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(all[i], &p);
        VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        if (p.apiVersion >= VK_API_VERSION_1_2) {
            VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            p2.pNext = &drv;
            vkGetPhysicalDeviceProperties2(all[i], &p2);
        }
        VkPhysicalDeviceMemoryProperties m{};
        vkGetPhysicalDeviceMemoryProperties(all[i], &m);
        uint64_t vram = 0, shared = 0;
        for (uint32_t h = 0; h < m.memoryHeapCount; h++)
            (m.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? vram : shared) += m.memoryHeaps[h].size;
        const char* type = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? "dedicada"
                           : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrada"
                           : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? "virtual"
                           : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU            ? "CPU"
                                                                                    : "outra";
        t += fmt("- %u: %s%s (%s; fabricante %#06x, modelo %#06x)\n", i, p.deviceName, all[i] == dev.physical ? "  << EM USO" : "", type,
                 p.vendorID, p.deviceID);
        t += fmt("  Vulkan %u.%u.%u; driver %s", VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
                 VK_API_VERSION_PATCH(p.apiVersion), driver_version(p.vendorID, p.driverVersion).c_str());
        if (drv.driverName[0]) t += fmt(" (%s %s)", drv.driverName, drv.driverInfo);
        t += fmt("\n  VRAM %llu MB; memória do sistema que ela usa %llu MB; textura 2D até %u\n", (unsigned long long)(vram >> 20),
                 (unsigned long long)(shared >> 20), p.limits.maxImageDimension2D);
    }
    if (dev.physical) {
        t += fmt("Recursos usados na placa em uso: mesh shaders %s, biblioteca de pipelines %s, orçamento de VRAM do driver %s, "
                 "relatório de falha (device fault) %s, matriz cooperativa %s, HDR %s, subgrupos %u (até %u)\n",
                 dev.mesh_shader ? "sim" : dev.mesh_fallback ? "não (compute no lugar)" : "não",
                 dev.graphics_pipeline_library ? "sim" : "não", dev.memory_budget ? "sim" : "não", dev.device_fault ? "sim" : "não",
                 dev.coop_matrix ? "sim" : "não", dev.hdr ? "sim" : "não", dev.subgroup_size, dev.subgroup_max);
        uint64_t used = 0, allowed = 0;
        if (now && vram_usage(&used, &allowed))
            t += fmt("VRAM agora: %llu MB em uso de %llu MB que o driver permite ao jogo\n", (unsigned long long)(used >> 20),
                     (unsigned long long)(allowed >> 20));
        else if (now)
            t += "VRAM agora: o driver não informa quanto permite (sem VK_EXT_memory_budget)\n";
    }
    return t;
}
std::string window_text() {
    std::string t;
    if (const HWND w = app_window()) {
        RECT r{};
        GetClientRect(w, &r);
        t += fmt("Janela do jogo: %ldx%ld%s", r.right - r.left, r.bottom - r.top, IsZoomed(w) ? " (maximizada)" : "");
        MONITORINFO mi{sizeof mi};
        if (GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi)) {
            const RECT& m = mi.rcMonitor;
            t += fmt("; monitor %ldx%ld", m.right - m.left, m.bottom - m.top);
            const bool full = r.right - r.left == m.right - m.left && r.bottom - r.top == m.bottom - m.top;
            if (full) t += " (tela cheia)";
        }
        t += "\n";
    }
    t += fmt("Imagem enviada à tela (swapchain): %ux%u%s\n", dev.swap_extent.width, dev.swap_extent.height, dev.hdr ? ", HDR10" : "");
    return t;
}
std::string env_text(const Scrub& scrub) {
    std::vector<std::string> lines;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* e = block; *e; e += wcslen(e) + 1) {
            const std::string s = utf8(e);
            static const char* const prefixes[] = {"RX_", "HOS_", "OCARINA_", "HOOKSHOT_", "EDEN_", "NVN_"};
            for (const char* p : prefixes)
                if (!_strnicmp(s.c_str(), p, strlen(p))) {
                    lines.push_back(scrub(s));
                    break;
                }
        }
        FreeEnvironmentStringsW(block);
    }
    std::sort(lines.begin(), lines.end());
    std::string t;
    for (const std::string& l : lines) t += "  " + l + "\n";
    return t.empty() ? "  (nenhuma)\n" : t;
}

/// What the session's log says: its first line, the last of each line the report wants, the crash lines, its end
struct LogFacts {
    std::string first, vram, detail, replay, gpu, status, governor, flicker;
    std::vector<std::string> errors, tail;
    bool clean_end = false;   // (the window closed, or the game ended itself)
};
LogFacts log_facts(const std::string& log) {
    LogFacts f;
    size_t at = 0;
    while (at < log.size()) {
        size_t e = log.find('\n', at);
        if (e == std::string::npos) e = log.size();
        std::string line = log.substr(at, e - at);
        at = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (f.first.empty()) f.first = line;
        const auto has = [&](const char* k) { return line.find(k) != std::string::npos; };
        if (has("vk: VRAM governor")) f.governor = line;
        else if (has("vk: VRAM")) f.vram = line;
        if (has("vk: texture detail")) f.detail = line;
        if (has("replay time")) f.replay = line;
        if (has("vk: GPU time")) f.gpu = line;
        if (has("vk: status:")) f.status = line;
        if (has("vk: scene flicker")) f.flicker = line;
        if (has("CRASH") || has("FATAL") || has("DEVICE_LOST") || has("device was lost") || has("device fault") || has("hang dump")) {
            f.errors.push_back(line);
            if (f.errors.size() > 40) f.errors.erase(f.errors.begin());
        }
        if (has("display: window closed") || has("svcExitProcess") || has("the game closes")) f.clean_end = true;
        f.tail.push_back(line);
        if (f.tail.size() > 25) f.tail.erase(f.tail.begin());
    }
    return f;
}

std::string stalls_text() {
    std::vector<Stall> list;
    {
        std::lock_guard lk(ring_mu);
        list = stalls;
    }
    std::string t;
    for (const Stall& s : list) t += fmt("  %s  %.0f ms  (%s, quadro %llu)\n", hour_of(s.at_ns).c_str(), s.ms, kind_text(s.kind).c_str(),
                                         (unsigned long long)s.seq);
    return t.empty() ? "  (nenhuma)\n" : t;
}
/// The session's stalls as CSV: their list (as # lines), then the frames around each (copied under the lock, written after it:
/// the replay thread waits on that lock at each frame)
std::string stalls_csv() {
    std::vector<Stall> stalls;
    std::vector<Snapshot> snaps;
    {
        std::lock_guard lk(ring_mu);
        stalls = ocarina_report::stalls, snaps = ocarina_report::snaps;
    }
    if (snaps.empty() && stalls.empty()) return "";
    std::string t = "# Travadas desta sessão do Ocarina Engine " OCARINA_VERSION
                    ": cada uma com os ~300 quadros antes e os ~300 depois (coluna travada)\n";
    for (const Stall& s : stalls)
        t += fmt("# %s %.0f ms: %s (quadro %llu)\n", hour_of(s.at_ns).c_str(), s.ms, kind_text(s.kind).c_str(), (unsigned long long)s.seq);
    t += frame_header("travada,");
    for (size_t k = 0; k < snaps.size(); k++) {
        const std::string first = fmt("%zu,", k + 1);
        for (size_t i = 0; i < snaps[k].frames.size(); i++) frame_row(t, first.c_str(), snaps[k].first_seq + i, snaps[k].frames[i]);
    }
    return t;
}
std::atomic<bool> stalls_saving{false}, stalls_dirty{false};
/// ocarina-travadas.csv beside the game, for the next start's report when this session ends badly (a thread: not on the replay's)
void save_stalls_async() {
    stalls_dirty.store(true);
    if (stalls_saving.exchange(true)) return;
    std::thread([] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        for (;;) {
            while (stalls_dirty.exchange(false)) {
                const std::string csv = stalls_csv();
                const std::wstring p = in_dir(L"ocarina-travadas.csv");
                if (!csv.empty() && write_file(p + L".tmp", csv)) MoveFileExW((p + L".tmp").c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
            }
            stalls_saving.store(false);
            if (!stalls_dirty.load() || stalls_saving.exchange(true)) break;
        }
    }).detach();
}

std::string frames_csv() {
    std::vector<Frame> fr;
    uint64_t first = 0;
    {
        std::lock_guard lk(ring_mu);
        if (!ring_n) return "";
        const uint64_t have = std::min<uint64_t>(ring_n, kRing);
        const int64_t newest = ring[(ring_n - 1) % kRing].at_ns;
        uint64_t k = 0;
        while (k < have && newest - ring[(ring_n - 1 - k) % kRing].at_ns <= kKeepNs) k++;
        first = ring_n - k;
        fr.reserve(k);
        for (uint64_t s = first; s < ring_n; s++) fr.push_back(ring[s % kRing]);
    }
    std::string t = "# Tempo de cada quadro do jogo (de um present ao seguinte) nos últimos ~3 minutos; gpu_ms: tempo da GPU lido nesse "
                    "quadro; gpu_atrasada_ms: espera da GPU; replay_*: fases da CPU (amostradas, como no log)\n";
    t += frame_header("");
    t.reserve(t.size() + fr.size() * 140);
    for (size_t i = 0; i < fr.size(); i++) frame_row(t, "", first + i, fr[i]);
    return t;
}

// ---- the picture of the frame (vk_replay.cpp records it at a present when asked, reads it on a thread, hands it here) ----
std::atomic<int> shot_asks{0};
std::mutex shot_mu;
std::condition_variable shot_cv;
uint64_t shot_serial = 0;   // (pictures handed over so far)
int shot_waiters = 0;
struct Picture {
    std::shared_ptr<const std::vector<uint8_t>> rgba;
    uint32_t w = 0, h = 0;
};
Picture shot_last;
/// A picture of the game's frame taken from now on, waited for up to timeout_ms (the game may not present: minimized, loading)
Picture take_picture(int timeout_ms) {
    std::unique_lock lk(shot_mu);
    const uint64_t want = shot_serial + 1;
    shot_waiters++;
    shot_asks.fetch_add(1);
    const bool got = shot_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return shot_serial >= want; });
    Picture p = got ? shot_last : Picture{};
    if (--shot_waiters == 0) shot_last = Picture{};   // (its memory let go once every one asking has it)
    return p;
}
/// The picture as PNG bytes (write_png writes files: through a file of ours in the folder, removed after)
std::string png_bytes(const Picture& p, const std::wstring& scratch) {
    std::string out;
    if (!p.rgba || p.rgba->empty() || !p.w || !p.h) return out;
    if (write_png(utf8(scratch).c_str(), p.rgba->data(), p.w, p.h)) read_file(scratch, &out, 1ull << 31, 0);
    DeleteFileW(scratch.c_str());
    return out;
}

// ---- the bug marks (ocarina-marcas beside the game) ----
constexpr size_t kMaxMarks = 30;
constexpr uint64_t kMarksInReport = 200ull << 20;   // (the newest marks that fit in this, in the report)
std::wstring marks_dir() { return in_dir(L"ocarina-marcas"); }
/// The marks' names without their extension (marca-AAAAMMDD-HHMMSS-mmm), oldest first
std::vector<std::wstring> mark_stems() {
    std::vector<std::wstring> stems;
    WIN32_FIND_DATAW fd;
    const HANDLE h = FindFirstFileW((marks_dir() + L"\\marca-*.*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return stems;
    do {
        std::wstring n = fd.cFileName;
        const size_t dot = n.find_last_of(L'.');
        if (dot != std::wstring::npos) n.resize(dot);
        if (std::find(stems.begin(), stems.end(), n) == stems.end()) stems.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(stems.begin(), stems.end());
    return stems;
}
void prune_marks() {
    const std::vector<std::wstring> stems = mark_stems();
    for (size_t i = 0; i + kMaxMarks < stems.size(); i++)
        for (const wchar_t* ext : {L".png", L".txt", L".csv"}) DeleteFileW((marks_dir() + L"\\" + stems[i] + ext).c_str());
}

// ---- the report ----
struct Made {
    bool ok = false;
    std::wstring path;
    std::string name, error;
};

std::wstring report_name(bool automatic) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    const std::wstring base = widen(fmt("ocarina-relatorio-%s%04d%02d%02d-%02d%02d%02d", automatic ? "auto-" : "", t.wYear, t.wMonth,
                                        t.wDay, t.wHour, t.wMinute, t.wSecond));
    std::wstring name = base + L".zip";
    for (int k = 2; exists(in_dir(name.c_str())) && k < 100; k++) name = base + widen(fmt("-%d", k)) + L".zip";
    return name;
}

bool upload_on();   // (below: sending on)

Made make_report(bool automatic, const std::string& why) {
    Made m;
    if (game_dir().empty()) {
        m.error = "a pasta do jogo não foi encontrada";
        return m;
    }
    const Scrub scrub;
    // (first the picture of the frame on screen now: the manual report's; at the start there is no frame of the last session)
    std::string picture, picture_note;
    if (!automatic) {
        const Picture p = take_picture(3000);
        picture = png_bytes(p, in_dir(L"ocarina-relatorio-quadro.png"));
        picture_note = !picture.empty() ? fmt("quadro.png: o último quadro do jogo, %ux%u", p.w, p.h)
                                        : "quadro.png: não deu para tirar a foto (o jogo não estava mostrando quadros)";
    } else {
        picture_note = "sem foto: o relatório automático é feito no início, quando a sessão que fechou já não tem quadro na tela";
    }
    const std::wstring name = report_name(automatic), path = in_dir(name.c_str()), tmp = path + L".tmp";
    Zip z;
    if (!z.open(tmp)) {
        m.error = fmt("a pasta do jogo não aceitou o arquivo (erro %lu)", (unsigned long)GetLastError());
        return m;
    }
    // 1. the logs: this session's and the four before
    std::string session_log;
    for (int k = 0; k <= 4; k++) {
        const std::string file = k ? fmt("hookshot_log.%d.txt", k) : "hookshot_log.txt";
        std::string body;
        if (!read_file(in_dir(widen(file).c_str()), &body)) continue;
        body = scrub(body);
        if (k == (automatic ? 1 : 0)) session_log = body;
        z.add(file, body);
    }
    // 2. the frames: the last ~3 minutes, and the stalls of this session and of the one before
    const std::string frames = frames_csv();
    if (!frames.empty()) z.add("quadros.csv", frames);
    const std::string st = stalls_csv();
    if (!st.empty()) z.add("travadas.csv", st);
    {
        std::string before;
        if (read_file(in_dir(L"ocarina-travadas.1.csv"), &before) && !before.empty()) z.add("travadas_sessao_anterior.csv", before);
    }
    {   // (the scene's brightness, frame by frame: the flicker detector's series)
        const std::vector<Lum> lv = lums_since(kKeepNs);
        if (!lv.empty()) z.add("brilho.csv", lums_csv(lv));
    }
    // 3. the settings
    for (const wchar_t* file : {L"hookshot_options.ini", L"hookshot_env.txt"}) {
        std::string body;
        if (read_file(in_dir(file), &body)) z.add(utf8(file), scrub(body));
    }
    // 4. the freeze watchdog's thread stacks (RX_VK_HANG_DUMP=<file>, vk_hangdump.cpp), when the switch names one that exists
    {
        const std::string hang = env_utf8(L"RX_VK_HANG_DUMP");
        std::string body;
        if (!hang.empty() && read_file(widen(hang), &body)) z.add("travamento_pilhas.txt", scrub(body));
    }
    // 5. the picture, and the bug marks (the newest that fit in kMarksInReport)
    if (!picture.empty()) z.add("quadro.png", picture);
    int marks_in = 0;
    {
        const std::vector<std::wstring> stems = mark_stems();
        uint64_t total = 0;
        for (size_t i = stems.size(); i-- > 0;) {
            std::string png, txt, csv;
            read_file(marks_dir() + L"\\" + stems[i] + L".png", &png, 1ull << 31, 0);
            read_file(marks_dir() + L"\\" + stems[i] + L".txt", &txt);
            read_file(marks_dir() + L"\\" + stems[i] + L".csv", &csv);
            if (total + png.size() + txt.size() + csv.size() > kMarksInReport) break;
            total += png.size() + txt.size() + csv.size();
            if (!png.empty()) z.add("marcas/" + utf8(stems[i]) + ".png", png);
            if (!txt.empty()) z.add("marcas/" + utf8(stems[i]) + ".txt", scrub(txt));
            if (!csv.empty()) z.add("marcas/" + utf8(stems[i]) + "_brilho.csv", csv);
            marks_in++;
        }
    }
    // 6. sistema.txt
    const LogFacts lf = log_facts(session_log);
    SYSTEMTIME now;
    GetLocalTime(&now);
    std::string s = "RELATÓRIO DO OCARINA ENGINE\r\n";
    s += fmt("Gerado em %04d-%02d-%02d %02d:%02d:%02d, %s\r\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
             automatic ? "sozinho, no início do jogo" : "pelo botão Gerar relatório");
    if (automatic) s += "Motivo: " + why + " (os dados dessa sessão estão no hookshot_log.1.txt)\r\n";
    s += upload_on() ? "Este arquivo é enviado ao Ocarina Engine (Enviar relatórios automaticamente, no menu). Nome de usuário, pasta pessoal, "
                       "nome do computador, endereços IP e e-mails foram retirados.\r\n\r\n"
                     : "Nada deste arquivo foi enviado: quem o envia é você. Nome de usuário, pasta pessoal, nome do computador, endereços IP "
                       "e e-mails foram retirados.\r\n\r\n";
    s += picture_note + "\r\n";
    s += fmt("Bugs marcados com a tecla (pasta ocarina-marcas): %d no relatório\r\n", marks_in);
    s += fmt("Ocarina Engine %s (hookshot_vk.dll), API do renderizador %d, o jogo pede a API %u\r\n", OCARINA_VERSION, MKWVK_API_VERSION,
             host ? host->version : 0u);
    if (!lf.first.empty()) s += "Jogo (1ª linha do log): " + lf.first + "\r\n";
    std::string body = "\nPLACAS DE VÍDEO (Vulkan)\n" + gpus_text(!automatic);
    body += "\nPROCESSADOR\n  " + cpu_text() + "\n";
    MEMORYSTATUSEX ms{sizeof ms};
    if (GlobalMemoryStatusEx(&ms))
        body += fmt("\nMEMÓRIA RAM\n  %.1f GB no total, %.1f GB livres agora (%lu%% em uso)\n", ms.ullTotalPhys / 1073741824.0,
                    ms.ullAvailPhys / 1073741824.0, (unsigned long)ms.dwMemoryLoad);
    body += "\nWINDOWS\n  " + windows_text() + "\n";
    body += "\nJANELA\n" + window_text();
    body += "\nVARIÁVEIS DE AMBIENTE EM USO (RX_, HOS_, OCARINA_, HOOKSHOT_...)\n" + env_text(scrub);
    body += automatic ? "\nÚLTIMAS LINHAS DO LOG DA SESSÃO QUE FECHOU COM ERRO (hookshot_log.1.txt)\n" : "\nÚLTIMAS LINHAS DO LOG (hookshot_log.txt)\n";
    const std::pair<const char*, const std::string*> wanted[] = {{"VRAM", &lf.vram}, {"VRAM (governador)", &lf.governor},
                                                                   {"detalhe de textura", &lf.detail}, {"replay", &lf.replay},
                                                                   {"GPU time", &lf.gpu}, {"status", &lf.status},
                                                                   {"piscadas da cena", &lf.flicker}};
    for (const auto& [label, line] : wanted)
        body +=fmt("  %s: %s\n", label, line->empty() ? "(não apareceu)" : line->c_str());
    body += "\nERROS NO LOG (CRASH, FATAL, device lost)\n";
    if (lf.errors.empty()) body += "  (nenhum)\n";
    for (const std::string& e : lf.errors) body += "  " + e + "\n";
    if (automatic) {
        body += "\nCOMO O LOG TERMINOU (últimas linhas)\n";
        for (const std::string& e : lf.tail) body += "  " + e + "\n";
    }
    body += "\nTRAVADAS DESTA SESSÃO\n" + stalls_text();
    body += flicker_on() ? "\nPISCADAS DA CENA NESTA SESSÃO (detector de brilho; a série está em brilho.csv)\n" + flicker_text()
                         : "\nPISCADAS DA CENA: detector desligado (RX_VK_FLICKER=0)\n";
    {
        std::lock_guard lk(ring_mu);
        if (automatic && !ring_n) body +="  (relatório feito no início: o tempo dos quadros da sessão anterior não fica guardado, só as travadas dela)\n";
    }
    for (char ch : body) {   // (line ends as Notepad wants them)
        if (ch == '\n') s += '\r';
        s += ch;
    }
    z.add("sistema.txt", scrub(s));
    if (!z.close()) {
        DeleteFileW(tmp.c_str());
        m.error = "o arquivo não pôde ser gravado inteiro (disco cheio?)";
        return m;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        m.error = fmt("o arquivo não pôde ser renomeado (erro %lu)", (unsigned long)GetLastError());
        return m;
    }
    m.ok = true, m.path = path, m.name = utf8(name);
    logf("ocarina: relatório salvo: %s (%s)", m.name.c_str(), automatic ? "automático" : "pelo menu");
    return m;
}

/// The automatic reports beyond the newest five removed (the manual ones never)
void prune_auto() {
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd;
    const HANDLE h = FindFirstFileW(in_dir(L"ocarina-relatorio-auto-*.zip").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do names.push_back(fd.cFileName);
    while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i + 5 < names.size(); i++) DeleteFileW(in_dir(names[i].c_str()).c_str());
}

// ---- what the menu shows ----
std::mutex st_mu;
struct State {
    std::wstring last;   // (the last report's path)
    std::string detail;
    bool auto_note = false;   // (the last one is the automatic one of the start: the note on screen)
} st;
std::atomic<bool> busy{false};
int64_t toast_from = 0;
// (the bug marks: the one being saved, how many this session, the note's start)
std::atomic<bool> marking{false};
int marks_made = 0;
int64_t mark_note_from = 0;
std::string mark_note;

/// The log's last lines, as they are now (its queue flushed first)
std::vector<std::string> log_last_lines(size_t n) {
    if (host && host->log_flush) host->log_flush();
    std::string tail;
    std::vector<std::string> lines;
    if (!read_file(in_dir(L"hookshot_log.txt"), &tail, 0, 256u << 10)) return lines;
    size_t at = 0;
    while (at < tail.size()) {
        size_t e = tail.find('\n', at);
        if (e == std::string::npos) e = tail.size();
        std::string line = tail.substr(at, e - at);
        at = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        lines.push_back(line);
        if (lines.size() > n) lines.erase(lines.begin());
    }
    return lines;
}

void open_in_explorer(const std::wstring& path) {
    wchar_t win[MAX_PATH];
    const UINT n = GetWindowsDirectoryW(win, MAX_PATH);
    const std::wstring exe = n && n < MAX_PATH ? std::wstring(win, n) + L"\\explorer.exe" : L"explorer.exe";
    std::wstring cmd = L"\"" + exe + L"\" /select,\"" + path + L"\"";
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
}

/// The game in Portuguese: HOS_LANGUAGE, or language.txt's first code (host/nn_applet.c desired_language's sources)
bool game_pt() {
    static const bool pt = [] {
        std::string code = env_utf8(L"HOS_LANGUAGE");
        if (code.empty()) {
            std::string file;
            if (read_file(in_dir(L"language.txt"), &file, 4096, 0)) {
                size_t at = 0;
                while (at < file.size()) {
                    size_t e = file.find('\n', at);
                    if (e == std::string::npos) e = file.size();
                    std::string line = file.substr(at, e - at);
                    at = e + 1;
                    if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line.erase(0, 3);
                    const size_t s = line.find_first_not_of(" \t\r");
                    if (s == std::string::npos || line[s] == '#') continue;
                    code = line.substr(s);
                    break;
                }
            }
        }
        return code.size() >= 2 && low(code[0]) == 'p' && low(code[1]) == 't';
    }();
    return pt;
}

/// Sending on: the menu's Enviar relatórios automaticamente row, and not OCARINA_REPORT_UPLOAD=0 / RX_VK_REPORT_UPLOAD=0, and a
/// receiver set (ocarina_upload.h)
bool upload_on() { return opts.report_upload && ocarina_upload::env_allows(); }
bool menu_allows_upload() { return opts.report_upload != 0; }

/// The one place a saved report goes on from: to the receiver, in the background, when sending is on (ocarina_upload.h).
/// Called on the report's own thread, after it is written whole
void deliver(const Made& m, bool automatic) {
    if (upload_on()) ocarina_upload::queue(widen(m.name), automatic);
}

/// No automatic reports: the menu's Relatórios automáticos row off, or OCARINA_AUTO_REPORT=0 / RX_VK_AUTO_REPORT=0
bool auto_off() {
    if (!opts.auto_report) return true;
    for (const char* k : {"OCARINA_AUTO_REPORT", "RX_VK_AUTO_REPORT"})
        if (const char* e = getenv(k); e && *e && atoi(e) == 0 && *e == '0') return true;
    return false;
}

/// Whether the last session ended badly, and why ("" when it ended well, or it is not known which log is the last session's)
std::string last_session_problem() {
    // (hookshot_log.1.txt is the last session's only when this one moved it there: host/hos_main.c does when the game has no console.
    // A line of ours, flushed, then looked for in hookshot_log.txt, says so)
    char token[64];
    snprintf(token, sizeof token, "ocarina: relatório: sessão %lu", (unsigned long)GetCurrentProcessId());
    logf("%s", token);
    if (host && host->log_flush) host->log_flush();
    std::string cur, before;
    if (!read_file(in_dir(L"hookshot_log.txt"), &cur, 1u << 20, 1u << 20) || cur.find(token) == std::string::npos) return "";
    if (!read_file(in_dir(L"hookshot_log.1.txt"), &before) || before.empty()) return "";
    const LogFacts f = log_facts(before);
    for (const std::string& e : f.errors) {
        if (e.find("DEVICE_LOST") != std::string::npos || e.find("device was lost") != std::string::npos)
            return "a placa de vídeo parou de responder (device lost)";
    }
    for (const std::string& e : f.errors) {
        if (e.find("CRASH") != std::string::npos) return "o jogo travou e fechou (CRASH no log)";
        if (e.find("FATAL") != std::string::npos) return "o jogo parou com um erro fatal (FATAL no log)";
    }
    if (!f.clean_end) return "o jogo fechou sem terminar normalmente (fechado à força, travou ou caiu)";
    return "";
}
#endif   // _WIN32

} // namespace

void frame(Frame f) {
    bool save = false;
    {
        std::lock_guard lk(ring_mu);
#ifdef _WIN32
        if (!ring_n) clock0_ns = f.at_ns, GetSystemTimeAsFileTime(&clock0_ft);
#endif
        const uint64_t seq = ring_n;
        if (f.ms > 250 && seq >= kWarmup) {
            f.flags |= kSlow;
#ifdef _WIN32
            const HWND w = app_window();
            const bool away = w && IsIconic(w);   // (minimized: no frames shown, not a stall)
#else
            const bool away = false;
#endif
            if (!away) {
                if (f.ms > 1000) f.flags |= kStallBig;
                int keep = 0;
                for (int i = 0; i < slow_n; i++)
                    if (f.at_ns - slow_at[i] <= 10'000'000'000) slow_at[keep++] = slow_at[i];
                slow_n = keep;
                if (slow_n < 8) slow_at[slow_n++] = f.at_ns;
                if (slow_n >= 5) f.flags |= kStallMany, slow_n = 0;
            }
        }
        ring[seq % kRing] = f;
        ring_n++;
        if (f.flags & (kStallBig | kStallMany)) {
            const Stall s{seq, f.at_ns, f.ms, f.flags};
            stalls.push_back(s);
            if (stalls.size() > kMaxStalls) stalls.erase(stalls.begin());
            if (!snap_pending) snap_pending = true, snap_seq = seq, snap_why = s;
        }
        if (snap_pending && ring_n >= snap_seq + kAround + 1) {   // (the frames after it are in: the ~600 around it kept)
            Snapshot sn;
            sn.why = snap_why;
            sn.first_seq = snap_seq >= (uint64_t)kAround ? snap_seq - kAround : 0;
            sn.frames.reserve((size_t)(ring_n - sn.first_seq));
            for (uint64_t s = sn.first_seq; s < ring_n; s++) sn.frames.push_back(ring[s % kRing]);
            snaps.push_back(std::move(sn));
            if (snaps.size() > kMaxSnaps) snaps.erase(snaps.begin());
            snap_pending = false, save = true;
        }
    }
#ifdef _WIN32
    if (save) save_stalls_async();
#else
    (void)save;
#endif
}

bool flicker_on() {
    static const bool on = !(getenv("RX_VK_FLICKER") && atoi(getenv("RX_VK_FLICKER")) == 0 && getenv("RX_VK_FLICKER")[0] == '0');
    return on;
}

void flicker_cost(int64_t ns) {
    win_cost_ns.fetch_add(ns, std::memory_order_relaxed);
    win_cost_n.fetch_add(1, std::memory_order_relaxed);
}

// A flicker: a frame (or two in a row) brighter than the two frames on each side of it by more than the threshold, the frames
// before and after it alike (the scene went back). Looked for 3 samples back, once the frames after it are in
void scene_brightness(int64_t present, float luma) {
    std::string line;
    {
        std::lock_guard lk(lum_mu);
        Lum& in = lums[lum_n % kRing];
        in = Lum{present, steady_ns(), luma, 0};
        lum_n++;
        const auto at = [](uint64_t k) -> Lum& { return lums[k % kRing]; };
        if (lum_n >= 6) {
            const uint64_t n = lum_n - 1, i = n - 3;   // (i-2 .. i+3 = n-5 .. n)
            bool consecutive = true;
            for (uint64_t k = i - 2; k < n; k++) consecutive &= at(k + 1).present == at(k).present + 1;
            const float limit_k = flicker_thresh() / 100.0f;
            float pct = 0;
            int frames = 0;
            if (consecutive && at(i).present > last_hit + 2) {
                // (one frame: i, against i-2, i-1, i+1, i+2)
                const float base1 = (at(i - 2).luma + at(i - 1).luma + at(i + 1).luma + at(i + 2).luma) * 0.25f;
                const float lim1 = std::max(limit_k * base1, 1.0f);
                const float rise1 = at(i).luma - base1;
                if (rise1 > lim1 && std::fabs(at(i - 1).luma - at(i + 1).luma) < lim1 * 0.5f && at(i + 1).luma < at(i).luma - lim1 * 0.5f)
                    pct = rise1 / std::max(base1, 1.0f) * 100.0f, frames = 1;
                // (two frames: i and i+1, against i-2, i-1, i+2, i+3)
                const float base2 = (at(i - 2).luma + at(i - 1).luma + at(i + 2).luma + at(i + 3).luma) * 0.25f;
                const float lim2 = std::max(limit_k * base2, 1.0f);
                const float rise2 = std::min(at(i).luma, at(i + 1).luma) - base2;
                if (!frames && rise2 > lim2 && std::fabs(at(i - 1).luma - at(i + 2).luma) < lim2 * 0.5f)
                    pct = rise2 / std::max(base2, 1.0f) * 100.0f, frames = 2;
            }
            if (frames) {
                last_hit = at(i).present + frames - 1;
                at(i).pct = pct;
                if (frames == 2) at(i + 1).pct = pct;
                flickers.push_back({at(i).present, at(i).at_ns, pct, frames});
                if (flickers.size() > 500) flickers.erase(flickers.begin());
                flickers_total++;
                win_count++, win_largest = std::max(win_largest, pct);
                if (win_frames.size() < 200) win_frames += fmt("%s%lld", win_frames.empty() ? "" : " ", (long long)at(i).present);
            }
        }
        // (every 600 presents: the log's line, and what measuring cost)
        if (win_present0 < 0) win_present0 = present, win_ns0 = in.at_ns;
        if (present - win_present0 >= 600) {
            const double secs = (in.at_ns - win_ns0) / 1e9;
            const int64_t cost = win_cost_ns.exchange(0), cost_n = win_cost_n.exchange(0);
            line = fmt("vk: scene flicker: %d in 600 presents (%.1f per minute), the largest +%.1f%%%s%s; measuring %.1f us of CPU a present",
                       win_count, secs > 0 ? win_count * 60.0 / secs : 0.0, win_largest, win_frames.empty() ? "" : "; presents ",
                       win_frames.c_str(), cost_n ? cost / 1e3 / cost_n : 0.0);
            win_present0 = present, win_ns0 = in.at_ns, win_count = 0, win_largest = 0, win_frames.clear();
        }
    }
    if (!line.empty()) logf("%s", line.c_str());
}

#ifdef _WIN32
void begin() {
    static std::atomic<bool> begun{false};
    if (begun.exchange(true) || game_dir().empty()) return;
    // (the last session's stalls: aside as .1, for its automatic report and the next manual ones; one from an older session goes)
    const std::wstring stalls_now = in_dir(L"ocarina-travadas.csv"), stalls_before = in_dir(L"ocarina-travadas.1.csv");
    DeleteFileW(stalls_before.c_str());
    MoveFileExW(stalls_now.c_str(), stalls_before.c_str(), MOVEFILE_REPLACE_EXISTING);
    // (the sending: what an earlier start left pending goes once the notice about sending was shown, now or at an earlier start)
    ocarina_upload::start(game_dir(), menu_allows_upload);
    if (menu_report_notice_seen(2)) ocarina_upload::allow();
    if (auto_off()) return;
    busy.store(true);
    std::thread([] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        const std::string why = last_session_problem();
        if (!why.empty()) {
            logf("ocarina: a sessão anterior fechou com erro (%s): relatório automático", why.c_str());
            const Made m = make_report(true, why);
            prune_auto();
            if (m.ok) deliver(m, true);
            std::lock_guard lk(st_mu);
            if (m.ok) {
                st.last = m.path, st.auto_note = true;
                st.detail = "O jogo fechou com erro na última vez. Relatório salvo: " + m.name;
            } else {
                st.detail = "O jogo fechou com erro na última vez, mas o relatório não pôde ser salvo: " + m.error + ".";
            }
        }
        busy.store(false);
    }).detach();
}

void press() {
    if (busy.exchange(true)) return;
    {
        std::lock_guard lk(st_mu);
        st.detail = "Juntando os arquivos do relatório...";
    }
    std::thread([] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        const Made m = make_report(false, "");
        {
            std::lock_guard lk(st_mu);
            if (m.ok) {
                st.last = m.path, st.auto_note = false;
                st.detail = "Relatório salvo: " + m.name + (upload_on() ? "." : ". Ele está na pasta do jogo; anexe-o ao pedir ajuda.");
            } else {
                st.detail = "Não deu para gerar o relatório: " + m.error + ".";
            }
        }
        if (m.ok) open_in_explorer(m.path), deliver(m, false);
        busy.store(false);
    }).detach();
}

void open_folder() {
    std::wstring last;
    {
        std::lock_guard lk(st_mu);
        last = st.last;
    }
    if (!last.empty() && exists(last)) open_in_explorer(last);
    else if (!game_dir().empty()) open_in_explorer(in_dir(L"hookshot_log.txt"));
}

View view() {
    View v;
    const bool b = busy.load();
    std::lock_guard lk(st_mu);
    v.button = b ? "Gerando..." : "Gerar relatório";
    v.can_press = !b;
    v.folder = "Abrir pasta";
    v.can_open = !game_dir().empty();
    v.detail = st.detail;
    if (!st.last.empty() && v.detail.find("Relatório salvo") != std::string::npos) {   // (what became of its sending: ocarina_upload.h)
        const std::wstring& p = st.last;
        switch (ocarina_upload::fate(utf8(p.substr(p.find_last_of(L"\\/") + 1)))) {
            case ocarina_upload::Fate::kWaiting:
            case ocarina_upload::Fate::kSending: v.detail += " Enviando para o Ocarina Engine..."; break;
            case ocarina_upload::Fate::kSent: v.detail += " Enviado para o Ocarina Engine."; break;
            case ocarina_upload::Fate::kRetry: v.detail += " Sem conexão com o Ocarina Engine agora: vai no próximo início."; break;
            case ocarina_upload::Fate::kRefused: v.detail += " O Ocarina Engine não aceitou o envio; anexe o arquivo ao pedir ajuda."; break;
            case ocarina_upload::Fate::kNone: break;
        }
    }
    return v;
}

bool toast(std::string* text, std::string* sub) {
    std::lock_guard lk(st_mu);
    const int64_t now = steady_ns();
    if (mark_note_from && now - mark_note_from < 2'500'000'000) {
        *text = mark_note, *sub = "Foto e log guardados em ocarina-marcas; vão no próximo relatório.";
        return true;
    }
    if (st.auto_note) {
        if (!toast_from) toast_from = now;
        if (now - toast_from <= 10'000'000'000) {
            *text = st.detail, *sub = "Insert ou F1 > Gameplay > Ocarina Engine > Pasta do relatório abre a pasta.";
            return true;
        }
    }
    // (the first start with the reports sent: once, ~12 s, one line, saved as shown in hookshot_options.ini by the menu; nothing
    // is sent before it. With the automatic reports only saved, not sent: the notice of that, once, as before)
    static int64_t notice_from = 0;
    static const int level = upload_on() ? 2 : auto_off() ? 0 : 1;
    static const bool notice = level && !menu_report_notice_seen(level);
    if (!notice || (notice_from && now - notice_from > 12'000'000'000)) return false;
    if (!notice_from) {
        notice_from = now, menu_report_notice_done(level);
        if (level == 2) ocarina_upload::allow();
    }
    if (level == 2) {
        *text = game_pt() ? "Os relatórios de erro vão para o Ocarina Engine, sem dados pessoais. Para desligar: Insert/F1 > Gameplay > "
                            "Enviar relatórios automaticamente."
                          : "Error reports are sent to Ocarina Engine, with no personal data. To turn this off: Insert/F1 > Gameplay > "
                            "Enviar relatórios automaticamente.";
        sub->clear();
        return true;
    }
    if (game_pt()) {
        *text = "O Ocarina Engine gera relatórios de erro automaticamente para melhorar o jogo. Para desligar: Insert/F1 > Gameplay > "
                "Relatórios automáticos.";
        *sub = "Eles ficam só na pasta do jogo; nada é enviado pela internet.";
    } else {
        *text = "Ocarina Engine saves error reports automatically to help improve the game. To turn this off: Insert/F1 > Gameplay > "
                "Relatórios automáticos.";
        *sub = "They stay in the game folder; nothing is sent over the internet.";
    }
    return true;
}

int bug_key() {
    static const int key = [] {
        const char* e = getenv("RX_VK_BUG_KEY");
        if (!e) return (int)VK_F9;
        std::string v;
        for (const char* q = e; *q; q++)
            if (*q != ' ' && *q != '\t') v += (char)(*q >= 'a' && *q <= 'z' ? *q - 32 : *q);
        if (v.empty() || v == "0" || v == "OFF" || v == "NONE") return 0;
        const bool digits_after_f = v.size() >= 2 && v[0] == 'F' && v.find_first_not_of("0123456789", 1) == std::string::npos;
        if (digits_after_f && atoi(v.c_str() + 1) >= 1 && atoi(v.c_str() + 1) <= 24) return (int)VK_F1 + atoi(v.c_str() + 1) - 1;
        if (v.size() == 1 && ((v[0] >= 'A' && v[0] <= 'Z') || (v[0] >= '0' && v[0] <= '9'))) return (int)v[0];
        if (v.find_first_not_of("0123456789") == std::string::npos && atoi(v.c_str()) > 0 && atoi(v.c_str()) < 256) return atoi(v.c_str());
        logf("ocarina: RX_VK_BUG_KEY=%s não é uma tecla conhecida: F9 no lugar", e);
        return (int)VK_F9;
    }();
    return key;
}

void mark_bug() {
    if (game_dir().empty() || marking.exchange(true)) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    uint64_t frame_no = 0;
    {
        std::lock_guard lk(ring_mu);
        frame_no = ring_n;
        if (ring_n) ring[(ring_n - 1) % kRing].flags |= kMark;   // (quadros.csv says where)
    }
    int n = 0;
    {
        std::lock_guard lk(st_mu);
        n = ++marks_made;
        mark_note = fmt("Bug marcado (%d)", n), mark_note_from = steady_ns();
    }
    logf("ocarina: bug marcado (%d), quadro %llu", n, (unsigned long long)frame_no);
    std::thread([t, frame_no, n] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        const std::vector<std::string> lines = log_last_lines(40);
        const Picture p = take_picture(3000);
        CreateDirectoryW(marks_dir().c_str(), nullptr);
        const std::string stem = fmt("marca-%04d%02d%02d-%02d%02d%02d-%03d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                                     t.wMilliseconds);
        const std::wstring base = marks_dir() + L"\\" + widen(stem);
        const bool png = p.rgba && !p.rgba->empty() && write_png(utf8(base + L".png").c_str(), p.rgba->data(), p.w, p.h);
        std::string txt = fmt("Bug marcado no Ocarina Engine %s (marca %d desta sessão)\r\n", OCARINA_VERSION, n);
        txt += fmt("Hora: %04d-%02d-%02d %02d:%02d:%02d.%03d\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        txt += fmt("Quadro: %llu (quadros do jogo desde o início; a mesma contagem do quadros.csv)\r\n", (unsigned long long)frame_no);
        txt += png ? fmt("Foto: %s.png (%ux%u, a imagem do jogo sem o menu)\r\n", stem.c_str(), p.w, p.h)
                   : std::string("Foto: não deu para tirar (o jogo não estava mostrando quadros)\r\n");
        txt += "\r\nÚltimas linhas do log nesse momento:\r\n";
        for (const std::string& l : lines) txt += l + "\r\n";
        write_file(base + L".txt", Scrub()(txt));
        if (flicker_on()) {   // (the scene's brightness around it: ~1 s after the key, the 600 frames up to then)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            const std::vector<Lum> lv = lums_since(0, 600);
            if (!lv.empty()) write_file(base + L".csv", lums_csv(lv));
        }
        prune_marks();
        marking.store(false);
    }).detach();
}

bool shot_wanted() { return shot_asks.exchange(0) > 0; }

void shot_taken(std::vector<uint8_t> rgba, uint32_t w, uint32_t h) {
    std::lock_guard lk(shot_mu);
    if (shot_waiters) shot_last = Picture{std::make_shared<const std::vector<uint8_t>>(std::move(rgba)), w, h};
    shot_serial++;
    shot_cv.notify_all();
}
#else
void begin() {}
void press() {}
void open_folder() {}
View view() {
    View v;
    v.button = "Gerar relatório";
    v.folder = "Abrir pasta";
    v.detail = "O relatório pelo menu é só no Windows.";
    return v;
}
bool toast(std::string*, std::string*) { return false; }
int bug_key() { return 0; }
void mark_bug() {}
bool shot_wanted() { return false; }
void shot_taken(std::vector<uint8_t>, uint32_t, uint32_t) {}
#endif

const char* sample(int k) {
    static const char* const t[] = {
        "O jogo fechou com erro na última vez. Relatório salvo: ocarina-relatorio-auto-20261111-235959-2.zip",
        "O jogo fechou com erro na última vez, mas o relatório não pôde ser salvo: o arquivo não pôde ser gravado inteiro (disco cheio?).",
        "Relatório salvo: ocarina-relatorio-20261111-235959-2.zip. Ele está na pasta do jogo; anexe-o ao pedir ajuda.",
        "Relatório salvo: ocarina-relatorio-20261111-235959-2.zip. O Ocarina Engine não aceitou o envio; anexe o arquivo ao pedir ajuda.",
        "O jogo fechou com erro na última vez. Relatório salvo: ocarina-relatorio-auto-20261111-235959-2.zip Sem conexão com o Ocarina "
        "Engine agora: vai no próximo início.",
    };
    return k >= 0 && k < (int)(sizeof t / sizeof t[0]) ? t[k] : nullptr;
}

} // namespace mkwvk::ocarina_report
