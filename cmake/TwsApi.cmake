# Locate / build the IBKR TWS API client library (sibling SDK by default).
#
# Set IBJTS_ROOT to the IBJts/ directory from the official Mac/Unix API package,
# e.g. ../twsapi_macunix/IBJts relative to this repo.

if(NOT IBJTS_ROOT)
    get_filename_component(_algo_trading_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(IBJTS_ROOT "${_algo_trading_root}/../twsapi_macunix/IBJts")
endif()

set(IBJTS_CLIENT_DIR "${IBJTS_ROOT}/source/cppclient/client")

if(NOT EXISTS "${IBJTS_CLIENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "IBKR TWS API not found at IBJTS_ROOT=${IBJTS_ROOT}. "
        "Download API Components (Mac/Unix Stable) and set -DIBJTS_ROOT=...")
endif()

# Build the official twsapi shared library inside our build tree.
add_subdirectory("${IBJTS_CLIENT_DIR}" "${CMAKE_BINARY_DIR}/ibjts-client")
