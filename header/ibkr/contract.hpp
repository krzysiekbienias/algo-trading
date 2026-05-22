#pragma once

#include <Contract.h>
#include <string>

namespace at::ibkr {

// US equity routed via SMART.
Contract makeStockSmart(const std::string& symbol, const std::string& currency = "USD");

// FX spot on IDEALPRO (symbol e.g. "EUR" with currency "USD" for EUR.USD).
Contract makeForexIdealpro(const std::string& base, const std::string& quote);

}  // namespace at::ibkr
