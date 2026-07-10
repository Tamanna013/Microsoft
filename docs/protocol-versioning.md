# Protocol Versioning Convention

This document defines the **mandatory protocol versioning rules** for all remote procedure calls (gRPC/Protobuf) within the NimbusFS project.

## The Rule

Every top-level request and response message in every `.proto` file written for this project must begin with:

```protobuf
message SomeRequest {
  uint32 protocol_version = 1;  // ALWAYS field number 1, ALWAYS this name, ALWAYS this type.
  // ... message-specific fields start at field number 2.
}
```

## Why?

This convention ensures that every component can safely identify the protocol version of the peer it is communicating with. By establishing this pattern from the very first commit, we avoid retrofitting complexity later.

## Server-Side Enforcement

Every RPC server handler method MUST call `nimbus::common::CheckProtocolVersion(request->protocol_version(), MIN, MAX)` as its very first action.
- If it returns an `Err`, the handler MUST immediately return `grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "...")`.
- Internal logic below the RPC handler boundary does not deal with protocol versions.

## Implemented Services

- **Storage Node (M5):** `storage_node.proto` is the first real (non-demo) proto file to follow this convention. All RPCs enforce protocol version 1.
