#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "nimbus/common/config.h"
#include "nimbus/common/logger.h"
#include "nimbus/storage_node/local_disk_chunk_store.h"
#include "nimbus/storage_node/storage_node_service_impl.h"

std::atomic<bool> g_shutdown_requested{false};

void SignalHandler(int signum) {
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::string config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }
    auto config_res = nimbus::common::Config::Load(config_path);
    if (!config_res.IsOk()) {
        std::cerr << "Failed to load config: " << config_res.Error() << "\n";
        return 1;
    }
    auto config = std::make_shared<nimbus::common::Config>(std::move(config_res.Value()));

    auto logger = std::make_shared<nimbus::common::Logger>("logs/storage_node.log", "logs/storage_node_audit.log");

    std::string data_dir = config->GetNodeString("data_directory").ValueOr("/var/lib/nimbusfs/data");
    auto chunk_store = std::make_shared<nimbus::storage_node::LocalDiskChunkStore>(data_dir, logger);

    nimbus::storage_node::StorageNodeServiceImpl service(chunk_store, logger, config);

    std::string server_address = config->GetNodeString("listen_address").ValueOr("0.0.0.0:50051");
    int thread_pool_size = config->GetNodeInt("grpc_thread_pool_size").ValueOr(8);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    builder.SetMaxMessageSize(4 * 1024 * 1024 + 1024);
    builder.SetMaxReceiveMessageSize(4 * 1024 * 1024 + 1024);
    builder.SetMaxSendMessageSize(4 * 1024 * 1024 + 1024);
    
    grpc::ResourceQuota quota;
    quota.SetMaxThreads(thread_pool_size);
    builder.SetResourceQuota(quota);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        logger->Error("Failed to bind to " + server_address);
        return 1;
    }

    logger->Info("Storage Node listening on " + server_address + " with thread pool size " + std::to_string(thread_pool_size));

    while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger->Info("Shutdown requested. Gracefully shutting down gRPC server...");
    server->Shutdown();
    logger->Info("Shutdown complete.");

    return 0;
}
