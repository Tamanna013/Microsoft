#include "nimbus/storage_node/storage_node_service_impl.h"
#include "nimbus/storage_node/capability_token.h"
#include "nimbus/storage_node/stream_frame_config.h"
#include "nimbus/common/protocol_version.h"
#include "nimbus/common/grpc_metadata_keys.h"
#include "nimbus/common/uuid.h"
#include <chrono>

namespace nimbus::storage_node {

using namespace nimbus::proto::storage_node;

namespace {
    ChunkStatus MapStoreError(ChunkStoreError err) {
        switch (err) {
            case ChunkStoreError::ChecksumMismatch: return ChunkStatus::CHECKSUM_MISMATCH;
            case ChunkStoreError::CorruptionDetected: return ChunkStatus::CORRUPTION_DETECTED;
            case ChunkStoreError::NotFound: return ChunkStatus::NOT_FOUND;
            case ChunkStoreError::IoError: return ChunkStatus::IO_ERROR;
        }
        return ChunkStatus::IO_ERROR;
    }
}

StorageNodeServiceImpl::StorageNodeServiceImpl(std::shared_ptr<IChunkStore> chunk_store,
                                               std::shared_ptr<const nimbus::common::Logger> logger,
                                               std::shared_ptr<const nimbus::common::Config> config)
    : chunk_store_(std::move(chunk_store)), logger_(std::move(logger)), config_(std::move(config)) {
    max_chunk_bytes_ = config_->GetClusterInt("max_chunk_bytes").ValueOr(4 * 1024 * 1024);
}

std::string StorageNodeServiceImpl::GetCorrelationId(grpc::ServerContext* context) {
    auto client_metadata = context->client_metadata();
    auto iter = client_metadata.find(nimbus::common::kCorrelationIdMetadataKey);
    if (iter != client_metadata.end()) {
        return std::string(iter->second.data(), iter->second.length());
    }
    return nimbus::common::GenerateUuidV4();
}

std::optional<grpc::Status> StorageNodeServiceImpl::RunPreamble(
    grpc::ServerContext* context,
    uint32_t received_protocol_version,
    std::optional<std::string_view> capability_token,
    std::optional<ChunkId> token_scoped_chunk_id) {

    auto version_check = nimbus::common::CheckProtocolVersion(
        received_protocol_version, nimbus::common::kCurrentProtocolVersion, nimbus::common::kCurrentProtocolVersion);
    
    if (!version_check.IsOk()) {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Unsupported protocol version");
    }

    if (capability_token && token_scoped_chunk_id) {
        auto token_check = ValidateCapabilityToken(*capability_token, *token_scoped_chunk_id);
        if (!token_check.IsOk()) {
            logger_->Warn("CAPABILITY_DENIED for chunk " + token_scoped_chunk_id->ToHexString());
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "CAPABILITY_DENIED");
        }
    }

    return std::nullopt;
}

grpc::Status StorageNodeServiceImpl::PutChunk(grpc::ServerContext* context,
                                               grpc::ServerReader<PutChunkRequest>* reader,
                                               PutChunkResponse* response) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    auto start_time = std::chrono::steady_clock::now();

    response->set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    PutChunkRequest first_req;
    if (!reader->Read(&first_req)) {
        logger_->Error("PutChunk failed: stream closed before metadata");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing metadata");
    }

    if (!first_req.has_metadata()) {
        logger_->Error("PutChunk failed: first message missing metadata");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "First message must contain metadata");
    }

    auto id_res = ChunkId::Parse(first_req.metadata().chunk_id());
    if (!id_res.IsOk()) {
        logger_->Error("PutChunk failed: invalid chunk_id");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid chunk_id");
    }
    ChunkId chunk_id = id_res.Value();

    auto preamble_err = RunPreamble(context, first_req.protocol_version(), first_req.metadata().capability_token(), chunk_id);
    if (preamble_err) {
        if (preamble_err->error_message() == "CAPABILITY_DENIED") {
            response->set_status(ChunkStatus::CAPABILITY_DENIED);
            response->set_message("Capability token denied");
            return grpc::Status::OK;
        }
        return *preamble_err;
    }

    uint64_t total_size = first_req.metadata().total_size_bytes();
    if (total_size > max_chunk_bytes_) {
        logger_->Warn("CHUNK_TOO_LARGE: requested " + std::to_string(total_size) + " > " + std::to_string(max_chunk_bytes_));
        response->set_status(ChunkStatus::CHUNK_TOO_LARGE);
        response->set_message("Chunk too large");
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Chunk too large");
    }

    std::vector<std::byte> payload;
    payload.reserve(total_size);

    PutChunkRequest frame;
    while (reader->Read(&frame)) {
        if (!frame.has_data_chunk()) {
            continue;
        }
        const std::string& data = frame.data_chunk();
        if (payload.size() + data.size() > max_chunk_bytes_) {
            logger_->Warn("CHUNK_TOO_LARGE during streaming");
            response->set_status(ChunkStatus::CHUNK_TOO_LARGE);
            response->set_message("Streamed chunk too large");
            return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Streamed chunk too large");
        }
        payload.insert(payload.end(), reinterpret_cast<const std::byte*>(data.data()), reinterpret_cast<const std::byte*>(data.data() + data.size()));
    }

    auto put_res = chunk_store_->Put(chunk_id, payload);
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (put_res.IsOk()) {
        logger_->Info("PutChunk successful for " + chunk_id.ToHexString() + " in " + std::to_string(duration_ms) + "ms");
        response->set_status(ChunkStatus::OK);
        response->set_message("Success");
    } else {
        response->set_status(MapStoreError(put_res.Error().kind));
        response->set_message(put_res.Error().message);
        if (put_res.Error().kind == ChunkStoreError::IoError || put_res.Error().kind == ChunkStoreError::CorruptionDetected) {
            logger_->Error("PutChunk failed: " + put_res.Error().message);
        } else {
            logger_->Warn("PutChunk domain error: " + put_res.Error().message);
        }
    }

    return grpc::Status::OK;
}

grpc::Status StorageNodeServiceImpl::GetChunk(grpc::ServerContext* context,
                                               const GetChunkRequest* request,
                                               grpc::ServerWriter<GetChunkResponse>* writer) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    auto start_time = std::chrono::steady_clock::now();

    auto preamble_err = RunPreamble(context, request->protocol_version(), std::nullopt, std::nullopt);
    if (preamble_err) {
        return *preamble_err;
    }

    auto id_res = ChunkId::Parse(request->chunk_id());
    if (!id_res.IsOk()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid chunk_id");
    }
    ChunkId chunk_id = id_res.Value();

    GetChunkResponse meta_resp;
    meta_resp.set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    auto get_res = chunk_store_->Get(chunk_id);
    if (!get_res.IsOk()) {
        auto status = MapStoreError(get_res.Error().kind);
        meta_resp.mutable_metadata()->set_status(status);
        meta_resp.mutable_metadata()->set_total_size_bytes(0);
        writer->Write(meta_resp);
        
        if (status == ChunkStatus::CORRUPTION_DETECTED || status == ChunkStatus::IO_ERROR) {
            logger_->Error("GetChunk failed for " + chunk_id.ToHexString() + ": " + get_res.Error().message);
        } else {
            logger_->Info("GetChunk completed with " + get_res.Error().message);
        }
        return grpc::Status::OK;
    }

    meta_resp.mutable_metadata()->set_status(ChunkStatus::OK);
    meta_resp.mutable_metadata()->set_total_size_bytes(get_res.Value().size());
    writer->Write(meta_resp);

    const auto& data = get_res.Value();
    size_t offset = 0;
    while (offset < data.size()) {
        size_t chunk_size = std::min(kStreamFrameSizeBytes, static_cast<uint64_t>(data.size() - offset));
        GetChunkResponse frame;
        frame.set_protocol_version(nimbus::common::kCurrentProtocolVersion);
        frame.set_data_chunk(reinterpret_cast<const char*>(data.data() + offset), chunk_size);
        if (!writer->Write(frame)) {
            logger_->Warn("GetChunk stream broken by client");
            break;
        }
        offset += chunk_size;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    logger_->Info("GetChunk successful for " + chunk_id.ToHexString() + " in " + std::to_string(duration_ms) + "ms");

    return grpc::Status::OK;
}

grpc::Status StorageNodeServiceImpl::VerifyChunk(grpc::ServerContext* context,
                                                  const VerifyChunkRequest* request,
                                                  VerifyChunkResponse* response) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    
    response->set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    auto preamble_err = RunPreamble(context, request->protocol_version(), std::nullopt, std::nullopt);
    if (preamble_err) {
        return *preamble_err;
    }

    auto id_res = ChunkId::Parse(request->chunk_id());
    if (!id_res.IsOk()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid chunk_id");
    }
    ChunkId chunk_id = id_res.Value();

    auto ver_res = chunk_store_->VerifyOnly(chunk_id);
    if (ver_res.IsOk()) {
        response->set_is_valid(true);
        response->set_status(ChunkStatus::OK);
        logger_->Debug("VerifyChunk OK for " + chunk_id.ToHexString());
    } else {
        response->set_is_valid(false);
        response->set_status(MapStoreError(ver_res.Error().kind));
        logger_->Debug("VerifyChunk failed for " + chunk_id.ToHexString() + ": " + ver_res.Error().message);
    }
    return grpc::Status::OK;
}

grpc::Status StorageNodeServiceImpl::DeleteChunk(grpc::ServerContext* context,
                                                  const DeleteChunkRequest* request,
                                                  DeleteChunkResponse* response) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    auto start_time = std::chrono::steady_clock::now();

    response->set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    auto id_res = ChunkId::Parse(request->chunk_id());
    if (!id_res.IsOk()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid chunk_id");
    }
    ChunkId chunk_id = id_res.Value();

    auto preamble_err = RunPreamble(context, request->protocol_version(), request->capability_token(), chunk_id);
    if (preamble_err) {
        if (preamble_err->error_message() == "CAPABILITY_DENIED") {
            response->set_status(ChunkStatus::CAPABILITY_DENIED);
            return grpc::Status::OK;
        }
        return *preamble_err;
    }

    auto del_res = chunk_store_->Delete(chunk_id);
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (del_res.IsOk()) {
        response->set_status(ChunkStatus::OK);
        logger_->Info("DeleteChunk successful for " + chunk_id.ToHexString() + " in " + std::to_string(duration_ms) + "ms");
    } else {
        response->set_status(MapStoreError(del_res.Error().kind));
        logger_->Error("DeleteChunk failed for " + chunk_id.ToHexString() + ": " + del_res.Error().message);
    }

    return grpc::Status::OK;
}

grpc::Status StorageNodeServiceImpl::ReplicateChunk(grpc::ServerContext* context,
                                                     const ReplicateChunkRequest* request,
                                                     ReplicateChunkResponse* response) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    auto start_time = std::chrono::steady_clock::now();

    response->set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    auto preamble_err = RunPreamble(context, request->protocol_version(), std::nullopt, std::nullopt);
    if (preamble_err) {
        return *preamble_err;
    }

    auto id_res = ChunkId::Parse(request->chunk_id());
    if (!id_res.IsOk()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid chunk_id");
    }
    ChunkId chunk_id = id_res.Value();

    logger_->Info("ReplicateChunk requested for " + chunk_id.ToHexString() + " from " + request->source_node_address());

    auto channel = grpc::CreateChannel(request->source_node_address(), grpc::InsecureChannelCredentials());
    std::unique_ptr<StorageNode::Stub> stub = StorageNode::NewStub(channel);

    GetChunkRequest get_req;
    get_req.set_protocol_version(nimbus::common::kCurrentProtocolVersion);
    get_req.set_chunk_id(chunk_id.ToHexString());

    grpc::ClientContext client_context;
    client_context.AddMetadata(nimbus::common::kCorrelationIdMetadataKey, GetCorrelationId(context));

    auto reader = stub->GetChunk(&client_context, get_req);

    GetChunkResponse get_resp;
    if (!reader->Read(&get_resp)) {
        logger_->Error("ReplicateChunk failed: SOURCE_UNREACHABLE");
        response->set_status(ChunkStatus::SOURCE_UNREACHABLE);
        return grpc::Status::OK;
    }

    if (get_resp.has_metadata() && get_resp.metadata().status() != ChunkStatus::OK) {
        logger_->Error("ReplicateChunk source returned status: " + std::to_string(get_resp.metadata().status()));
        response->set_status(get_resp.metadata().status()); // Forward status
        return grpc::Status::OK;
    }

    std::vector<std::byte> payload;
    uint64_t expected_size = get_resp.metadata().total_size_bytes();
    if (expected_size > max_chunk_bytes_) {
        logger_->Warn("ReplicateChunk source offered chunk too large");
        response->set_status(ChunkStatus::CHUNK_TOO_LARGE);
        return grpc::Status::OK;
    }
    payload.reserve(expected_size);

    while (reader->Read(&get_resp)) {
        if (get_resp.has_data_chunk()) {
            const std::string& data = get_resp.data_chunk();
            payload.insert(payload.end(), reinterpret_cast<const std::byte*>(data.data()), reinterpret_cast<const std::byte*>(data.data() + data.size()));
        }
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        logger_->Error("ReplicateChunk RPC failed: " + status.error_message());
        response->set_status(ChunkStatus::SOURCE_UNREACHABLE);
        return grpc::Status::OK;
    }

    auto put_res = chunk_store_->Put(chunk_id, payload);
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (put_res.IsOk()) {
        logger_->Info("ReplicateChunk successful for " + chunk_id.ToHexString() + " in " + std::to_string(duration_ms) + "ms");
        response->set_status(ChunkStatus::OK);
    } else {
        logger_->Error("ReplicateChunk Put failed: " + put_res.Error().message);
        response->set_status(MapStoreError(put_res.Error().kind));
    }

    return grpc::Status::OK;
}

grpc::Status StorageNodeServiceImpl::GetNodeStats(grpc::ServerContext* context,
                                                   const GetNodeStatsRequest* request,
                                                   GetNodeStatsResponse* response) {
    auto scope = logger_->WithCorrelationId(GetCorrelationId(context));
    
    response->set_protocol_version(nimbus::common::kCurrentProtocolVersion);

    auto preamble_err = RunPreamble(context, request->protocol_version(), std::nullopt, std::nullopt);
    if (preamble_err) {
        return *preamble_err;
    }

    auto stats = chunk_store_->GetStats();
    response->set_chunk_count(stats.chunk_count);
    response->set_bytes_used(stats.bytes_used);
    response->set_bytes_free(stats.bytes_free);
    
    logger_->Debug("GetNodeStats completed");

    return grpc::Status::OK;
}

}  // namespace nimbus::storage_node
