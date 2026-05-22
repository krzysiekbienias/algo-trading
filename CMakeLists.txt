cmake_minimum_required(VERSION 3.20)
project(algo_trading
    VERSION 0.1.0
    DESCRIPTION "Algo-trading data pipeline (IBKR TWS -> Parquet)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json so clangd in the IDE can resolve includes.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified (so optimized Arrow paths kick in).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Apple Silicon: pick up Homebrew installs first.
if(APPLE AND EXISTS "/opt/homebrew")
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    # Homebrew's openssl@3 is keg-only, point CMake at it explicitly.
    if(EXISTS "/opt/homebrew/opt/openssl@3")
        list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/openssl@3")
    endif()
endif()

option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_APP "Build app (production CLI)" ON)
option(ENABLE_DEV_MAIN "Build dev_main executable (experiments)" ON)
option(ENABLE_IBKR "Build IBKR TWS API integration (requires sibling twsapi_macunix/IBJts)" ON)

# --- External dependencies ---
# Heavy / system-y deps come from Homebrew (see scripts/install_deps.sh).
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Arrow CONFIG REQUIRED)
find_package(Parquet CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

if(ENABLE_IBKR)
    include(cmake/TwsApi.cmake)
endif()

# --- Core library: storage + utils ---
add_library(core_lib STATIC
    src/core_lib.cpp
    src/util/env.cpp
    src/util/log.cpp
    src/util/time.cpp
    src/storage/parquet_writer.cpp
    src/storage/data_lake_paths.cpp
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/header
)

file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/header/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/header/*.h"
)
target_sources(core_lib PRIVATE ${CORE_HEADERS})

target_link_libraries(core_lib PUBLIC
    nlohmann_json::nlohmann_json
    Arrow::arrow_shared
    Parquet::parquet_shared
    spdlog::spdlog
    fmt::fmt
)

target_compile_options(core_lib PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
)

if(ENABLE_IBKR)
    add_library(ibkr_session STATIC
        src/ibkr/session.cpp
        src/ibkr/contract.cpp
    )
    target_include_directories(ibkr_session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/header)
    target_link_libraries(ibkr_session PUBLIC twsapi spdlog::spdlog)
    target_compile_options(ibkr_session PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
    )
endif()

# --- Production CLI app ---
if(ENABLE_APP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/main.cpp")
    add_executable(app app/main.cpp)
    target_link_libraries(app PRIVATE core_lib)
    if(ENABLE_IBKR)
        target_link_libraries(app PRIVATE ibkr_session)
        target_compile_definitions(app PRIVATE ENABLE_IBKR=1)
    endif()
endif()

# --- Dev / scratch main ---
if(ENABLE_DEV_MAIN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/dev_main.cpp")
    add_executable(dev_main app/dev_main.cpp)
    target_link_libraries(dev_main PRIVATE core_lib)
    target_compile_definitions(dev_main PRIVATE DEV_MAIN_BUILD=1)
    if(ENABLE_IBKR)
        target_link_libraries(dev_main PRIVATE ibkr_session)
        target_compile_definitions(dev_main PRIVATE ENABLE_IBKR=1)
        # twsapi is built as a shared library in the build tree.
        set_target_properties(dev_main PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR}/ibjts-client/lib"
        )
    endif()
endif()

# --- Unit tests ---
if(ENABLE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(unit_tests)
endif()
