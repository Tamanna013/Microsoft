#include "nimbus/common/uuid.h"
#include <uuid.h>
#include <random>

namespace nimbus::common {

std::string GenerateUuidV4() {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local uuids::uuid_random_generator uuid_gen{gen};
    return uuids::to_string(uuid_gen());
}

}  // namespace nimbus::common
