// (ocarina) Sending the problem reports: ocarina_upload.h. One thread, started at the first start() with a receiver set, sends
// the pending reports one at a time, oldest first, once allow() said the notice was shown. ocarina-envio.txt beside the game is
// its book: the installation's number, the reports waiting to go, the ones that went (never again)
#include "ocarina_upload.h"
#include "ocarina_brand.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <chrono>
#include <condition_variable>
#include <random>
#include <thread>
#endif

namespace mkwvk {
void logf(const char* fmt, ...);   // (vk_main.cpp: the game's log)
}

namespace mkwvk::ocarina_upload {
namespace {

bool env_zero(const char* k) {
    const char* e = getenv(k);
    return e && *e == '0' && atoi(e) == 0;
}

std::mutex mu;   // (the book, the fates, the worker's flags)
std::map<std::string, Fate> fates;

#ifdef _WIN32
std::string fmt(const char* f, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(b, sizeof b, f, ap);
    va_end(ap);
    return b;
}
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

// ---- the book: ocarina-envio.txt ----
struct Book {
    std::string id;                                   // (instalacao=: 16 hex digits, random, made at the first send)
    std::vector<std::pair<std::string, bool>> pending;   // (pendente=auto|manual <name>: oldest first)
    std::vector<std::string> sent;                    // (enviado=<name>: the newest kMaxSent)
};
constexpr size_t kMaxSent = 200;
std::wstring dir;
bool (*menu_ok)() = nullptr;
bool started = false, allowed = false, blocked = false;   // (blocked: a send failed; the next queue() tries again)
std::condition_variable cv;
Book book;

std::wstring book_path() { return dir + L"\\ocarina-envio.txt"; }
void book_load() {
    book = Book{};
    FILE* f = _wfopen(book_path().c_str(), L"rb");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.size() >= 3 && (unsigned char)s[0] == 0xEF) s.erase(0, 3);
        const size_t eq = s.find('=');
        if (s.empty() || s[0] == '#' || eq == std::string::npos) continue;
        const std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (k == "instalacao" && v.size() == 16 && v.find_first_not_of("0123456789abcdef") == std::string::npos) book.id = v;
        else if (k == "pendente" && v.find(' ') != std::string::npos) {
            const std::string name = v.substr(v.find(' ') + 1);
            if (name.find_first_of("\\/:") == std::string::npos) book.pending.push_back({name, v.compare(0, 5, "auto ") == 0});
        } else if (k == "enviado" && !v.empty()) book.sent.push_back(v);
    }
    fclose(f);
}
void book_save() {
    std::string t = "# Ocarina Engine: os relatórios de erro enviados e os que esperam a internet (Insert ou F1 > Gameplay > Enviar "
                    "relatórios automaticamente desliga o envio). instalacao é um número sorteado, sem nada do seu PC.\n";
    if (!book.id.empty()) t += "instalacao=" + book.id + "\n";
    for (const auto& [name, automatic] : book.pending) t += std::string("pendente=") + (automatic ? "auto " : "manual ") + name + "\n";
    for (size_t i = book.sent.size() > kMaxSent ? book.sent.size() - kMaxSent : 0; i < book.sent.size(); i++) t += "enviado=" + book.sent[i] + "\n";
    const std::wstring p = book_path(), tmp = p + L".tmp";
    if (FILE* f = _wfopen(tmp.c_str(), L"wb")) {
        const bool ok = fwrite(t.data(), 1, t.size(), f) == t.size();
        if (fclose(f) == 0 && ok) MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
        else DeleteFileW(tmp.c_str());
    }
}
bool was_sent(const std::string& name) { return std::find(book.sent.begin(), book.sent.end(), name) != book.sent.end(); }
std::string new_id() {
    uint64_t v = 0;
    typedef BOOLEAN(WINAPI * GenRandom)(PVOID, ULONG);   // (RtlGenRandom, advapi32's SystemFunction036)
    if (HMODULE a = LoadLibraryW(L"advapi32.dll"))
        if (const auto g = (GenRandom)(void*)GetProcAddress(a, "SystemFunction036")) g(&v, sizeof v);
    if (!v) {
        std::random_device rd;
        v = ((uint64_t)rd() << 32 | rd()) ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    }
    return fmt("%016llx", (unsigned long long)v);
}

// ---- a report too big for the receiver: the same ZIP without its largest parts ----
uint16_t le16(const unsigned char* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t le32(const unsigned char* p) { return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16; }
struct Entry {
    std::string name;
    uint64_t off, size;   // (its data: ocarina_report.cpp writes them "stored", so size is the bytes as they are)
};
bool read_at(HANDLE h, uint64_t at, void* to, uint64_t n) {
    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)at;
    if (!SetFilePointerEx(h, pos, nullptr, FILE_BEGIN)) return false;
    uint64_t have = 0;
    while (have < n) {
        DWORD got = 0;
        if (!ReadFile(h, (char*)to + have, (DWORD)std::min<uint64_t>(n - have, 1u << 24), &got, nullptr) || !got) return false;
        have += got;
    }
    return true;
}
/// The entries of a ZIP of ocarina_report.cpp's (local headers one after another, stored, sizes in the header)
bool zip_entries(HANDLE h, std::vector<Entry>* out) {
    uint64_t at = 0;
    for (;;) {
        unsigned char hd[30];
        if (!read_at(h, at, hd, 30) || le32(hd) != 0x04034b50) break;
        const uint16_t flags = le16(hd + 6), method = le16(hd + 8), nlen = le16(hd + 26), xlen = le16(hd + 28);
        const uint32_t csize = le32(hd + 18), usize = le32(hd + 22);
        if (method != 0 || (flags & 8) || csize != usize) return false;
        std::string name(nlen, '\0');
        if (nlen && !read_at(h, at + 30, name.data(), nlen)) return false;
        out->push_back({name, at + 30 + nlen + xlen, csize});
        at += 30ull + nlen + xlen + csize;
    }
    return !out->empty();
}
uint32_t crc32_of(const char* d, size_t n, uint32_t c = 0) {
    static uint32_t table[256];
    static std::once_flag once;
    std::call_once(once, [] {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t x = i;
            for (int k = 0; k < 8; k++) x = x & 1 ? 0xEDB88320u ^ (x >> 1) : x >> 1;
            table[i] = x;
        }
    });
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ (unsigned char)d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
/// A stored ZIP in memory (as ocarina_report.cpp Zip writes to its file)
struct MemZip {
    std::string out, cd;
    uint16_t n = 0, dtime = 0, ddate = 0;
    MemZip() {
        SYSTEMTIME t;
        GetLocalTime(&t);
        dtime = (uint16_t)(t.wHour << 11 | t.wMinute << 5 | t.wSecond / 2);
        ddate = (uint16_t)((t.wYear - 1980) << 9 | t.wMonth << 5 | t.wDay);
    }
    static void u16(std::string& b, uint16_t v) { b += (char)(v & 0xFF), b += (char)(v >> 8); }
    static void u32(std::string& b, uint32_t v) { u16(b, (uint16_t)(v & 0xFFFF)), u16(b, (uint16_t)(v >> 16)); }
    void add(const std::string& name, const std::string& data) {
        const uint32_t crc = crc32_of(data.data(), data.size()), off = (uint32_t)out.size(), size = (uint32_t)data.size();
        u32(out, 0x04034b50), u16(out, 20), u16(out, 0x0800), u16(out, 0), u16(out, dtime), u16(out, ddate);
        u32(out, crc), u32(out, size), u32(out, size), u16(out, (uint16_t)name.size()), u16(out, 0);
        out += name, out += data;
        u32(cd, 0x02014b50), u16(cd, 20), u16(cd, 20), u16(cd, 0x0800), u16(cd, 0), u16(cd, dtime), u16(cd, ddate);
        u32(cd, crc), u32(cd, size), u32(cd, size), u16(cd, (uint16_t)name.size()), u16(cd, 0), u16(cd, 0), u16(cd, 0), u16(cd, 0);
        u32(cd, 0), u32(cd, off);
        cd += name;
        n++;
    }
    std::string finish() {
        const uint32_t at = (uint32_t)out.size();
        out += cd;
        u32(out, 0x06054b50), u16(out, 0), u16(out, 0), u16(out, n), u16(out, n), u32(out, (uint32_t)cd.size()), u32(out, at), u16(out, 0);
        return std::move(out);
    }
};
bool ends_with(const std::string& s, const char* e) { return s.size() >= strlen(e) && !s.compare(s.size() - strlen(e), strlen(e), e); }
/// The order the parts are left out in, while the report is over kMaxBytes: the marks' pictures (oldest first), the frame's
/// picture, the older logs, then the rest of the marks and the series; what is never left out: 99
int drop_rank(const std::string& n, bool automatic) {
    if (!n.compare(0, 7, "marcas/") && ends_with(n, ".png")) return 0;
    if (n == "quadro.png") return 1;
    if (n == "hookshot_log.4.txt") return 2;
    if (n == "hookshot_log.3.txt") return 3;
    if (n == "hookshot_log.2.txt") return 4;
    if (n == "travadas_sessao_anterior.csv") return 5;
    if (!n.compare(0, 7, "marcas/")) return 6;
    if (n == "brilho.csv") return 7;
    if (n == (automatic ? "hookshot_log.txt" : "hookshot_log.1.txt")) return 8;   // (the log that is not the session's)
    if (n == "quadros.csv") return 9;
    return 99;
}
/// body: the report at path without its largest parts, and a note (envio.txt) of what was left out; false when it cannot be read
bool slim(const std::wstring& path, bool automatic, std::string* body, std::string* left_out) {
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::vector<Entry> all;
    if (!zip_entries(h, &all)) {
        CloseHandle(h);
        return false;
    }
    const auto cost = [](const Entry& e) { return e.size + 76 + 2 * e.name.size(); };   // (data, local header, central entry)
    uint64_t total = 22 + 4096;   // (the end record, envio.txt)
    for (const Entry& e : all) total += cost(e);
    std::vector<size_t> order(all.size());
    for (size_t i = 0; i < all.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return drop_rank(all[a].name, automatic) < drop_rank(all[b].name, automatic); });
    std::vector<bool> drop(all.size(), false);
    std::string note = "Este relatório passou de " + fmt("%llu", (unsigned long long)(kMaxBytes >> 20)) +
                       " MB e foi enviado sem as partes abaixo (elas continuam no .zip, na pasta do jogo de quem testou):\r\n";
    for (size_t i : order) {
        if (total <= kMaxBytes || drop_rank(all[i].name, automatic) == 99) break;
        drop[i] = true, total -= cost(all[i]);
        note += fmt("- %s (%llu KB)\r\n", all[i].name.c_str(), (unsigned long long)(all[i].size >> 10));
    }
    // (still too big: the largest part left keeps its end, as much as fits; a log's newest lines are its end)
    size_t cut = all.size();
    uint64_t keep = 0;
    if (total > kMaxBytes) {
        for (size_t i = 0; i < all.size(); i++)
            if (!drop[i] && (cut == all.size() || all[i].size > all[cut].size)) cut = i;
        const uint64_t over = total - kMaxBytes + (64u << 10);
        if (cut == all.size() || all[cut].size <= over) {
            CloseHandle(h);
            return false;
        }
        keep = all[cut].size - over;
        note += fmt("- o começo de %s: foram só os últimos %llu KB de %llu KB\r\n", all[cut].name.c_str(), (unsigned long long)(keep >> 10),
                    (unsigned long long)(all[cut].size >> 10));
    }
    MemZip z;
    z.out.reserve((size_t)std::min<uint64_t>(total + (1u << 16), kMaxBytes + (1u << 20)));
    bool ok = true;
    for (size_t i = 0; i < all.size() && ok; i++) {
        if (drop[i]) continue;
        const uint64_t from = i == cut ? all[i].off + all[i].size - keep : all[i].off, n = i == cut ? keep : all[i].size;
        std::string data((size_t)n, '\0');
        ok = !n || read_at(h, from, data.data(), n);
        if (i == cut) data = "[... o começo deste arquivo ficou de fora do envio ...]\r\n" + data;
        z.add(all[i].name, data);
    }
    CloseHandle(h);
    if (!ok) return false;
    z.add("envio.txt", note);
    *body = z.finish();
    *left_out = note;
    return true;
}

// ---- HTTP: one POST ----
struct Answer {
    bool reached = false;   // (the receiver answered)
    DWORD status = 0, error = 0;
};
bool local_host(const std::wstring& h) {
    return !_wcsicmp(h.c_str(), L"localhost") || h == L"127.0.0.1" || h == L"[::1]" || h == L"::1";
}
Answer post(const std::string& to, const std::string& body, const std::wstring& headers) {
    Answer a;
    const std::wstring url = widen(to);
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof uc;
    wchar_t host[256] = {}, path[2048] = {}, extra[2048] = {};
    uc.lpszHostName = host, uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path, uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra, uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return a;
    const std::wstring hostname(host, uc.dwHostNameLength);
    const bool tls = uc.nScheme == INTERNET_SCHEME_HTTPS;
    if (!tls && !(uc.nScheme == INTERNET_SCHEME_HTTP && local_host(hostname))) return a;   // (http: only to this PC, a test)
    const std::wstring agent = L"OcarinaEngine/" + widen(OCARINA_VERSION);
    HINTERNET s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return a;
    WinHttpSetTimeouts(s, 5000, 5000, 15000, 20000);   // (name, connection, each piece sent, the answer)
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;   // (a POST is not followed anywhere)
    WinHttpSetOption(s, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy);
    if (tls) {
        DWORD p = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000;   // (| TLS 1.3 where known)
        if (!WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &p, sizeof p)) {
            p = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &p, sizeof p);
        }
    }
    std::wstring object(path, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) object.append(extra, uc.dwExtraInfoLength);
    HINTERNET c = WinHttpConnect(s, hostname.c_str(), uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"POST", object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         tls ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    bool sent = r && WinHttpSendRequest(r, headers.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, (DWORD)body.size(), 0);
    for (size_t at = 0; sent && at < body.size();) {
        DWORD put = 0;
        sent = WinHttpWriteData(r, body.data() + at, (DWORD)std::min<size_t>(body.size() - at, 64u << 10), &put) && put;
        at += put;
    }
    if (sent && WinHttpReceiveResponse(r, nullptr)) {
        DWORD size = sizeof a.status;
        a.reached = WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &a.status,
                                        &size, WINHTTP_NO_HEADER_INDEX) != FALSE;
        char buf[4096];
        DWORD got = 0;
        for (int k = 0; k < 16 && WinHttpReadData(r, buf, sizeof buf, &got) && got; k++) {}   // (its short answer, not needed)
    } else {
        a.error = GetLastError();
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return a;
}

// ---- one report ----
enum class Out { kOk, kRetry, kRefused, kGone };
Out send_one(const std::string& name, bool automatic, const std::string& id, std::string* said) {
    const std::wstring path = dir + L"\\" + widen(name);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa)) return Out::kGone;
    const uint64_t size = (uint64_t)fa.nFileSizeHigh << 32 | fa.nFileSizeLow;
    std::string body, left_out;
    bool slimmed = false;
    if (size <= kMaxBytes) {
        const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return Out::kGone;
        body.resize((size_t)size);
        const bool ok = !size || read_at(h, 0, body.data(), size);
        CloseHandle(h);
        if (!ok) return Out::kRetry;
    } else if (slim(path, automatic, &body, &left_out)) {
        slimmed = true;
    } else {
        *said = fmt("passou de %llu MB e não deu para reduzir", (unsigned long long)(kMaxBytes >> 20));
        return Out::kRefused;
    }
    std::wstring headers = L"Content-Type: application/zip\r\nX-Ocarina-Versao: " + widen(OCARINA_VERSION) +
                           L"\r\nX-Ocarina-Tipo: " + (automatic ? L"automatico" : L"manual") + L"\r\nX-Ocarina-Instalacao: " +
                           widen(id) + L"\r\nX-Ocarina-Arquivo: " + widen(name) + L"\r\n";
    if (slimmed) headers += L"X-Ocarina-Reduzido: 1\r\n";
    const Answer a = post(url(), body, headers);
    *said = a.reached ? fmt("resposta %lu, %llu KB%s", (unsigned long)a.status, (unsigned long long)(body.size() >> 10),
                            slimmed ? ", sem as partes maiores" : "")
                      : a.error ? fmt("sem resposta (erro %lu)", (unsigned long)a.error)
                                : std::string("o endereço do receptor não serve (precisa ser https; http só para este PC)");
    if (!a.reached) return Out::kRetry;
    if (a.status >= 200 && a.status < 300) return Out::kOk;
    if (a.status == 408 || a.status == 429 || a.status >= 500) return Out::kRetry;
    return Out::kRefused;
}

void worker() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;) {
        std::string name, id;
        bool automatic = false;
        {
            std::unique_lock lk(mu);
            cv.wait(lk, [] { return allowed && !blocked && !book.pending.empty(); });
            name = book.pending.front().first, automatic = book.pending.front().second;
            if (was_sent(name)) {   // (sent by another start meanwhile)
                book.pending.erase(book.pending.begin());
                book_save();
                continue;
            }
            if (!(menu_ok && menu_ok()) || !env_allows()) {   // (turned off since: it waits for a start with sending on)
                blocked = true;
                continue;
            }
            if (book.id.empty()) book.id = new_id(), book_save();
            id = book.id;
            fates[name] = Fate::kSending;
        }
        std::string said;
        const Out out = send_one(name, automatic, id, &said);
        std::lock_guard lk(mu);
        if (out != Out::kRetry) {
            book.pending.erase(std::remove_if(book.pending.begin(), book.pending.end(), [&](const auto& p) { return p.first == name; }),
                               book.pending.end());
            if (out == Out::kOk) book.sent.push_back(name);
            book_save();
        }
        switch (out) {
            case Out::kOk:
                fates[name] = Fate::kSent;
                logf("ocarina: relatório enviado: %s (%s)", name.c_str(), said.c_str());
                break;
            case Out::kRetry:
                fates[name] = Fate::kRetry, blocked = true;
                logf("ocarina: o envio do relatório %s falhou (%s): fica para o próximo início", name.c_str(), said.c_str());
                break;
            case Out::kRefused:
                fates[name] = Fate::kRefused;
                logf("ocarina: o receptor recusou o relatório %s (%s): não será enviado de novo", name.c_str(), said.c_str());
                break;
            case Out::kGone:
                fates.erase(name);
                logf("ocarina: o relatório %s não está mais na pasta do jogo: saiu da fila de envio", name.c_str());
                break;
        }
    }
}
#endif   // _WIN32

} // namespace

std::string url() {
    if (const char* e = getenv("OCARINA_REPORT_URL"); e && *e) return e;
    const std::string u = kReportUrl;
    return u.find('<') == std::string::npos ? u : "";   // (the placeholder, until the receiver is deployed)
}

bool env_allows() { return !env_zero("OCARINA_REPORT_UPLOAD") && !env_zero("RX_VK_REPORT_UPLOAD") && !url().empty(); }

Fate fate(const std::string& zip_name) {
    std::lock_guard lk(mu);
    const auto it = fates.find(zip_name);
    return it == fates.end() ? Fate::kNone : it->second;
}

#ifdef _WIN32
void start(const std::wstring& game_dir, bool (*menu_allows)()) {
    std::lock_guard lk(mu);
    if (started || game_dir.empty() || !env_allows()) return;
    started = true, dir = game_dir, menu_ok = menu_allows;
    book_load();
    while ((int)book.pending.size() > kMaxPending) book.pending.erase(book.pending.begin());
    for (const auto& p : book.pending) fates[p.first] = Fate::kWaiting;
    if (!book.pending.empty())
        logf("ocarina: %zu relatório(s) esperando envio de um início anterior; vão para %s", book.pending.size(), url().c_str());
    std::thread(worker).detach();
}

void allow() {
    std::lock_guard lk(mu);
    if (!allowed) allowed = true, cv.notify_all();
}

void queue(const std::wstring& zip_name, bool automatic) {
    const std::string name = utf8(zip_name);
    std::lock_guard lk(mu);
    if (!started || name.empty() || was_sent(name)) return;
    book.pending.erase(std::remove_if(book.pending.begin(), book.pending.end(), [&](const auto& p) { return p.first == name; }),
                       book.pending.end());
    book.pending.push_back({name, automatic});
    while ((int)book.pending.size() > kMaxPending) {
        logf("ocarina: fila de envio cheia: %s fica só na pasta do jogo", book.pending.front().first.c_str());
        fates.erase(book.pending.front().first);
        book.pending.erase(book.pending.begin());
    }
    book_save();
    fates[name] = Fate::kWaiting;
    blocked = false;   // (a new report: worth trying again)
    cv.notify_all();
}
#endif

} // namespace mkwvk::ocarina_upload
