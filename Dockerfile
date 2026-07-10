# Stage 1: Builder
FROM ubuntu:22.04 AS builder

# Prevent tzdata prompts
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    clang-format \
    clang-tidy \
    && rm -rf /var/lib/apt/lists/*

# Bootstrap vcpkg
# Pinned to the same baseline as vcpkg.json to ensure reproducibility
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git
WORKDIR /opt/vcpkg
RUN git checkout 3d9943660bf1cb54de93b9cd41eb43ef5be5241b
RUN ./bootstrap-vcpkg.sh -disableMetrics

# Build the project
WORKDIR /src
COPY . .
# We use the vcpkg toolchain directly in CMake
RUN cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build/release

# Stage 2: Runtime
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y wget && \
    wget -qO/bin/grpc_health_probe https://github.com/grpc-ecosystem/grpc-health-probe/releases/download/v0.4.19/grpc_health_probe-linux-amd64 && \
    chmod +x /bin/grpc_health_probe && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/release/services/storage_node/storage_node /usr/local/bin/storage_node

ENTRYPOINT ["/usr/local/bin/storage_node"]
