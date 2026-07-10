#include <gtest/gtest.h>
#include "nimbus/common/uuid.h"
#include <unordered_set>

using namespace nimbus::common;

TEST(UuidTest, FormatValidation) {
    std::string uuid = GenerateUuidV4();
    // length must be 36
    ASSERT_EQ(uuid.length(), 36);
    
    // Check hyphens
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');

    // Check version nibble
    EXPECT_EQ(uuid[14], '4');

    // Check variant nibble (8, 9, a, or b)
    char variant = uuid[19];
    EXPECT_TRUE(variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

TEST(UuidTest, Uniqueness) {
    std::unordered_set<std::string> generated;
    for (int i = 0; i < 10000; ++i) {
        std::string uuid = GenerateUuidV4();
        EXPECT_TRUE(generated.insert(uuid).second);
    }
}
