// TEMPORARY: This is scaffold for Milestone 3 to prove gRPC works.
#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "nimbus/proto/greeter.grpc.pb.h"
#include "nimbus/common/logger.h"
#include "nimbus/common/config.h"
#include "nimbus/common/protocol_version.h"
#include "nimbus/common/uuid.h"
#include "nimbus/common/grpc_metadata_keys.h"

using namespace nimbus::common;
using namespace nimbus::proto;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class GreeterServiceImpl final : public Greeter::Service {
 public:
  explicit GreeterServiceImpl(std::shared_ptr<Logger> logger) : logger_(std::move(logger)) {}

  Status SayHello(ServerContext* context, const HelloRequest* request, HelloResponse* reply) override {
    auto client_metadata = context->client_metadata();
    auto iter = client_metadata.find(kCorrelationIdMetadataKey);
    std::string correlation_id;
    if (iter != client_metadata.end()) {
        correlation_id = std::string(iter->second.data(), iter->second.length());
    } else {
        correlation_id = GenerateUuidV4(); // fallback
    }

    auto scope = logger_->WithCorrelationId(correlation_id);

    logger_->Info("Received SayHello request from " + request->name());

    auto version_check = CheckProtocolVersion(request->protocol_version(), kCurrentProtocolVersion, kCurrentProtocolVersion);
    if (!version_check.IsOk()) {
        logger_->Warn("Protocol version mismatch");
        return Status(grpc::StatusCode::FAILED_PRECONDITION, "Unsupported protocol version");
    }

    reply->set_protocol_version(kCurrentProtocolVersion);
    reply->set_message("Hello, " + request->name() + "! (from greeter_server)");
    
    logger_->Info("Sending SayHello reply");
    return Status::OK;
  }
 private:
  std::shared_ptr<Logger> logger_;
};

int main(int argc, char** argv) {
  auto logger = std::make_shared<Logger>("logs/greeter_server_op.log", "logs/greeter_server_audit.log");
  
  ConfigSchema schema;
  schema.AddNodeLocalField("listen_address", "0.0.0.0:50051");
  Config config(schema);
  auto res = config.LoadFromFile("config.json");
  std::string server_address = config.GetNodeLocalString("listen_address").ValueOr("0.0.0.0:50051");

  GreeterServiceImpl service(logger);

  grpc::EnableDefaultHealthCheckService(true);
  
  ServerBuilder builder;
  // STUB(M17): replace with TLS credentials
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<Server> server(builder.BuildAndStart());
  logger->Info("Server listening on " + server_address);
  server->Wait();

  return 0;
}
