#include <gtest/gtest.h>

#include "backfill/engine.hpp"
#include "xtb/client.hpp"

// run() delegates to runOne() per symbol; runOne() requires a live XTB connection
// and is covered by integration tests. The only pure unit-testable case here is
// an empty symbol list — run() must not touch the client and returns zero.

TEST(BackfillEngineTest, RunWithNoSymbolsReturnsZero) {
    at::backfill::BackfillEngine engine({});
    at::xtb::Client client;
    EXPECT_EQ(engine.run(client), 0);
}
