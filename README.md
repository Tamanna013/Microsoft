# NimbusFS

NimbusFS is a distributed, replicated, fault-tolerant file storage platform written in modern C++20, using gRPC/Protocol Buffers for all inter-node and client-server communication. It is conceptually similar to Dropbox/Google Drive/OneDrive in that it allows a user to upload, download, version, and synchronize files — but its purpose is fundamentally different from a consumer product: **NimbusFS exists to demonstrate systems-engineering depth**.

## Project Status
This project is under active milestone-based development. See the Milestone Roadmap document for the progression of features.

## Build Instructions

1. **Bootstrap vcpkg**:
   ```bash
   ./scripts/bootstrap.sh
   ```

2. **Configure CMake**:
   ```bash
   cmake --preset release
   ```

3. **Build the project**:
   ```bash
   cmake --build build/release
   ```

4. **Run tests**:
   ```bash
   cd build/release
   ctest
   ```
