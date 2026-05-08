#include "xtb/api.hpp"

#include <nlohmann/json.hpp>

#include <cmath>

namespace at::xtb::api {

at::time::Timestamp getServerTime(Client& client) {
    // XTB response: {"status": true, "returnData": {"time": <epoch_ms>, ...}}
    auto data = client.call("getServerTime");
    return at::time::fromEpochMs(data.at("time").get<std::int64_t>());
}

std::vector<Bar> getChartRange(Client& client,
                               const std::string& symbol,
                               Period period,
                               at::time::Timestamp start,
                               at::time::Timestamp end) {
    nlohmann::json args = {
        {"info", {
            {"symbol", symbol},
            {"period", static_cast<int>(period)},
            {"start",  at::time::toEpochMs(start)},
            {"end",    at::time::toEpochMs(end)},
            {"ticks",  0},
        }},
    };

    auto data = client.call("getChartRangeRequest", args);

    // XTB price encoding:
    //   open             — absolute price * 10^digits
    //   close, high, low — DELTA from open * 10^digits (NOT absolute prices)
    // We must add the delta back to open before dividing by the divisor.
    const int    digits  = data.at("digits").get<int>();
    const double divisor = std::pow(10.0, digits);

    const auto& rate_infos = data.at("rateInfos");
    std::vector<Bar> bars;
    bars.reserve(rate_infos.size());

    for (const auto& r : rate_infos) {
        const double open_raw = r.at("open").get<double>();
        bars.push_back(Bar{
            at::time::fromEpochMs(r.at("ctm").get<std::int64_t>()),
            open_raw / divisor,
            (open_raw + r.at("high").get<double>())  / divisor,
            (open_raw + r.at("low").get<double>())   / divisor,
            (open_raw + r.at("close").get<double>()) / divisor,
            r.at("vol").get<double>(),
        });
    }

    return bars;
}

}  // namespace at::xtb::api
