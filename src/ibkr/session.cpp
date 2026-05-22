#include "ibkr/session.hpp"

#include <Contract.h>
#include <Decimal.h>
#include <DefaultEWrapper.h>
#include <EClientSocket.h>
#include <EReader.h>
#include <EReaderOSSignal.h>
#include <bar.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "market/types.hpp"
#include "util/time.hpp"

namespace at::ibkr {

namespace {

constexpr int kHistReqIdBase = 10'000;

bool isInformationalError(int code) {
    return code >= 2100 && code <= 2199;
}

bool isFatalError(int code) {
    if (code <= 0) {
        return false;
    }
    if (isInformationalError(code)) {
        return false;
    }
    if (code == 162) {
        return false;
    }
    switch (code) {
        case 502:
        case 503:
        case 504:
        case 1100:
        case 326:
            return true;
        default:
            return code < 1100;
    }
}

// TWS bar.time with formatDate=1: "YYYYMMDD  HH:MM:SS" or epoch string (formatDate=2).
std::optional<at::time::Timestamp> parseTwsBarTime(const std::string& time_str) {
    if (time_str.empty()) {
        return std::nullopt;
    }

    bool all_digit = true;
    for (const char ch : time_str) {
        if (ch < '0' || ch > '9') {
            all_digit = false;
            break;
        }
    }
    if (all_digit) {
        try {
            const auto sec = std::stoll(time_str);
            return at::time::fromEpochSeconds(sec);
        } catch (...) {
            return std::nullopt;
        }
    }

    int date = 0;
    int hour = 0;
    int minute = 0;
    int sec = 0;
    if (std::sscanf(time_str.c_str(), "%8d %d:%d:%d", &date, &hour, &minute, &sec) != 4) {
        return std::nullopt;
    }
    const int year = date / 10'000;
    const int month = (date / 100) % 100;
    const int day = date % 100;

    struct tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = sec;
    t.tm_isdst = 0;

    const time_t epoch_sec = timegm(&t);
    if (epoch_sec == static_cast<time_t>(-1)) {
        return std::nullopt;
    }
    return at::time::fromEpochSeconds(static_cast<std::int64_t>(epoch_sec));
}

double decimalToDouble(Decimal d) {
    if (d == UNSET_DECIMAL) {
        return 0.0;
    }
    return DecimalFunctions::decimalToDouble(d);
}

at::market::Bar mapTwsBar(const Bar& bar) {
    const auto ts = parseTwsBarTime(bar.time);
    if (!ts) {
        throw std::runtime_error("ibkr::Session: failed to parse bar time: " + bar.time);
    }
    return at::market::Bar{
        .timestamp = *ts,
        .open      = bar.open,
        .high      = bar.high,
        .low       = bar.low,
        .close     = bar.close,
        .volume    = decimalToDouble(bar.volume),
    };
}

}  // namespace

struct Session::Impl : DefaultEWrapper {
    explicit Impl(SessionConfig cfg)
        : config(std::move(cfg)), client(this, &os_signal) {}

    SessionConfig config;
    EReaderOSSignal os_signal{2000};
    EClientSocket client;
    std::unique_ptr<EReader> reader;

    std::mutex mtx;
    std::condition_variable cv;
    bool api_ready = false;
    bool time_done = false;
    std::int64_t server_time = 0;
    std::optional<std::string> fatal_error;

    int next_req_id = kHistReqIdBase;
    int active_hist_req_id = -1;
    bool hist_done = false;
    bool pacing_violation = false;
    std::vector<at::market::Bar> hist_bars;

    int allocateHistReqId() {
        std::lock_guard lock(mtx);
        return next_req_id++;
    }

    void resetHistState(int req_id) {
        std::lock_guard lock(mtx);
        active_hist_req_id = req_id;
        hist_done = false;
        pacing_violation = false;
        hist_bars.clear();
    }

    void connectAck() override {
        if (client.asyncEConnect()) {
            client.startApi();
        }
    }

    void nextValidId(int /*order_id*/) override {
        std::lock_guard lock(mtx);
        api_ready = true;
        cv.notify_all();
    }

    void currentTime(long long time) override {
        std::lock_guard lock(mtx);
        server_time = static_cast<std::int64_t>(time);
        time_done = true;
        cv.notify_all();
    }

    void historicalData(int reqId, const Bar& bar) override {
        std::lock_guard lock(mtx);
        if (reqId != active_hist_req_id) {
            return;
        }
        if (bar.time.empty()) {
            return;
        }
        try {
            hist_bars.push_back(mapTwsBar(bar));
        } catch (const std::exception& e) {
            spdlog::warn("ibkr::Session: skipping bar: {}", e.what());
        }
    }

    void historicalDataEnd(int reqId, const std::string& /*startDateStr*/,
                           const std::string& /*endDateStr*/) override {
        std::lock_guard lock(mtx);
        if (reqId != active_hist_req_id) {
            return;
        }
        hist_done = true;
        cv.notify_all();
    }

    void error(int id, time_t /*error_time*/, int error_code, const std::string& error_string,
               const std::string& /*advanced_order_reject_json*/) override {
        if (isInformationalError(error_code)) {
            spdlog::debug("ibkr::Session info [{}] id={}: {}", error_code, id, error_string);
            return;
        }

        if (error_code == 162) {
            spdlog::warn("ibkr::Session pacing [{}] id={}: {}", error_code, id, error_string);
            std::lock_guard lock(mtx);
            if (id == -1 || id == active_hist_req_id) {
                pacing_violation = true;
                cv.notify_all();
            }
            return;
        }

        spdlog::warn("ibkr::Session error [{}] id={}: {}", error_code, id, error_string);
        if (isFatalError(error_code)) {
            std::lock_guard lock(mtx);
            fatal_error = error_string.empty() ? ("IBKR error " + std::to_string(error_code))
                                             : error_string;
            cv.notify_all();
        }
    }

    void pumpUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (reader) {
                reader->processMsgs();
            }
            {
                std::lock_guard lock(mtx);
                if (fatal_error) {
                    throw std::runtime_error(*fatal_error);
                }
                if (pacing_violation) {
                    throw PacingViolationError(
                        "IBKR historical data pacing violation (error 162) — retry after backoff");
                }
            }
            if (done()) {
                return;
            }
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds(0)) {
                break;
            }
            os_signal.waitForSignal();
        }
        throw std::runtime_error(
            "ibkr::Session: timed out waiting for TWS response — is TWS running with API "
            "enabled? If TWS shows an incoming connection dialog, click Accept.");
    }
};

Session::Session(SessionConfig config) : impl_(new Impl(std::move(config))) {}

Session::~Session() {
    try {
        disconnect();
    } catch (...) {
    }
    delete impl_;
}

void Session::connect() {
    impl_->client.setConnectOptions("+PACEAPI");

    spdlog::info("ibkr::Session: connecting to {}:{} clientId={}", impl_->config.host,
                 impl_->config.port, impl_->config.client_id);

    if (!impl_->client.eConnect(impl_->config.host.c_str(), impl_->config.port,
                                impl_->config.client_id, false)) {
        throw std::runtime_error("ibkr::Session: eConnect failed — is TWS/Gateway running with "
                                 "API enabled on port " +
                                 std::to_string(impl_->config.port) + "?");
    }

    impl_->reader = std::make_unique<EReader>(&impl_->client, &impl_->os_signal);
    impl_->reader->start();

    spdlog::debug(
        "ibkr::Session: waiting for nextValidId (accept API connection in TWS if prompted)...");
    impl_->pumpUntil([this] {
        std::lock_guard lock(impl_->mtx);
        return impl_->api_ready;
    }, impl_->config.timeout);

    spdlog::info("ibkr::Session: connected (serverVersion={})",
                 impl_->client.EClient::serverVersion());
}

std::int64_t Session::reqCurrentTime() {
    if (!impl_->client.isConnected()) {
        throw std::runtime_error("ibkr::Session::reqCurrentTime(): not connected");
    }

    {
        std::lock_guard lock(impl_->mtx);
        impl_->time_done = false;
        impl_->server_time = 0;
        impl_->pacing_violation = false;
    }

    impl_->client.reqCurrentTime();

    impl_->pumpUntil([this] {
        std::lock_guard lock(impl_->mtx);
        return impl_->time_done;
    }, impl_->config.timeout);

    std::lock_guard lock(impl_->mtx);
    return impl_->server_time;
}

std::vector<at::market::Bar> Session::reqHistoricalBars(const Contract& contract,
                                                        const HistoricalRequest& request) {
    if (!impl_->client.isConnected()) {
        throw std::runtime_error("ibkr::Session::reqHistoricalBars(): not connected");
    }

    const int req_id = impl_->allocateHistReqId();
    impl_->resetHistState(req_id);

    spdlog::info(
        "ibkr::Session: reqHistoricalData id={} {} {} end='{}' duration='{}' bar='{}'",
        req_id, contract.symbol, contract.secType,
        request.end_date_time.empty() ? "(now)" : request.end_date_time, request.duration_str,
        request.bar_size_setting);

    impl_->client.reqHistoricalData(
        req_id, contract, request.end_date_time, request.duration_str, request.bar_size_setting,
        request.what_to_show, request.use_rth, request.format_date, false, TagValueListSPtr{});

    impl_->pumpUntil([this] {
        std::lock_guard lock(impl_->mtx);
        return impl_->hist_done;
    }, request.timeout);

    std::lock_guard lock(impl_->mtx);
    spdlog::info("ibkr::Session: received {} bars (reqId={})", impl_->hist_bars.size(), req_id);
    return std::move(impl_->hist_bars);
}

void Session::disconnect() {
    if (impl_->reader) {
        impl_->reader.reset();
    }
    if (impl_->client.isConnected()) {
        impl_->client.eDisconnect();
        spdlog::debug("ibkr::Session: disconnected");
    }
}

bool Session::isConnected() const {
    return impl_->client.isConnected();
}

}  // namespace at::ibkr
