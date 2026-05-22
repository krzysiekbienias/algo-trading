#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "ibkr/historical.hpp"
#include "market/types.hpp"

struct Contract;

namespace at::ibkr {

struct SessionConfig {
    std::string host = "127.0.0.1";
    int port = 7497;  // paper default; live = 7496
    int client_id = 1;
    std::chrono::milliseconds timeout = std::chrono::seconds(15);
};

class Session {
public:
    explicit Session(SessionConfig config = {});
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void connect();
    std::int64_t reqCurrentTime();

    // One synchronous historical chunk: blocks until historicalDataEnd (or error).
    // May throw PacingViolationError on IBKR error 162.
    std::vector<at::market::Bar> reqHistoricalBars(const Contract& contract,
                                                   const HistoricalRequest& request);

    void disconnect();

    [[nodiscard]] bool isConnected() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace at::ibkr
