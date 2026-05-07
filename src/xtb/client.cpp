#include "xtb/client.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace at::xtb {

// NOTE: This is a stub. Step 2 will replace it with a real IXWebSocket
// implementation (login + synchronous call/response loop + throttle).
// The class compiles and links so the rest of the codebase can develop
// against the public interface.

struct Client::Impl {
    ClientConfig config;
    bool connected = false;
    std::string stream_session_id;
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
    throw std::runtime_error("xtb::Client::connect() not implemented yet (Step 2)");
}

void Client::login(const Credentials&) {
    throw std::runtime_error("xtb::Client::login() not implemented yet (Step 2)");
}

void Client::disconnect() noexcept {
    if (impl_ && impl_->connected) {
        impl_->connected = false;
    }
}

nlohmann::json Client::call(const std::string& command, const nlohmann::json&) {
    throw std::runtime_error("xtb::Client::call(" + command + ") not implemented yet (Step 2)");
}

bool Client::isConnected() const noexcept {
    return impl_ && impl_->connected;
}

const std::string& Client::streamSessionId() const noexcept {
    return impl_->stream_session_id;
}

}  // namespace at::xtb
