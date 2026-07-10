#include <gtest/gtest.h>
#include "nimbus/common/path_validator.h"

using namespace nimbus::common;

TEST(PathValidatorTest, PositiveCases) {
    PathValidator validator;
    EXPECT_TRUE(validator.Validate("/").IsOk());
    EXPECT_TRUE(validator.Validate("/simple").IsOk());
    EXPECT_TRUE(validator.Validate("/deep/nested/path/with.special-chars_and spaces").IsOk());
}

TEST(PathValidatorTest, NegativeNotAbsolute) {
    PathValidator validator;
    auto res = validator.Validate("relative/path");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::NotAbsolute);
}

TEST(PathValidatorTest, NegativeContainsParentTraversal) {
    PathValidator validator;
    auto res = validator.Validate("/a/../b");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::ContainsParentTraversal);
}

TEST(PathValidatorTest, NegativeContainsNullByte) {
    PathValidator validator;
    std::string path = "/a/\0/b";
    auto res = validator.Validate(std::string_view(path.data(), 7));
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::ContainsNullByte);
}

TEST(PathValidatorTest, NegativeComponentTooLong) {
    PathValidatorConfig config;
    config.max_component_length = 5;
    PathValidator validator(config);
    auto res = validator.Validate("/abcdef");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::ComponentTooLong);
}

TEST(PathValidatorTest, NegativePathTooLong) {
    PathValidatorConfig config;
    config.max_path_length = 5;
    PathValidator validator(config);
    auto res = validator.Validate("/abcdef");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::PathTooLong);
}

TEST(PathValidatorTest, NegativeInvalidCharacter) {
    PathValidator validator;
    auto res = validator.Validate("/valid/inv@lid");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::InvalidCharacter);
}

TEST(PathValidatorTest, NegativeEmptyComponent) {
    PathValidator validator;
    auto res = validator.Validate("/a//b");
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), PathValidationError::EmptyComponent);
}

TEST(PathValidatorTest, NormalizeForDisplay) {
    PathValidator validator;
    EXPECT_EQ(validator.NormalizeForDisplay("/a//b///c"), "/a/b/c");
}
