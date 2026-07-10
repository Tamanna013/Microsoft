# NimbusFS Coding Standards

## 1. Naming Conventions
- **Classes, Structs, and Enums**: `PascalCase` (e.g., `StorageNode`, `ChunkMetadata`).
- **Functions and Methods**: `PascalCase` (e.g., `GetChunk`, `Initialize`).
- **Member Variables**: `snake_case_with_trailing_underscore_` (e.g., `chunk_size_`).
- **Constants**: `kPascalCase` (e.g., `kMaxChunkSize`).
- **Namespaces**: `snake_case` (e.g., `nimbus::common`).

## 2. File Naming
- Files must use `snake_case.h` and `snake_case.cpp` (e.g., `storage_node.h`, `storage_node.cpp`).
- There must be exactly one primary class per file matching the file name.

## 3. Header Guards
- Use `#pragma once` exclusively in all header files. Do not use manual `#ifndef` include guards.

## 4. Include Ordering
Includes must be ordered in the following sequence, with each group separated by a blank line and alphabetized within the group:
1. The matching `.h` file for this `.cpp` file.
2. C++ Standard Library headers.
3. Third-party library headers (e.g., `<grpcpp/grpcpp.h>`, `<nlohmann/json.hpp>`).
4. Other NimbusFS project headers.

## 5. Memory Management and RAII
- **RAII is mandatory**. Every class with non-trivial resource ownership must follow Resource Acquisition Is Initialization.
- **No manual `new` or `delete`** anywhere in the codebase. Use value semantics or smart pointers (`std::unique_ptr`, `std::shared_ptr`). This is strictly enforced by `.clang-tidy`.

## 6. Error Handling (Exceptions vs. Results)
- **Exceptions** are reserved for truly exceptional conditions (e.g., out of memory, programmer logic errors) and constructor failures where an object cannot be initialized to a valid state.
- **Expected and recoverable error conditions** in business logic must use a `Result<T, Error>` return type rather than throwing exceptions.

## 7. Deferred Functionality / Stubs
- Use the following comment convention for functionality that is intentionally stubbed out to be completed in a later milestone:
  `// STUB(M<N>): description of the stub behavior`
- Example: `// STUB(M17): capability token validation always returns OK until security milestone`

## 8. Time and Clocks
- **Always inject `nimbus::common::IClock`** for time-dependent operations. 
- Never call `std::chrono::system_clock::now()` directly inside testable business logic. This allows deterministic testing using `nimbus::common::FakeClock`.
