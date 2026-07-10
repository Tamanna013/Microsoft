#include <iostream>
#include "nimbus/version.h"

int main() {
    // STUB(M1): This will be replaced with structured logging in M1
    std::cout << "NimbusFS Coordinator v" 
              << NIMBUS_VERSION_MAJOR << "." 
              << NIMBUS_VERSION_MINOR << "." 
              << NIMBUS_VERSION_PATCH << " starting..." << std::endl;
    return 0;
}
