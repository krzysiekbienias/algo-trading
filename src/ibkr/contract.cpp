#include "ibkr/contract.hpp"

namespace at::ibkr {

Contract makeStockSmart(const std::string& symbol, const std::string& currency) {
    Contract c;
    c.symbol = symbol;
    c.secType = "STK";
    c.currency = currency;
    c.exchange = "SMART";
    return c;
}

Contract makeForexIdealpro(const std::string& base, const std::string& quote) {
    Contract c;
    c.symbol = base;
    c.secType = "CASH";
    c.currency = quote;
    c.exchange = "IDEALPRO";
    return c;
}

}  // namespace at::ibkr
