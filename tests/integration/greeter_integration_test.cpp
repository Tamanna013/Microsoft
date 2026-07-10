#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "nimbus/proto/greeter.grpc.pb.h"
#include "nimbus/common/logger.h"
#include "nimbus/common/protocol_version.h"
#include "nimbus/common/uuid.h"
#include "nimbus/common/grpc_metadata_keys.h"
#include <thread>
#include <memory>

using namespace nimbus::common;
using namespace nimbus::proto;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;

class GreeterServiceImpl final : public Greeter::Service {
 public:
  Status SayHello(ServerContext* context, const HelloRequest* request, HelloResponse* reply) override {
    auto version_check = CheckProtocolVersion(request->protocol_version(), kCurrentProtocolVersion, kCurrentProtocolVersion);
    if (!version_check.IsOk()) {
        return Status(grpc::StatusCode::FAILED_PRECONDITION, "Unsupported protocol version");
    }
    
    // Check correlation id fallback behavior if absent
    auto iter = context->client_metadata().find(kCorrelationIdMetadataKey);
    if (iter == context->client_metadata().end() && request->name() == "RequireMetadata") {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing correlation id");
    }

    reply->set_protocol_version(kCurrentProtocolVersion);
    reply->set_message("Hello, " + request->name());
    return Status::OK;
  }
};

class GreeterIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ServerBuilder builder;
    int port = 0;
    // STUB(M17): replace with TLS credentials
    builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    ASSERT_TRUE(server_) << "Server failed to start";
    ASSERT_GT(port, 0);

    auto channel = grpc::CreateChannel("localhost:" + std::to_string(port), grpc::InsecureChannelCredentials());
    stub_ = Greeter::NewStub(channel);
  }

  void TearDown() override {
    if (server_) {
        server_->Shutdown();
    }
  }

  GreeterServiceImpl service_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<Greeter::Stub> stub_;
};

TEST_F(GreeterIntegrationTest, Success) {
    HelloRequest request;
    request.set_protocol_version(kCurrentProtocolVersion);
    request.set_name("TestClient");
    
    HelloResponse reply;
    ClientContext context;
    context.AddMetadata(kCorrelationIdMetadataKey, GenerateUuidV4());
    
    Status status = stub_->SayHello(&context, request, &reply);
    ASSERT_TRUE(status.ok());
    EXPECT_EQ(reply.message(), "Hello, TestClient");
    EXPECT_EQ(reply.protocol_version(), kCurrentProtocolVersion);
}

TEST_F(GreeterIntegrationTest, VersionMismatch) {
    HelloRequest request;
    request.set_protocol_version(0); // Invalid version
    request.set_name("TestClient");
    
    HelloResponse reply;
    ClientContext context;
    
    Status status = stub_->SayHello(&context, request, &reply);
    ASSERT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(GreeterIntegrationTest, MissingCorrelationIdFallback) {
    HelloRequest request;
    request.set_protocol_version(kCurrentProtocolVersion);
    request.set_name("FallbackTest"); // Will not trigger RequireMetadata error
    
    HelloResponse reply;
    ClientContext context;
    // Note: NOT adding correlation id metadata
    
    Status status = stub_->SayHello(&context, request, &reply);
    ASSERT_TRUE(status.ok()); // Server handles fallback internally
}
