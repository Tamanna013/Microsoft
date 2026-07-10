#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "nimbus/proto/storage_node.grpc.pb.h"
#include "nimbus/storage_node/storage_node_service_impl.h"
#include "nimbus/storage_node/local_disk_chunk_store.h"
#include "nimbus/common/uuid.h"
#include "nimbus/common/sha256.h"
#include "nimbus/common/protocol_version.h"

using namespace nimbus::storage_node;
using namespace nimbus::proto::storage_node;
using namespace nimbus::common;

class ReplicateChunkTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_a_ = std::filesystem::temp_directory_path() / ("nimbus_sn_test_a_" + GenerateUuidV4());
        temp_dir_b_ = std::filesystem::temp_directory_path() / ("nimbus_sn_test_b_" + GenerateUuidV4());
        
        logger_ = std::make_shared<Logger>("logs/test_op.log", "logs/test_audit.log");
        auto config = std::make_shared<Config>(std::unordered_map<std::string, std::string>{
            {"grpc_thread_pool_size", "2"},
            {"max_chunk_bytes", "4194304"}
        }, std::unordered_map<std::string, std::string>{});

        store_a_ = std::make_shared<LocalDiskChunkStore>(temp_dir_a_, logger_);
        service_a_ = std::make_unique<StorageNodeServiceImpl>(store_a_, logger_, config);

        store_b_ = std::make_shared<LocalDiskChunkStore>(temp_dir_b_, logger_);
        service_b_ = std::make_unique<StorageNodeServiceImpl>(store_b_, logger_, config);

        grpc::ServerBuilder builder_a;
        builder_a.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_a_);
        builder_a.RegisterService(service_a_.get());
        server_a_ = builder_a.BuildAndStart();

        grpc::ServerBuilder builder_b;
        builder_b.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_b_);
        builder_b.RegisterService(service_b_.get());
        server_b_ = builder_b.BuildAndStart();
    }

    void TearDown() override {
        server_a_->Shutdown();
        server_b_->Shutdown();
        if (std::filesystem::exists(temp_dir_a_)) std::filesystem::remove_all(temp_dir_a_);
        if (std::filesystem::exists(temp_dir_b_)) std::filesystem::remove_all(temp_dir_b_);
    }

    std::filesystem::path temp_dir_a_;
    std::filesystem::path temp_dir_b_;
    std::shared_ptr<Logger> logger_;
    
    std::shared_ptr<LocalDiskChunkStore> store_a_;
    std::unique_ptr<StorageNodeServiceImpl> service_a_;
    std::unique_ptr<grpc::Server> server_a_;
    int port_a_ = 0;

    std::shared_ptr<LocalDiskChunkStore> store_b_;
    std::unique_ptr<StorageNodeServiceImpl> service_b_;
    std::unique_ptr<grpc::Server> server_b_;
    int port_b_ = 0;
};

TEST_F(ReplicateChunkTest, ReplicateSuccessfully) {
    std::string data = "Hello Peer-to-Peer Replication!";
    auto id_res = ChunkId::FromDigest(Sha256::HashBytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size())));
    
    std::vector<std::byte> payload(reinterpret_cast<const std::byte*>(data.data()), reinterpret_cast<const std::byte*>(data.data() + data.size()));
    EXPECT_TRUE(store_a_->Put(id_res, payload).IsOk());

    std::string address_b = "127.0.0.1:" + std::to_string(port_b_);
    auto channel_b = grpc::CreateChannel(address_b, grpc::InsecureChannelCredentials());
    auto stub_b = StorageNode::NewStub(channel_b);

    grpc::ClientContext context;
    ReplicateChunkRequest req;
    req.set_protocol_version(kCurrentProtocolVersion);
    req.set_chunk_id(id_res.ToHexString());
    req.set_source_node_address("127.0.0.1:" + std::to_string(port_a_));

    ReplicateChunkResponse resp;
    grpc::Status status = stub_b->ReplicateChunk(&context, req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.status(), ChunkStatus::OK);

    EXPECT_TRUE(store_b_->Exists(id_res));
    auto get_res = store_b_->Get(id_res);
    EXPECT_TRUE(get_res.IsOk());
    EXPECT_EQ(get_res.Value().size(), payload.size());
}
