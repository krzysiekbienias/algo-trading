// Scratchpad executable for ad-hoc experiments. Built only when
// ENABLE_DEV_MAIN=ON. Anything you write here should NOT migrate into
// production paths — promote tested ideas into src/ instead.

#include <spdlog/spdlog.h>

#include "util/log.hpp"

int main() {
    at::log::init("debug");
    spdlog::info("dev_main: scratchpad ready");
    return 0;
}
