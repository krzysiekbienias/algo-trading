#include "xtb/client.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace at::xtb {

struct Client::Impl {
    ClientConfig config;

    ix::WebSocket ws;

    // ── connect() synchronization ─────────────────────────────────────────
    // The IXWebSocket callback thread signals cv_conn when the socket either
    // opens successfully or fails. connect() waits here instead of spinning.
    std::mutex              mtx;
    std::condition_variable cv_conn;
    bool        conn_done  = false;   // set on Open or Error
    std::string conn_error;           // non-empty → connect failed
    bool        connected  = false;

    // ── call() synchronization ────────────────────────────────────────────
    // Reused for every request/response round-trip. Guarded by the same mtx.
    std::condition_variable cv_resp;
    nlohmann::json          response;
    bool                    resp_ready = false;

    std::string stream_session_id;

    // Timestamp of the last outgoing request — used to enforce the throttle.
    std::chrono::steady_clock::time_point last_call_time{};
};

Client::Client(ClientConfig config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
}

Client::~Client() {
    disconnect();
}

Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

void Client::connect() {
    impl_->ws.setUrl(impl_->config.endpoint);
    impl_->ws.disableAutomaticReconnection();
    impl_->ws.setHandshakeTimeout(
        static_cast<int>(impl_->config.request_timeout.count() / 1000));

    // Reset state from any previous attempt.
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->conn_done  = false;
        impl_->conn_error.clear();
        impl_->connected  = false;
    }

    // The callback runs on IXWebSocket's internal thread. We capture the raw
    // Impl* so the lambda stays valid even if the Client is move-constructed
    // after connect() returns (ws lives inside Impl, so impl is always alive
    // as long as the callback can fire).
    Impl* impl = impl_.get();
    impl_->ws.setOnMessageCallback([impl](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            spdlog::debug("xtb::Client: socket opened");
            std::lock_guard<std::mutex> lock(impl->mtx);
            impl->connected = true;
            impl->conn_done = true;
            impl->cv_conn.notify_one();

        } else if (msg->type == ix::WebSocketMessageType::Error) {
            spdlog::warn("xtb::Client: socket error: {}", msg->errorInfo.reason);
            std::lock_guard<std::mutex> lock(impl->mtx);
            impl->conn_error = msg->errorInfo.reason;
            impl->conn_done  = true;
            impl->cv_conn.notify_one();

        } else if (msg->type == ix::WebSocketMessageType::Close) {
            spdlog::debug("xtb::Client: socket closed");
            std::lock_guard<std::mutex> lock(impl->mtx);
            impl->connected = false;

        } else if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto json = nlohmann::json::parse(msg->str);
                std::lock_guard<std::mutex> lock(impl->mtx);
                impl->response   = std::move(json);
                impl->resp_ready = true;
                impl->cv_resp.notify_one();
            } catch (const nlohmann::json::parse_error& e) {
                spdlog::warn("xtb::Client: JSON parse error: {}", e.what());
            }
        }
    });

    impl_->ws.start();

    // Block until the socket is open or an error is reported.
    std::unique_lock<std::mutex> lock(impl_->mtx);
    if (!impl_->cv_conn.wait_for(lock, impl_->config.request_timeout,
                                 [&] { return impl_->conn_done; })) {
        impl_->ws.stop();
        throw std::runtime_error(
            "xtb::Client::connect() timed out connecting to " +
            impl_->config.endpoint);
    }
    if (!impl_->connected) {
        impl_->ws.stop();
        throw std::runtime_error(
            "xtb::Client::connect() failed: " + impl_->conn_error);
    }

    spdlog::info("xtb::Client: connected to {}", impl_->config.endpoint);
}

void Client::login(const Credentials& creds) {
    if (!impl_->connected) {
        throw std::runtime_error("xtb::Client::login(): not connected");
    }

    // XTB login response has a unique shape — "streamSessionId" sits at the
    // top level of the frame, NOT inside "returnData" like every other command.
    // We therefore do our own send/wait instead of delegating to call().
    nlohmann::json req = {
        {"command", "login"},
        {"arguments", {
            {"userId",   creds.user_id},
            {"password", creds.password},
            {"appName",  creds.app_name},
        }},
    };

    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->resp_ready = false;
    }

    impl_->ws.send(req.dump());
    impl_->last_call_time = std::chrono::steady_clock::now();
    spdlog::debug("xtb::Client → login (userId={})", creds.user_id);

    std::unique_lock<std::mutex> lock(impl_->mtx);
    if (!impl_->cv_resp.wait_for(lock, impl_->config.request_timeout,
                                 [&] { return impl_->resp_ready; })) {
        throw std::runtime_error("xtb::Client::login(): timed out waiting for response");
    }

    auto resp = std::move(impl_->response);
    impl_->resp_ready = false;

    if (!resp.value("status", false)) {
        throw std::runtime_error(
            "xtb::Client::login() failed [" +
            resp.value("errorCode", "?") + "]: " +
            resp.value("errorDescr", "unknown"));
    }

    impl_->stream_session_id = resp.value("streamSessionId", "");
    spdlog::info("xtb::Client: logged in as {}", creds.user_id);
}

void Client::disconnect() noexcept {
    if (!impl_) return;
    if (impl_->connected) {
        impl_->ws.stop();
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->connected = false;
        spdlog::debug("xtb::Client: disconnected");
    }
}

nlohmann::json Client::call(const std::string& command,
                            const nlohmann::json& arguments) {
    if (!impl_->connected) {
        throw std::runtime_error(
            "xtb::Client::call(" + command + "): not connected");
    }

    // Throttle: enforce minimum delay between consecutive requests.
    // XTB rejects bursts faster than ~200 ms; ClientConfig defaults to 250 ms.
    if (impl_->last_call_time.time_since_epoch().count() != 0) {
        auto elapsed = std::chrono::steady_clock::now() - impl_->last_call_time;
        if (elapsed < impl_->config.throttle) {
            std::this_thread::sleep_for(impl_->config.throttle - elapsed);
        }
    }

    // Build the XTB request frame: {"command": "...", "arguments": {...}}
    nlohmann::json req = {{"command", command}};
    if (!arguments.is_null()) {
        req["arguments"] = arguments;
    }

    // Clear any stale response before sending so the cv_resp.wait_for()
    // below picks up only the fresh reply to THIS request.
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->resp_ready = false;
    }

    impl_->ws.send(req.dump());
    impl_->last_call_time = std::chrono::steady_clock::now();
    spdlog::debug("xtb::Client → {}", command);

    // Block until the callback delivers a response frame (or we time out).
    std::unique_lock<std::mutex> lock(impl_->mtx);
    if (!impl_->cv_resp.wait_for(lock, impl_->config.request_timeout,
                                 [&] { return impl_->resp_ready; })) {
        throw std::runtime_error(
            "xtb::Client::call(" + command + "): timed out waiting for response");
    }

    auto resp = std::move(impl_->response);
    impl_->resp_ready = false;

    spdlog::debug("xtb::Client ← {} status={}", command,
                  resp.value("status", false));

    // XTB signals errors with "status": false, "errorCode", "errorDescr".
    if (!resp.value("status", false)) {
        throw std::runtime_error(
            "xtb::Client::call(" + command + ") XTB error [" +
            resp.value("errorCode", "?") + "]: " +
            resp.value("errorDescr", "unknown"));
    }

    // On success XTB puts the payload under "returnData" (may be absent for
    // commands like logout that return no data).
    return resp.value("returnData", nlohmann::json{});
}

bool Client::isConnected() const noexcept {
    return impl_ && impl_->connected;
}

const std::string& Client::streamSessionId() const noexcept {
    return impl_->stream_session_id;
}

}  // namespace at::xtb
