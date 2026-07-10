#include "nimbus/common/config_schema.h"

namespace nimbus::common {

void ConfigSchema::RequireClusterInvariant(const std::string& key, ConfigFieldType type) {
  cluster_invariant_specs_[key] = {type, true};
}

void ConfigSchema::RequireNodeLocal(const std::string& key, ConfigFieldType type) {
  node_local_specs_[key] = {type, true};
}

void ConfigSchema::OptionalClusterInvariant(const std::string& key, ConfigFieldType type) {
  cluster_invariant_specs_[key] = {type, false};
}

void ConfigSchema::OptionalNodeLocal(const std::string& key, ConfigFieldType type) {
  node_local_specs_[key] = {type, false};
}

const std::map<std::string, ConfigFieldSpec>& ConfigSchema::GetClusterInvariantSpecs() const {
  return cluster_invariant_specs_;
}

const std::map<std::string, ConfigFieldSpec>& ConfigSchema::GetNodeLocalSpecs() const {
  return node_local_specs_;
}

}  // namespace nimbus::common
