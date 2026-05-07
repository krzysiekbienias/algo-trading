#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <string>

#include "xtb/types.hpp"

namespace at::xtb {

// Thin wrapper around an IXWebSocket connection to XTB's xStation API.
//
// Responsibilities:
//   * open/close TLS WebSocket
//   * synchronous request/response (XTB returns one frame per command)
//   * throttle outgoing requests to respect XTB's rate limits
//   * surface errors as exceptions with XTB error codes
//
// Threading: a single Client instance is NOT thread-safe. Use one per thread
// or wrap calls in your own mutex.
class Client {
public:
    explicit Client(ClientConfig config = {});
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    // Open the WebSocket and complete the TLS handshake. Throws on failure.
    void connect();

    // Send `login` command. Stores `streamSessionId` for future streaming
    // use (Phase 2). Throws on auth failure.
    void login(const Credentials& creds);

    // Send `logout` and close the socket. Safe to call from destructor.
    void disconnect() noexcept;

    // Send any XTB command and wait for the matching response. Honors the
    // throttle configured in ClientConfig. `command` is the XTB command name
    // (e.g. "getServerTime"); `arguments` is the JSON object sent under the
    // "arguments" key (may be null).
    //
    // Returns the "returnData" field of the response on success.
    // Throws std::runtime_error on transport, timeout, or XTB error.
    nlohmann::json call(const std::string& command,
                        const nlohmann::json& arguments = nlohmann::json());

    bool isConnected() const noexcept;
    const std::string& streamSessionId() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace at::xtb
