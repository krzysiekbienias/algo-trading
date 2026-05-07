#include <gtest/gtest.h>

#include "util/env.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

using namespace at::env;

// Base path to the fixtures directory, injected by CMake at compile time.
static const std::filesystem::path kFixtures{TEST_FIXTURES_DIR};

// ─── loadFile: happy paths ────────────────────────────────────────────────────

TEST(Env, LoadFileBasicKeyValue) {
    // fixtures/valid.env:  FOO=bar  /  BAZ=qux
    auto m = loadFile(kFixtures / "valid.env");
    EXPECT_EQ(m.at("FOO"), "bar");
    EXPECT_EQ(m.at("BAZ"), "qux");
}

TEST(Env, LoadFileIgnoresComments) {
    // fixtures/comments.env:  # comment  /  FOO=bar
    auto m = loadFile(kFixtures / "comments.env");
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at("FOO"), "bar");
}

TEST(Env, LoadFileIgnoresBlankLines) {
    // fixtures/blank_lines.env:  (empty)  /  FOO=bar  /  (empty)
    auto m = loadFile(kFixtures / "blank_lines.env");
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at("FOO"), "bar");
}

TEST(Env, LoadFileStripsWhitespace) {
    // fixtures/whitespace.env:  "  FOO  =  bar  "
    auto m = loadFile(kFixtures / "whitespace.env");
    EXPECT_EQ(m.at("FOO"), "bar");
}

TEST(Env, LoadFileUnquotesDoubleQuotes) {
    // fixtures/double_quotes.env:  FOO="hello world"
    auto m = loadFile(kFixtures / "double_quotes.env");
    EXPECT_EQ(m.at("FOO"), "hello world");
}

TEST(Env, LoadFileUnquotesSingleQuotes) {
    // fixtures/single_quotes.env:  FOO='hello world'
    auto m = loadFile(kFixtures / "single_quotes.env");
    EXPECT_EQ(m.at("FOO"), "hello world");
}

TEST(Env, LoadFileExportPrefix) {
    // fixtures/export_prefix.env:  export FOO=bar
    auto m = loadFile(kFixtures / "export_prefix.env");
    EXPECT_EQ(m.at("FOO"), "bar");
}

TEST(Env, LoadFileEmptyValue) {
    // fixtures/empty_value.env:  FOO=
    auto m = loadFile(kFixtures / "empty_value.env");
    EXPECT_EQ(m.at("FOO"), "");
}

// ─── loadFile: error paths ────────────────────────────────────────────────────

TEST(Env, LoadFileMissingFileThrows) {
    EXPECT_THROW(loadFile(kFixtures / "nonexistent.env"), std::runtime_error);
}

TEST(Env, LoadFileMalformedLineThrows) {
    // fixtures/malformed.env:  NOEQUALSSIGN
    EXPECT_THROW(loadFile(kFixtures / "malformed.env"), std::runtime_error);
}

TEST(Env, LoadFileEmptyKeyThrows) {
    // fixtures/empty_key.env:  =value
    EXPECT_THROW(loadFile(kFixtures / "empty_key.env"), std::runtime_error);
}

// ─── getOr ───────────────────────────────────────────────────────────────────

TEST(Env, GetOrReturnsValueWhenSet) {
    ::setenv("AT_TEST_VAR", "hello", 1);
    EXPECT_EQ(getOr("AT_TEST_VAR", "default"), "hello");
    ::unsetenv("AT_TEST_VAR");
}

TEST(Env, GetOrReturnsDefaultWhenUnset) {
    ::unsetenv("AT_TEST_VAR");
    EXPECT_EQ(getOr("AT_TEST_VAR", "default"), "default");
}

TEST(Env, GetOrReturnsDefaultWhenEmpty) {
    ::setenv("AT_TEST_VAR", "", 1);
    EXPECT_EQ(getOr("AT_TEST_VAR", "default"), "default");
    ::unsetenv("AT_TEST_VAR");
}

// ─── get ─────────────────────────────────────────────────────────────────────

TEST(Env, GetReturnsValueWhenSet) {
    ::setenv("AT_TEST_VAR", "hello", 1);
    const auto v = get("AT_TEST_VAR");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "hello");
    ::unsetenv("AT_TEST_VAR");
}

TEST(Env, GetReturnsNulloptWhenUnset) {
    ::unsetenv("AT_TEST_VAR");
    EXPECT_FALSE(get("AT_TEST_VAR").has_value());
}

TEST(Env, GetReturnsNulloptWhenEmpty) {
    ::setenv("AT_TEST_VAR", "", 1);
    EXPECT_FALSE(get("AT_TEST_VAR").has_value());
    ::unsetenv("AT_TEST_VAR");
}

// ─── require ─────────────────────────────────────────────────────────────────

TEST(Env, RequireReturnsValueWhenSet) {
    ::setenv("AT_TEST_VAR", "hello", 1);
    EXPECT_EQ(require("AT_TEST_VAR"), "hello");
    ::unsetenv("AT_TEST_VAR");
}

TEST(Env, RequireThrowsWhenUnset) {
    ::unsetenv("AT_TEST_VAR");
    EXPECT_THROW(require("AT_TEST_VAR"), std::runtime_error);
}

TEST(Env, RequireThrowsWhenEmpty) {
    ::setenv("AT_TEST_VAR", "", 1);
    EXPECT_THROW(require("AT_TEST_VAR"), std::runtime_error);
    ::unsetenv("AT_TEST_VAR");
}

// ─── loadIntoProcess ─────────────────────────────────────────────────────────

TEST(Env, LoadIntoProcessSetsEnvVar) {
    // fixtures/load_into_process.env:  AT_TEST_LOAD=from_file
    ::unsetenv("AT_TEST_LOAD");
    loadIntoProcess(kFixtures / "load_into_process.env");
    EXPECT_STREQ(std::getenv("AT_TEST_LOAD"), "from_file");
    ::unsetenv("AT_TEST_LOAD");
}

TEST(Env, LoadIntoProcessOsEnvWins) {
    // overwrite=0 means a variable already in the OS env must NOT be overwritten.
    ::setenv("AT_TEST_LOAD", "from_os", 1);
    loadIntoProcess(kFixtures / "load_into_process.env");
    EXPECT_STREQ(std::getenv("AT_TEST_LOAD"), "from_os");
    ::unsetenv("AT_TEST_LOAD");
}
