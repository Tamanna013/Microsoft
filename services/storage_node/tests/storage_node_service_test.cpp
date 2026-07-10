#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "nimbus/proto/storage_node.grpc.pb.h"
#include "nimbus/storage_node/storage_node_service_impl.h"
#include "nimbus/storage_node/local_disk_chunk_store.h"
#include "nimbus/common/uuid.h"
#include "nimbus/common/sha256.h"
#include "nimbus/common/protocol_version.h"
#include <thread>
#include <filesystem>

using namespace nimbus::storage_node;
using namespace nimbus::proto::storage_node;
using namespace nimbus::common;

class StorageNodeServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / ("nimbus_sn_test_" + GenerateUuidV4());
        logger_ = std::make_shared<Logger>("logs/test_op.log", "logs/test_audit.log");
        
        auto config = std::make_shared<Config>(std::unordered_map<std::string, std::string>{
            {"data_directory", temp_dir_.string()},
            {"grpc_thread_pool_size", "2"},
            {"max_chunk_bytes", "4194304"}
        }, std::unordered_map<std::string, std::string>{});

        store_ = std::make_shared<LocalDiskChunkStore>(temp_dir_, logger_);
        service_ = std::make_unique<StorageNodeServiceImpl>(store_, logger_, config);

        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &bound_port_);
        builder.RegisterService(service_.get());
        server_ = builder.BuildAndStart();

        std::string address = "127.0.0.1:" + std::to_string(bound_port_);
        channel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        stub_ = StorageNode::NewStub(channel_);
    }

    void TearDown() override {
        server_->Shutdown();
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    std::filesystem::path temp_dir_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<LocalDiskChunkStore> store_;
    std::unique_ptr<StorageNodeServiceImpl> service_;
    std::unique_ptr<grpc::Server> server_;
    int bound_port_ = 0;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<StorageNode::Stub> stub_;
};

TEST_F(StorageNodeServiceTest, PutAndGetChunk) {
    std::string data(1024 * 1024, 'A'); // 1MB chunk
    std::string hex_id = Sha256::HashBytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()));

    // PUT
    {
        grpc::ClientContext context;
        PutChunkResponse response;
        auto writer = stub_->PutChunk(&context, &response);

        PutChunkRequest meta_req;
        meta_req.set_protocol_version(kCurrentProtocolVersion);
        meta_req.mutable_metadata()->set_chunk_id(hex_id);
        meta_req.mutable_metadata()->set_total_size_bytes(data.size());
        EXPECT_TRUE(writer->Write(meta_req));

        PutChunkRequest data_req;
        data_req.set_protocol_version(kCurrentProtocolVersion);
        data_req.set_data_chunk(data);
        EXPECT_TRUE(writer->Write(data_req));

        writer->WritesDone();
        grpc::Status status = writer->Finish();
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(response.status(), ChunkStatus::OK);
    }

    // GET
    {
        grpc::ClientContext context;
        GetChunkRequest request;
        request.set_protocol_version(kCurrentProtocolVersion);
        request.set_chunk_id(hex_id);

        auto reader = stub_->GetChunk(&context, request);

        GetChunkResponse resp;
        EXPECT_TRUE(reader->Read(&resp));
        ASSERT_TRUE(resp.has_metadata());
        EXPECT_EQ(resp.metadata().status(), ChunkStatus::OK);
        EXPECT_EQ(resp.metadata().total_size_bytes(), data.size());

        std::string received_data;
        while (reader->Read(&resp)) {
            if (resp.has_data_chunk()) {
                received_data += resp.data_chunk();
            }
        }
        grpc::Status status = reader->Finish();
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(received_data, data);
    }
}

TEST_F(StorageNodeServiceTest, PutChunkChecksumMismatch) {
    std::string data(1024, 'B');
    std::string hex_id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    grpc::ClientContext context;
    PutChunkResponse response;
    auto writer = stub_->PutChunk(&context, &response);

    PutChunkRequest meta_req;
    meta_req.set_protocol_version(kCurrentProtocolVersion);
    meta_req.mutable_metadata()->set_chunk_id(hex_id);
    meta_req.mutable_metadata()->set_total_size_bytes(data.size());
    EXPECT_TRUE(writer->Write(meta_req));

    PutChunkRequest data_req;
    data_req.set_protocol_version(kCurrentProtocolVersion);
    data_req.set_data_chunk(data);
    EXPECT_TRUE(writer->Write(data_req));

    writer->WritesDone();
    grpc::Status status = writer->Finish();
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), ChunkStatus::CHECKSUM_MISMATCH);
}

TEST_F(StorageNodeServiceTest, PutChunkTooLarge) {
    std::string hex_id = "0000000000000000000000000000000000000000000000000000000000000000";

    grpc::ClientContext context;
    PutChunkResponse response;
    auto writer = stub_->PutChunk(&context, &response);

    PutChunkRequest meta_req;
    meta_req.set_protocol_version(kCurrentProtocolVersion);
    meta_req.mutable_metadata()->set_chunk_id(hex_id);
    meta_req.mutable_metadata()->set_total_size_bytes(5 * 1024 * 1024); // 5MB
    EXPECT_TRUE(writer->Write(meta_req));

    writer->WritesDone();
    grpc::Status status = writer->Finish();
    EXPECT_EQ(status.error_code(), grpc::StatusCode::RESOURCE_EXHAUSTED);
    
    bool temp_found = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(temp_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() != ".log") {
            temp_found = true;
        }
    }
    EXPECT_FALSE(temp_found);
}
