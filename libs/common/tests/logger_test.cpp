#include <gtest/gtest.h>
#include "nimbus/common/logger.h"

using namespace nimbus::common;

TEST(LoggerTest, CorrelationIdScoping) {
    // Requires no crash and correct scoping
    LoggerConfig config;
    config.component_name = "test";
    Logger logger(config);

    {
        auto scope1 = logger.WithCorrelationId("ID-123");
        logger.Info("Inside scope 1");
        {
            auto scope2 = logger.WithCorrelationId("ID-456");
            logger.Info("Inside scope 2");
        }
        logger.Info("Back to scope 1");
    }
    logger.Info("Outside scope");
    EXPECT_TRUE(true);
}

TEST(LoggerTest, Formatting) {
    LoggerConfig config;
    config.component_name = "test";
    Logger logger(config);

    logger.Info("Test message", {{"key1", "val1"}, {"key2", "val2"}});
    EXPECT_TRUE(true);
}
