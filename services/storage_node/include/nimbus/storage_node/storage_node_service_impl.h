#pragma once
#include <memory>
#include <optional>
#include <grpcpp/grpcpp.h>
#include "nimbus/proto/storage_node.grpc.pb.h"
#include "nimbus/storage_node/i_chunk_store.h"
#include "nimbus/common/logger.h"
#include "nimbus/common/config.h"

namespace nimbus::storage_node {

class StorageNodeServiceImpl final : public nimbus::proto::storage_node::StorageNode::Service {
 public:
  StorageNodeServiceImpl(std::shared_ptr<IChunkStore> chunk_store,
                          std::shared_ptr<const nimbus::common::Logger> logger,
                          std::shared_ptr<const nimbus::common::Config> config);

  grpc::Status PutChunk(grpc::ServerContext* context,
                         grpc::ServerReader<nimbus::proto::storage_node::PutChunkRequest>* reader,
                         nimbus::proto::storage_node::PutChunkResponse* response) override;

  grpc::Status GetChunk(grpc::ServerContext* context,
                         const nimbus::proto::storage_node::GetChunkRequest* request,
                         grpc::ServerWriter<nimbus::proto::storage_node::GetChunkResponse>* writer) override;

  grpc::Status VerifyChunk(grpc::ServerContext* context,
                            const nimbus::proto::storage_node::VerifyChunkRequest* request,
                            nimbus::proto::storage_node::VerifyChunkResponse* response) override;

  grpc::Status DeleteChunk(grpc::ServerContext* context,
                            const nimbus::proto::storage_node::DeleteChunkRequest* request,
                            nimbus::proto::storage_node::DeleteChunkResponse* response) override;

  grpc::Status ReplicateChunk(grpc::ServerContext* context,
                               const nimbus::proto::storage_node::ReplicateChunkRequest* request,
                               nimbus::proto::storage_node::ReplicateChunkResponse* response) override;

  grpc::Status GetNodeStats(grpc::ServerContext* context,
                             const nimbus::proto::storage_node::GetNodeStatsRequest* request,
                             nimbus::proto::storage_node::GetNodeStatsResponse* response) override;

 private:
  std::optional<grpc::Status> RunPreamble(grpc::ServerContext* context,
                                           uint32_t received_protocol_version,
                                           std::optional<std::string_view> capability_token,
                                           std::optional<ChunkId> token_scoped_chunk_id);

  std::string GetCorrelationId(grpc::ServerContext* context);

  std::shared_ptr<IChunkStore> chunk_store_;
  std::shared_ptr<const nimbus::common::Logger> logger_;
  std::shared_ptr<const nimbus::common::Config> config_;
  uint64_t max_chunk_bytes_;
};

}  // namespace nimbus::storage_node
