// (ocarina) Ocarina Engine's update from the Insert menu: ocarina_update.h. The check mirrors the host's own (host/hos_update.c:
// WinHTTP, GitHub's releases/latest, HOOKSHOT_NO_UPDATE_CHECK), off the game's threads: a thread of its own, once per start and on
// the player's button. Nothing personal is sent: a GET with a generic User-Agent ("OcarinaEngine"). The running DLL is never
// touched until the new one is downloaded whole and checked; a failure at any step leaves the install as it was
#include "ocarina_update.h"
#include "ocarina_brand.h"
#include "vk_api.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#include "vk_paths.h"   // (windows.h as the back end includes it)
#include <winhttp.h>
#endif

#define OCARINA_STR2(x) #x
#define OCARINA_STR(x) OCARINA_STR2(x)

namespace mkwvk {
void logf(const char* fmt, ...);   // (vk_main.cpp)
extern const MkwVkHost* host;      // (vk_main.cpp: the game's, its API version)
}

#ifdef _WIN32
// (exported: read from the file, without running it, by a newer copy's verify_dll, which refuses a DLL of another API than the
// game's. "ocarina-engine api=<MKWVK_API_VERSION> version=<OCARINA_VERSION>")
extern "C" __declspec(dllexport) const char ocarina_engine_info[] =
    "ocarina-engine api=" OCARINA_STR(MKWVK_API_VERSION) " version=" OCARINA_VERSION;
#endif

namespace mkwvk::ocarina_update {
namespace {

constexpr const char* kOurs = "v" OCARINA_VERSION;
constexpr const char* kReleasesPage = "github.com/Samuel25z/ocarina-engine/releases";

std::string fmt(const char* f, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}

/// A release tag as shown ("v" in front) and as it may be put in a URL: letters, digits and . _ + - (host/hos_update.c safe_tag)
bool safe_tag(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (char ch : s)
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch && strchr("._+-", ch)))) return false;
    return true;
}
std::string shown_tag(const std::string& t) { return !t.empty() && (t[0] == 'v' || t[0] == 'V') ? t : "v" + t; }

/// A version, "v1.2.3", "1.2", "0.3.0-rc1": up to four numbers; one with a "-" after them (a pre-release) comes before the same
/// numbers without. ok false when it starts with no number
struct Ver {
    long n[4] = {0, 0, 0, 0};
    bool pre = false, ok = false;
};
Ver parse_ver(const std::string& t) {
    Ver v;
    const char* p = t.c_str();
    if (*p == 'v' || *p == 'V') p++;
    for (int i = 0; i < 4; i++) {
        if (*p < '0' || *p > '9') break;
        v.n[i] = strtol(p, (char**)&p, 10);
        v.ok = true;
        if (*p != '.') break;
        p++;
    }
    v.pre = *p == '-';
    return v;
}
int cmp_ver(const Ver& a, const Ver& b) {
    for (int i = 0; i < 4; i++)
        if (a.n[i] != b.n[i]) return a.n[i] < b.n[i] ? -1 : 1;
    if (a.pre != b.pre) return a.pre ? -1 : 1;
    return 0;
}

// ---- what the menu shows (st under mu) ----
struct State {
    Phase phase = kOff;
    std::string tag;       // (the latest release's, once asked)
    std::string dll_url, env_url;
    uint64_t dll_size = 0;
    std::string detail;
    std::string installed;   // (the tag put in place)
    int percent = 0;
};
std::mutex mu;
State st;
std::atomic<bool> busy{false};
std::atomic<bool> begun{false};

void set_phase(Phase p, const std::string& detail) {
    std::lock_guard<std::mutex> lock(mu);
    st.phase = p, st.detail = detail;
}

const char* const kHelpIdle = "A busca no início está desligada (HOOKSHOT_NO_UPDATE_CHECK=1). Procurar atualização pergunta ao GitHub agora.";

} // namespace

const char* sample(int k) {
    // (the longest each detail can be, with a long tag in it: the help box keeps that height)
    static std::string t[8];
    static bool made = false;
    if (!made) {
        made = true;
        const char* x = "v00.00.00-rc00";
        t[0] = fmt("Saiu a %s do Ocarina Engine (esta é a %s). Atualizar baixa a hookshot_vk.dll nova, confere se ela serve para este "
                   "jogo e a coloca no lugar; a atual fica guardada até o próximo início.", x, x);
        t[1] = fmt("A %s precisa de outro HookshotBuilder (ela é da API 00; este jogo é da 00). Nada foi trocado: monte o jogo de novo "
                   "com o HookshotBuilder novo e depois atualize.", x);
        t[2] = fmt("Reinicie o jogo para usar a %s. A versão anterior ficou guardada como hookshot_vk.dll.old e é apagada no próximo "
                   "início. Seu hookshot_env.txt foi mantido; o da %s ficou ao lado, como hookshot_env.txt.novo.", x, x);
        t[3] = fmt("A %s não diz para qual HookshotBuilder foi feita. Nada foi trocado; instale-a à mão, pela página %s.", x, kReleasesPage);
        t[4] = kHelpIdle;
    }
    return k >= 0 && k < 5 ? t[k].c_str() : nullptr;
}

#ifdef _WIN32
namespace {

constexpr const wchar_t* kAgent = L"OcarinaEngine";
constexpr const wchar_t* kApiUrl = L"https://api.github.com/repos/Samuel25z/ocarina-engine/releases/latest";
// (a release's files only from this repository's releases: what the API says is taken from nowhere else)
constexpr const char* kDownloadPrefix = "https://github.com/Samuel25z/ocarina-engine/releases/download/";
constexpr uint64_t kDllMin = 1u << 20, kDllMax = 256u << 20;   // (v0.2.0's is 10 MB)
constexpr uint64_t kEnvMax = 64u << 10;

bool env_set(const char* name) {
    const char* v = getenv(name);
    return v && *v && strcmp(v, "0");
}
std::wstring widen(const std::string& s) {
    std::wstring w;
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n > 0) {
        w.resize((size_t)n);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    }
    return w;
}

// ---- paths ----
/// This DLL's own file (hookshot_vk.dll, beside the exe: the game folder or its bin\)
std::wstring own_path() {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)(const void*)&ocarina_engine_info, &m))
        return L"";
    std::wstring p(32768, L'\0');
    const DWORD n = GetModuleFileNameW(m, p.data(), (DWORD)p.size());
    if (!n || n >= p.size()) return L"";
    p.resize(n);
    return p;
}
std::wstring dir_of(const std::wstring& p) {
    const size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"" : p.substr(0, k);
}
/// The game folder, where hookshot_env.txt is (host/hos_main.c load_hookshot_env; vk_paths.h's rule: the exe's folder, or the
/// one above when the exe is in a "bin" folder beside image.bin)
std::wstring game_dir() {
    std::wstring exe(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, exe.data(), (DWORD)exe.size());
    if (!n || n >= exe.size()) return L"";
    exe.resize(n);
    const std::wstring dir = dir_of(exe), up = dir_of(dir);
    const std::wstring last = dir.substr(up.size() + (up.empty() ? 0 : 1));
    if (!up.empty() && !_wcsicmp(last.c_str(), L"bin") && GetFileAttributesW((up + L"\\image.bin").c_str()) != INVALID_FILE_ATTRIBUTES)
        return up;
    return dir;
}
bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

// ---- HTTPS (host/hos_update.c latest_tag, builder/setup/setup.c http_get) ----
enum Net { kNetOk, kNetOffline, kNetStatus, kNetTooBig, kNetBadUrl };
struct Got {
    Net net = kNetOffline;
    DWORD status = 0;
    std::string body;
};
/// GET url (https only; redirects followed, never to http): the body when the answer is 200, at most max bytes. progress(have,
/// total) as it comes (total 0 when not said)
template <class Progress>
Got http_get(const std::wstring& url, bool api, uint64_t max, Progress progress) {
    Got g;
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof uc;
    wchar_t host[256] = {}, path[2048] = {}, extra[2048] = {};
    uc.lpszHostName = host, uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path, uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra, uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
        g.net = kNetBadUrl;
        return g;
    }
    HINTERNET s = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return g;
    WinHttpSetTimeouts(s, 10000, 10000, 15000, api ? 15000 : 60000);
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(s, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy);
    DWORD tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000;   // (| TLS 1.3, WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3, where known)
    if (!WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof tls)) {
        tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof tls);
    }
    std::wstring object(path, uc.dwUrlPathLength);   // (with its ?query, if any)
    if (uc.lpszExtraInfo && uc.dwExtraInfoLength) object.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    HINTERNET c = WinHttpConnect(s, std::wstring(host, uc.dwHostNameLength).c_str(), uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE)
                    : nullptr;
    const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n"
                                 : L"Accept: application/octet-stream\r\n";
    if (r && WinHttpSendRequest(r, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD size = sizeof g.status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &g.status, &size,
                            WINHTTP_NO_HEADER_INDEX);
        if (g.status != 200) {
            g.net = kNetStatus;
        } else {
            DWORD total = 0;
            size = sizeof total;
            if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &total,
                                     &size, WINHTTP_NO_HEADER_INDEX))
                total = 0;
            if (total > max) {
                g.net = kNetTooBig;
            } else {
                g.body.reserve(total ? total : 64 << 10);
                g.net = kNetOk;
                char buf[64 << 10];
                for (;;) {
                    DWORD got = 0;
                    if (!WinHttpReadData(r, buf, sizeof buf, &got)) {
                        g.net = kNetOffline;
                        break;
                    }
                    if (!got) break;
                    if (g.body.size() + got > max) {
                        g.net = kNetTooBig;
                        break;
                    }
                    g.body.append(buf, got);
                    progress((uint64_t)g.body.size(), (uint64_t)total);
                }
                if (g.net == kNetOk && total && g.body.size() != total) g.net = kNetOffline;   // (cut short)
            }
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    if (g.net != kNetOk) g.body.clear();
    return g;
}
Got http_get(const std::wstring& url, bool api, uint64_t max) {
    return http_get(url, api, max, [](uint64_t, uint64_t) {});
}

// ---- GitHub's answer (a few fields of releases/latest, as host/hos_update.c reads tag_name) ----
/// The string value after "key": at or after from; npos when none
size_t json_string(const std::string& j, const char* key, size_t from, std::string* out, size_t before = std::string::npos) {
    const std::string k = std::string("\"") + key + "\"";
    size_t at = j.find(k, from);
    if (at == std::string::npos || at >= before) return std::string::npos;
    size_t p = at + k.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == ':' || j[p] == '\t' || j[p] == '\n' || j[p] == '\r')) p++;
    if (p >= j.size() || j[p] != '"') return std::string::npos;
    const size_t e = j.find('"', p + 1);
    if (e == std::string::npos) return std::string::npos;
    *out = j.substr(p + 1, e - p - 1);
    return e + 1;
}
struct Release {
    std::string tag, dll_url, env_url;
    uint64_t dll_size = 0;
};
/// tag_name and the assets hookshot_vk.dll and hookshot_env.txt (each asset's name is its browser_download_url's last part; its
/// "size" the last one before that URL, past the asset before)
bool parse_release(const std::string& j, Release* r) {
    if (json_string(j, "tag_name", 0, &r->tag) == std::string::npos || !safe_tag(r->tag)) return false;
    const size_t assets = j.find("\"assets\"");
    if (assets == std::string::npos) return true;
    size_t from = assets, prev_end = assets;
    std::string url;
    for (;;) {
        const size_t key = j.find("\"browser_download_url\"", from);
        if (key == std::string::npos) break;
        const size_t end = json_string(j, "browser_download_url", key, &url);
        if (end == std::string::npos) break;
        from = end;
        if (url.size() <= strlen(kDownloadPrefix) || _strnicmp(url.c_str(), kDownloadPrefix, strlen(kDownloadPrefix))) {
            prev_end = end;
            continue;
        }
        const std::string name = url.substr(url.find_last_of('/') + 1);
        uint64_t size = 0;
        for (size_t p = j.find("\"size\"", prev_end); p != std::string::npos && p < key; p = j.find("\"size\"", p + 6)) {
            const char* q = j.c_str() + p + 6;
            while (*q == ' ' || *q == ':') q++;
            size = strtoull(q, nullptr, 10);
        }
        if (name == "hookshot_vk.dll" && r->dll_url.empty()) r->dll_url = url, r->dll_size = size;
        else if (name == "hookshot_env.txt" && r->env_url.empty()) r->env_url = url;
        prev_end = end;
    }
    return true;
}

// ---- the new DLL, checked without running it ----
template <class T> bool rd(const std::string& b, uint64_t off, T* out) {
    if (off > b.size() || b.size() - off < sizeof(T)) return false;
    memcpy(out, b.data() + off, sizeof(T));
    return true;
}
struct Pe {
    const std::string& b;
    uint64_t sections = 0;
    unsigned nsec = 0;
    uint32_t headers = 0;
    bool to_off(uint32_t rva, uint64_t* off) const {
        if (rva < headers) return *off = rva, rva < b.size();
        for (unsigned i = 0; i < nsec; i++) {
            IMAGE_SECTION_HEADER h;
            if (!rd(b, sections + (uint64_t)i * sizeof h, &h)) return false;
            if (rva >= h.VirtualAddress && rva - h.VirtualAddress < h.SizeOfRawData) {
                *off = (uint64_t)h.PointerToRawData + (rva - h.VirtualAddress);
                return *off < b.size();
            }
        }
        return false;
    }
    bool cstr(uint32_t rva, size_t max, std::string* out) const {
        uint64_t off;
        if (!to_off(rva, &off)) return false;
        const char* p = b.data() + off;
        const size_t room = (size_t)std::min<uint64_t>(max, b.size() - off);
        const void* z = memchr(p, 0, room);
        if (!z) return false;
        out->assign(p, (const char*)z - p);
        return true;
    }
};
enum Check { kCheckOk, kCheckNotDll, kCheckNoInit, kCheckNoInfo };
/// A Windows DLL of this machine, with the export mkwvk_init (host/rx_backend.c load_vulkan) and ocarina_engine_info: its text
Check verify_dll(const std::string& b, std::string* info) {
    IMAGE_DOS_HEADER dos;
    if (!rd(b, 0, &dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) return kCheckNotDll;
    IMAGE_NT_HEADERS nt;
    if (!rd(b, (uint64_t)dos.e_lfanew, &nt) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC || !(nt.FileHeader.Characteristics & IMAGE_FILE_DLL))
        return kCheckNotDll;
    {   // (the machine this DLL was built for: the same as the running one's)
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(const void*)&ocarina_engine_info, &self);
        if (self) {
            const auto* sd = (const IMAGE_DOS_HEADER*)self;
            const auto* sn = (const IMAGE_NT_HEADERS*)((const char*)self + sd->e_lfanew);
            if (sn->FileHeader.Machine != nt.FileHeader.Machine) return kCheckNotDll;
        }
    }
    Pe pe{b};
    pe.sections = (uint64_t)dos.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    pe.nsec = nt.FileHeader.NumberOfSections;
    pe.headers = nt.OptionalHeader.SizeOfHeaders;
    if (pe.nsec == 0 || pe.nsec > 96 || pe.sections + (uint64_t)pe.nsec * sizeof(IMAGE_SECTION_HEADER) > b.size()) return kCheckNotDll;
    if (nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return kCheckNoInit;
    const IMAGE_DATA_DIRECTORY dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    uint64_t eoff;
    IMAGE_EXPORT_DIRECTORY ed;
    if (!dir.VirtualAddress || !pe.to_off(dir.VirtualAddress, &eoff) || !rd(b, eoff, &ed)) return kCheckNoInit;
    if (ed.NumberOfNames > 65536) return kCheckNotDll;
    uint64_t names, ords, funcs;
    if (ed.NumberOfNames && (!pe.to_off(ed.AddressOfNames, &names) || !pe.to_off(ed.AddressOfNameOrdinals, &ords) ||
                             !pe.to_off(ed.AddressOfFunctions, &funcs)))
        return kCheckNotDll;
    bool init = false;
    uint32_t info_rva = 0;
    std::string name;
    for (uint32_t i = 0; i < ed.NumberOfNames; i++) {
        uint32_t nrva;
        uint16_t ord;
        if (!rd(b, names + 4ull * i, &nrva)) return kCheckNotDll;
        if (!pe.cstr(nrva, 4096, &name)) continue;   // (a name not in the file's bytes: not one of ours)
        if (name == "mkwvk_init") init = true;
        else if (name == "ocarina_engine_info") {
            if (!rd(b, ords + 2ull * i, &ord) || ord >= ed.NumberOfFunctions || !rd(b, funcs + 4ull * ord, &info_rva)) return kCheckNotDll;
        }
    }
    if (!init) return kCheckNoInit;
    if (!info_rva || !pe.cstr(info_rva, 256, info)) return kCheckNoInfo;
    return kCheckOk;
}

// ---- files ----
bool write_file(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    size_t done = 0;
    bool ok = true;
    while (ok && done < data.size()) {
        DWORD n = 0;
        const DWORD want = (DWORD)std::min<size_t>(data.size() - done, 1u << 24);
        ok = WriteFile(h, data.data() + done, want, &n, nullptr) && n == want;
        done += n;
    }
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
}
bool read_file(const std::wstring& path, uint64_t max, std::string* out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && (uint64_t)size.QuadPart <= max;
    if (ok) {
        out->resize((size_t)size.QuadPart);
        DWORD n = 0;
        ok = out->empty() || (ReadFile(h, out->data(), (DWORD)out->size(), &n, nullptr) && n == out->size());
    }
    CloseHandle(h);
    return ok;
}
/// A text file as compared: no BOM, no carriage returns, no blank end
std::string normal_text(std::string t) {
    if (t.size() >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) t.erase(0, 3);
    std::string o;
    o.reserve(t.size());
    for (char ch : t)
        if (ch != '\r') o += ch;
    while (!o.empty() && (o.back() == '\n' || o.back() == ' ' || o.back() == '\t')) o.pop_back();
    return o;
}

// ---- the work, on a thread of its own ----
std::string net_error(const Got& g, const char* what) {
    if (g.net == kNetStatus && (g.status == 403 || g.status == 429))
        return "O GitHub limitou as consultas por agora. Tente de novo daqui a alguns minutos.";
    if (g.net == kNetStatus && g.status == 404) return fmt("%s não foi encontrado no GitHub (resposta 404).", what);
    if (g.net == kNetStatus) return fmt("O GitHub respondeu com o erro %lu ao pedir %s. Tente de novo mais tarde.", (unsigned long)g.status, what);
    if (g.net == kNetTooBig) return fmt("%s veio maior do que o esperado e foi recusado.", what);
    return "Não deu para falar com o GitHub (sem internet?). O jogo segue normal; tente de novo mais tarde.";
}

void check_now() {
    set_phase(kChecking, "Perguntando ao GitHub qual é a versão mais nova...");
    const Got g = http_get(kApiUrl, true, 1u << 20);
    if (g.net != kNetOk) {
        if (g.net == kNetStatus && g.status == 404) {
            set_phase(kError, "Ainda não há uma versão do Ocarina Engine publicada no GitHub.");
        } else {
            set_phase(kError, net_error(g, "A versão mais nova"));
        }
        logf("ocarina: update check failed (net %d, HTTP %lu)", (int)g.net, (unsigned long)g.status);
        return;
    }
    Release rel;
    if (!parse_release(g.body, &rel)) {
        set_phase(kError, "A resposta do GitHub veio num formato inesperado. Tente de novo mais tarde.");
        logf("ocarina: update check: no usable tag_name in GitHub's answer");
        return;
    }
    const Ver latest = parse_ver(rel.tag), ours = parse_ver(OCARINA_VERSION);
    logf("ocarina: this is %s, the latest release is %s%s", kOurs, rel.tag.c_str(), rel.dll_url.empty() ? " (no hookshot_vk.dll in it)" : "");
    std::lock_guard<std::mutex> lock(mu);
    st.tag = rel.tag, st.dll_url = rel.dll_url, st.env_url = rel.env_url, st.dll_size = rel.dll_size;
    if (!latest.ok || cmp_ver(latest, ours) <= 0) {
        st.phase = kUpToDate;
        st.detail = fmt("Esta é a versão mais nova (a última no GitHub é a %s).", shown_tag(rel.tag).c_str());
    } else if (rel.dll_url.empty()) {
        st.phase = kError;
        st.detail = fmt("A %s saiu, mas sem o arquivo hookshot_vk.dll para baixar. Baixe-a à mão pela página %s.",
                        shown_tag(rel.tag).c_str(), kReleasesPage);
    } else {
        st.phase = kAvailable;
        st.detail = fmt("Saiu a %s do Ocarina Engine (esta é a %s). Atualizar baixa a hookshot_vk.dll nova, confere se ela serve para "
                        "este jogo e a coloca no lugar; a atual fica guardada até o próximo início.",
                        shown_tag(rel.tag).c_str(), kOurs);
    }
}

void install_now() {
    std::string tag, dll_url, env_url;
    uint64_t dll_size;
    {
        std::lock_guard<std::mutex> lock(mu);
        tag = st.tag, dll_url = st.dll_url, env_url = st.env_url, dll_size = st.dll_size;
        st.phase = kDownloading, st.percent = 0;
        st.detail = fmt("Baixando a %s. Pode continuar jogando: a versão atual só é trocada no fim, depois de conferida.",
                        shown_tag(tag).c_str());
    }
    const std::string v = shown_tag(tag);
    const auto fail = [&](const std::string& why) {
        set_phase(kError, why);
        logf("ocarina: update to %s not made: %s", tag.c_str(), why.c_str());
    };
    const std::wstring self = own_path();
    if (self.empty()) return fail("Não deu para achar o arquivo hookshot_vk.dll em uso. Nada foi trocado.");

    // 1. the DLL, whole, into memory
    const Got dll = http_get(widen(dll_url), false, kDllMax, [&](uint64_t have, uint64_t total) {
        if (!total) total = dll_size;
        const int pc = total ? (int)std::min<uint64_t>(99, have * 100 / total) : 0;
        std::lock_guard<std::mutex> lock(mu);
        st.percent = pc;
    });
    if (dll.net != kNetOk) {
        return fail(dll.net == kNetOffline ? fmt("O download da %s não terminou (a internet caiu?). Nada foi trocado; tente de novo.", v.c_str())
                                           : net_error(dll, "O arquivo hookshot_vk.dll") + " Nada foi trocado.");
    }
    if ((dll_size && dll.body.size() != dll_size) || dll.body.size() < kDllMin)
        return fail(fmt("O arquivo da %s veio incompleto ou com tamanho estranho (%llu bytes). Nada foi trocado; tente de novo.", v.c_str(),
                        (unsigned long long)dll.body.size()));

    // 2. checked: a DLL for this machine, the export the game calls, and the same API as this game's (MKWVK_API_VERSION)
    std::string info;
    switch (verify_dll(dll.body, &info)) {
    case kCheckNotDll: return fail(fmt("O arquivo da %s não é uma DLL do Windows para este PC. Nada foi trocado.", v.c_str()));
    case kCheckNoInit: return fail(fmt("O arquivo da %s não tem a entrada mkwvk_init: não é um Ocarina Engine. Nada foi trocado.", v.c_str()));
    case kCheckNoInfo:
        return fail(fmt("A %s não diz para qual HookshotBuilder foi feita. Nada foi trocado; instale-a à mão, pela página %s.", v.c_str(),
                        kReleasesPage));
    case kCheckOk: break;
    }
    unsigned api = 0;
    char their[64] = "";
    if (sscanf(info.c_str(), "ocarina-engine api=%u version=%63s", &api, their) != 2)
        return fail(fmt("A %s se identifica de um jeito que esta versão não entende (\"%s\"). Nada foi trocado; instale-a à mão, pela "
                        "página %s.", v.c_str(), info.substr(0, 60).c_str(), kReleasesPage));
    const unsigned game_api = host ? host->version : (unsigned)MKWVK_API_VERSION;
    if (api != game_api)
        return fail(fmt("A %s precisa de outro HookshotBuilder (ela é da API %u; este jogo é da %u). Nada foi trocado: monte o jogo de novo "
                        "com o HookshotBuilder novo e depois atualize.", v.c_str(), api, game_api));
    if (cmp_ver(parse_ver(their), parse_ver(tag)) != 0)   // (else the next start would offer it again, and again)
        return fail(fmt("O arquivo da %s diz ser a v%s. Nada foi trocado; avise o autor.", v.c_str(), their));

    // 3. hookshot_env.txt of the release, if it has one (before anything is changed: a failure here only skips it)
    std::string env_new;
    const bool env_own_file = getenv("HOS_ENV_FILE") && *getenv("HOS_ENV_FILE");   // (a test run's switches: left alone)
    if (!env_url.empty() && !env_own_file) {
        const Got e = http_get(widen(env_url), false, kEnvMax);
        if (e.net == kNetOk) env_new = e.body;
        else logf("ocarina: the release's hookshot_env.txt could not be downloaded (net %d, HTTP %lu): left as it is", (int)e.net,
                  (unsigned long)e.status);
    }

    // 4. in place: the new one written beside, the running one renamed (Windows allows that for a loaded DLL), the new one renamed
    // to its name; any failure puts the old one back
    const std::wstring fresh = self + L".new", old = self + L".old";
    if (!write_file(fresh, dll.body))
        return fail("Não deu para gravar na pasta do jogo (sem permissão ou disco cheio?). Nada foi trocado.");
    if (exists(old) && !DeleteFileW(old.c_str())) {   // (a .old still in use: by another copy of the game)
        wchar_t aside[32];
        _snwprintf(aside, 32, L".%llu", (unsigned long long)GetTickCount64());
        MoveFileExW(old.c_str(), (old + aside).c_str(), 0);
    }
    if (!MoveFileExW(self.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD e = GetLastError();
        DeleteFileW(fresh.c_str());
        return fail(fmt("Não deu para trocar a hookshot_vk.dll (erro %lu: arquivo em uso ou sem permissão). A versão atual continua.",
                        (unsigned long)e));
    }
    if (!MoveFileExW(fresh.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD e = GetLastError();
        const bool back = MoveFileExW(old.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        DeleteFileW(fresh.c_str());
        logf("ocarina: update: the new DLL could not take the old one's name (%lu); old one put back: %d", (unsigned long)e, (int)back);
        return fail(back ? fmt("Não deu para colocar a %s no lugar (erro %lu). A versão atual continua.", v.c_str(), (unsigned long)e)
                         : fmt("Não deu para colocar a %s no lugar (erro %lu). Renomeie hookshot_vk.dll.old para hookshot_vk.dll "
                               "na pasta do jogo antes de abri-lo de novo.", v.c_str(), (unsigned long)e));
    }
    logf("ocarina: hookshot_vk.dll %s in place (the running one is hookshot_vk.dll.old until the next start)", tag.c_str());

    // 5. hookshot_env.txt: the new release's when the player has none, or theirs is still this release's own (unchanged); else
    // theirs is kept and the new one put beside it
    std::string env_note;
    if (!env_new.empty()) {
        const std::wstring dir = game_dir(), env = dir + L"\\hookshot_env.txt";
        std::string mine;
        const bool have = read_file(env, kEnvMax, &mine);
        bool replace = !have;
        if (have && normal_text(mine) == normal_text(env_new)) {
            replace = false;   // (already the same)
        } else if (have) {
            const Got prev = http_get(widen(std::string(kDownloadPrefix) + kOurs + "/hookshot_env.txt"), false, kEnvMax);
            replace = prev.net == kNetOk && normal_text(prev.body) == normal_text(mine);
        }
        if (dir.empty()) {
            replace = false;
        } else if (replace) {
            const std::wstring tmp = env + L".tmp";
            if (write_file(tmp, env_new) && MoveFileExW(tmp.c_str(), env.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                env_note = " O hookshot_env.txt também foi atualizado.";
            else
                DeleteFileW(tmp.c_str()), env_note = " O hookshot_env.txt novo não pôde ser gravado; o seu ficou como estava.";
        } else if (have && normal_text(mine) != normal_text(env_new)) {
            if (write_file(env + L".novo", env_new))
                env_note = fmt(" Seu hookshot_env.txt foi mantido; o da %s ficou ao lado, como hookshot_env.txt.novo.", v.c_str());
        }
    }
    std::lock_guard<std::mutex> lock(mu);
    st.phase = kInstalled, st.installed = tag, st.percent = 100;
    st.detail = fmt("Reinicie o jogo para usar a %s. A versão anterior ficou guardada como hookshot_vk.dll.old e é apagada no próximo "
                    "início.", v.c_str()) + env_note;
}

void run(bool install) {
    if (busy.exchange(true)) return;
    std::thread([install] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        if (install) install_now();
        else check_now();
        busy.store(false);
    }).detach();
}

} // namespace

void begin() {
    if (begun.exchange(true)) return;
    const std::wstring self = own_path();
    if (!self.empty()) {   // (the last update's leftovers: the DLL it replaced, a download cut short)
        DeleteFileW((self + L".old").c_str());
        DeleteFileW((self + L".new").c_str());
    }
    if (env_set("OCARINA_NO_UPDATE_CHECK")) {
        set_phase(kOff, "OCARINA_NO_UPDATE_CHECK=1: a atualização pelo menu está desligada.");
        logf("ocarina: OCARINA_NO_UPDATE_CHECK=1: no update check");
        return;
    }
    if (env_set("HOOKSHOT_NO_UPDATE_CHECK")) {   // (the launchers set it for their own check: the button still asks)
        set_phase(kIdle, kHelpIdle);
        return;
    }
    run(false);
}

void press() {
    Phase p;
    {
        std::lock_guard<std::mutex> lock(mu);
        p = st.phase;
    }
    if (busy.load() || p == kOff || p == kChecking || p == kDownloading || p == kInstalled) return;
    run(p == kAvailable);
}

#else   // (not Windows: the rows say so)
void begin() {}
void press() {}
#endif

View view() {
    State s;
    {
        std::lock_guard<std::mutex> lock(mu);
        s = st;
    }
    View v;
    v.phase = s.phase, v.detail = s.detail;
#ifndef _WIN32
    v.detail = fmt("A atualização pelo menu é só no Windows. Baixe a versão nova pela página %s.", kReleasesPage);
#endif
    const std::string t = shown_tag(s.tag);
    switch (s.phase) {
    case kOff: v.status = kOurs, v.button = "Desligado"; break;
    case kIdle: v.status = kOurs, v.button = "Procurar atualização", v.can_press = true; break;
    case kChecking: v.status = fmt("%s - procurando...", kOurs), v.button = "Procurando..."; break;
    case kUpToDate: v.status = fmt("%s - em dia", kOurs), v.button = "Procurar atualização", v.can_press = true; break;
    case kAvailable: v.status = fmt("Nova versão %s disponível", t.c_str()), v.button = "Atualizar", v.can_press = true; break;
    case kDownloading:
        v.status = fmt("Baixando a %s... %d%%", t.c_str(), s.percent), v.button = fmt("Baixando... %d%%", s.percent);
        break;
    case kInstalled: v.status = fmt("Reinicie o jogo para usar a %s", shown_tag(s.installed).c_str()), v.button = "Reinicie o jogo"; break;
    case kError: v.status = fmt("%s - não deu certo", kOurs), v.button = "Procurar atualização", v.can_press = true; break;
    }
    return v;
}

} // namespace mkwvk::ocarina_update
