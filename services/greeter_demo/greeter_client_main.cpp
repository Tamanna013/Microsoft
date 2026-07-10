// TEMPORARY: This is scaffold for Milestone 3 to prove gRPC works.
#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "nimbus/proto/greeter.grpc.pb.h"
#include "nimbus/common/logger.h"
#include "nimbus/common/config.h"
#include "nimbus/common/protocol_version.h"
#include "nimbus/common/uuid.h"
#include "nimbus/common/grpc_metadata_keys.h"

using namespace nimbus::common;
using namespace nimbus::proto;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

int main(int argc, char** argv) {
  auto logger = std::make_shared<Logger>("logs/greeter_client_op.log", "logs/greeter_client_audit.log");

  ConfigSchema schema;
  schema.AddNodeLocalField("target_address", "localhost:50051");
  Config config(schema);
  config.LoadFromFile("client_config.json");
  std::string target_address = config.GetNodeLocalString("target_address").ValueOr("localhost:50051");

  logger->Info("Client connecting to " + target_address);

  // STUB(M17): replace with TLS credentials
  auto channel = grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<Greeter::Stub> stub = Greeter::NewStub(channel);

  HelloRequest request;
  request.set_protocol_version(kCurrentProtocolVersion);
  request.set_name("NimbusFS-Client");

  HelloResponse reply;
  ClientContext context;
  
  std::string correlation_id = GenerateUuidV4();
  context.AddMetadata(kCorrelationIdMetadataKey, correlation_id);
  
  auto scope = logger->WithCorrelationId(correlation_id);
  
  logger->Info("Sending SayHello request");
  
  Status status = stub->SayHello(&context, request, &reply);

  if (status.ok()) {
    logger->Info("Received reply: " + reply.message());
    std::cout << "Greeter received: " << reply.message() << std::endl;
  } else {
    logger->Error("RPC failed: " + status.error_message());
    std::cout << "RPC failed. Error: " << status.error_message() << std::endl;
  }

  return 0;
}
