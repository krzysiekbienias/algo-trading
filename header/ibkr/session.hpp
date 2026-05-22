#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace at::ibkr {

// Connection settings for Trader Workstation / IB Gateway socket API.
struct SessionConfig {
    std::string host = "127.0.0.1";
    int port = 7497;  // paper default; live = 7496
    int client_id = 1;
    std::chrono::milliseconds timeout = std::chrono::seconds(15);
};

// Thin wrapper around IBKR EClientSocket: connect + reqCurrentTime smoke test.
class Session {
public:
    explicit Session(SessionConfig config = {});
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void connect();
    std::int64_t reqCurrentTime();
    void disconnect();

    [[nodiscard]] bool isConnected() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace at::ibkr
