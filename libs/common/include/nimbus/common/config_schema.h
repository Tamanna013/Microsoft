#pragma once

#include <map>
#include <string>

namespace nimbus::common {

enum class ConfigFieldType { String, Int64, Double, Bool };
enum class ConfigSection { ClusterInvariant, NodeLocal };

struct ConfigFieldSpec {
  ConfigFieldType type;
  bool required;
};

class ConfigSchema {
 public:
  void RequireClusterInvariant(const std::string& key, ConfigFieldType type);
  void RequireNodeLocal(const std::string& key, ConfigFieldType type);
  void OptionalClusterInvariant(const std::string& key, ConfigFieldType type);
  void OptionalNodeLocal(const std::string& key, ConfigFieldType type);

  const std::map<std::string, ConfigFieldSpec>& GetClusterInvariantSpecs() const;
  const std::map<std::string, ConfigFieldSpec>& GetNodeLocalSpecs() const;

 private:
  std::map<std::string, ConfigFieldSpec> cluster_invariant_specs_;
  std::map<std::string, ConfigFieldSpec> node_local_specs_;
};

}  // namespace nimbus::common
