#include <gtest/gtest.h>
#include "nimbus/common/config.h"
#include <fstream>

using namespace nimbus::common;

TEST(ConfigTest, Validation) {
    ConfigSchema schema;
    schema.RequireClusterInvariant("cluster_port", ConfigFieldType::Int64);
    schema.OptionalNodeLocal("node_name", ConfigFieldType::String);

    std::ofstream("test_config.json") << R"({"cluster_invariant": {"cluster_port": 8080}})";
    
    auto load_res = Config::LoadFromFile("test_config.json");
    ASSERT_TRUE(load_res.IsOk());
    
    auto val_res = load_res.Value().Validate(schema);
    ASSERT_TRUE(val_res.IsOk());

    EXPECT_EQ(load_res.Value().GetInt64(ConfigSection::ClusterInvariant, "cluster_port").Value(), 8080);
    EXPECT_EQ(load_res.Value().GetString(ConfigSection::NodeLocal, "node_name", "default"), "default");
}

TEST(ConfigTest, MissingRequired) {
    ConfigSchema schema;
    schema.RequireClusterInvariant("cluster_port", ConfigFieldType::Int64);

    std::ofstream("test_config2.json") << R"({"cluster_invariant": {}})";
    auto load_res = Config::LoadFromFile("test_config2.json");
    ASSERT_TRUE(load_res.IsOk());
    
    auto val_res = load_res.Value().Validate(schema);
    ASSERT_FALSE(val_res.IsOk());
}
