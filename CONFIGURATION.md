# Configuration Options (M1 + M5)

## Cluster-Invariant Config Fields
- `protocol_version`: Current protocol version (e.g. `1`).
- `chunk_size_bytes`: The nominal target size of a chunk.
- `max_chunk_bytes`: The hard limit for a chunk payload size (e.g., `4194304` for 4MiB).
- `stream_frame_bytes`: Fixed wire chunk-frame size for streamed requests (e.g., `262144` for 256KiB).

## Node-Local Config Fields
- `listen_address`: The IP:port for the node's gRPC server to bind (e.g. `0.0.0.0:50051`).
- `data_directory`: Absolute path to the node's local disk chunk store.
- `grpc_thread_pool_size`: Size of the isolated RPC-handling thread pool.
