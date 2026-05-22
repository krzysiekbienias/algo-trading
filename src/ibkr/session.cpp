#include "ibkr/session.hpp"

#include <DefaultEWrapper.h>
#include <EClientSocket.h>
#include <EReader.h>
#include <EReaderOSSignal.h>
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

namespace at::ibkr {

namespace {

bool isInformationalError(int code) {
    // IBKR delivers connectivity / farm status as "errors" in 2100–2199.
    return code >= 2100 && code <= 2199;
}

bool isFatalError(int code) {
    if (code <= 0) {
        return false;
    }
    if (isInformationalError(code)) {
        return false;
    }
    switch (code) {
        case 502:  // couldn't connect to TWS
        case 503:  // TWS version / client version
        case 504:  // not connected
        case 1100: // connectivity lost
        case 326:  // client id already in use
            return true;
        default:
            return code < 1100;
    }
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

    void error(int id, time_t /*error_time*/, int error_code, const std::string& error_string,
               const std::string& /*advanced_order_reject_json*/) override {
        if (isInformationalError(error_code)) {
            spdlog::debug("ibkr::Session info [{}] id={}: {}", error_code, id, error_string);
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

    spdlog::debug("ibkr::Session: waiting for nextValidId (accept API connection in TWS if prompted)...");
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
    }

    impl_->client.reqCurrentTime();

    impl_->pumpUntil([this] {
        std::lock_guard lock(impl_->mtx);
        return impl_->time_done;
    }, impl_->config.timeout);

    std::lock_guard lock(impl_->mtx);
    return impl_->server_time;
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
