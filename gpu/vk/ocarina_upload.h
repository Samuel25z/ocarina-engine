// (ocarina) Sending the problem reports (ocarina_report.cpp) to the Ocarina Engine's receiver, a Cloudflare Worker
// (C:\Zelda Radar\ocarina-engine\receptor-relatorios): each saved .zip goes on a thread of its own at below-normal priority, by
// WinHTTP, its body the .zip as it is (at most kMaxBytes: a bigger one is sent without its largest parts, the pictures first,
// and a note of what was left out), with the Ocarina Engine's version, the kind (automatic, manual or desempenho: the light
// performance report, ocarina_report.cpp perf), a performance report's short reason, and the installation's anonymous number
// (random, made once, kept in ocarina-envio.txt beside the game: nothing taken from the PC) as headers.
// On by default with three ways out: the Insert menu's Enviar relatórios automaticamente row (opts.report_upload),
// OCARINA_REPORT_UPLOAD=0 or RX_VK_REPORT_UPLOAD=0 (hookshot_env.txt takes the RX_ name), and the first start's notice says how.
// Nothing is sent before that notice was shown. One that fails (no internet, the receiver down or busy) stays pending in
// ocarina-envio.txt and goes at the next start (the newest kMaxPending kept); one sent is written there as sent, never again.
// OCARINA_REPORT_URL=<url> sends to another receiver (https; http only to this PC, for testing against `wrangler dev`)
#pragma once

#include <cstdint>
#include <string>

namespace mkwvk::ocarina_upload {

constexpr uint64_t kMaxBytes = 20ull << 20;   // (the receiver's limit: receptor-relatorios/src/index.ts MAX_BYTES)
constexpr int kMaxPending = 5;
constexpr int kMaxPendingPerf = 2;   // (performance reports waiting: over it the oldest of them leaves the queue, before any other)

/// What a report is (X-Ocarina-Tipo): automatico, manual, desempenho
enum class Kind { kAuto, kManual, kPerf };

/// Where reports go: OCARINA_REPORT_URL, else kReportUrl (ocarina_brand.h); "" while kReportUrl is still the placeholder
std::string url();
/// Not turned off by OCARINA_REPORT_UPLOAD=0 / RX_VK_REPORT_UPLOAD=0, and a receiver is set (the menu's row is the caller's)
bool env_allows();

#ifdef _WIN32
/// Once, at the start: the game folder, and what says whether the menu still allows sending (read before each report goes).
/// What was left pending by an earlier session is sent once allow() is called
void start(const std::wstring& game_dir, bool (*menu_allows)());
/// The notice about sending was shown (now, or at an earlier start): sending may begin
void allow();
/// A report saved in the game folder (its file name): sent in the background. reason: a performance report's limits, ASCII,
/// sent as X-Ocarina-Motivo (kept for this session only: one left for the next start goes without it)
void queue(const std::wstring& zip_name, Kind kind, const std::string& reason = "");
/// Waits up to timeout_ms for that report's sending to end (sent, refused, or left for the next start): true when it was sent
bool wait_sent(const std::string& zip_name, int timeout_ms);
/// Whether ocarina-envio.txt says a performance report of this version was made (the reference one, ocarina_report.cpp)
bool reference_done(const std::string& version);
void reference_mark(const std::string& version);
#endif

/// What became of a report, by its file name (the menu's line under Gerar relatório)
enum class Fate { kNone, kWaiting, kSending, kSent, kRetry, kRefused };
Fate fate(const std::string& zip_name);

} // namespace mkwvk::ocarina_upload
