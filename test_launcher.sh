#!/usr/bin/env bash
set -e

ninja -C cmake-build-debug
./cmake-build-debug/test_environment ${1:+--gtest_filter="$1.*"}