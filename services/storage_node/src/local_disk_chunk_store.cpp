#include "nimbus/storage_node/local_disk_chunk_store.h"
#include "nimbus/common/sha256.h"
#include "nimbus/common/uuid.h"
#include <fstream>
#include <system_error>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace nimbus::storage_node {

// Internal RAII wrapper for a POSIX file descriptor, specifically to fsync directories safely.
class ScopedFd {
public:
    explicit ScopedFd(int fd) : fd_(fd) {}
    ~ScopedFd() {
        if (fd_ >= 0) {
#ifdef _WIN32
            ::_close(fd_);
#else
            ::close(fd_);
#endif
        }
    }
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
    
    int get() const { return fd_; }
    int release() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }
private:
    int fd_;
};

#ifdef _WIN32
static inline int internal_fsync(int fd) {
    return _commit(fd);
}
static inline int internal_open(const char* path, int oflag, int pmode = 0) {
    return _open(path, oflag | O_BINARY, pmode);
}
#else
static inline int internal_fsync(int fd) {
    return ::fsync(fd);
}
static inline int internal_open(const char* path, int oflag, int pmode = 0) {
    return ::open(path, oflag, pmode);
}
#endif

LocalDiskChunkStore::LocalDiskChunkStore(std::filesystem::path data_root,
                                         std::shared_ptr<const nimbus::common::Logger> logger)
    : data_root_(std::move(data_root)), logger_(std::move(logger)) {
    
    uint64_t count = 0;
    uint64_t bytes = 0;

    if (std::filesystem::exists(data_root_)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(data_root_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".chunk") {
                count++;
                bytes += entry.file_size();
            }
        }
    } else {
        std::filesystem::create_directories(data_root_);
    }

    chunk_count_.store(count, std::memory_order_relaxed);
    bytes_used_.store(bytes, std::memory_order_relaxed);
}

std::mutex& LocalDiskChunkStore::LockFor(const ChunkId& id) const {
    size_t hash_val = ChunkId::Hash{}(id);
    return stripe_locks_[hash_val % kLockStripeCount];
}

std::filesystem::path LocalDiskChunkStore::ShardDirFor(const ChunkId& id) const {
    const std::string& hex = id.ToHexString();
    return data_root_ / hex.substr(0, 2) / hex.substr(2, 2);
}

std::filesystem::path LocalDiskChunkStore::FinalPathFor(const ChunkId& id) const {
    return ShardDirFor(id) / (id.ToHexString() + ".chunk");
}

common::Result<std::monostate, ChunkStoreErrorDetail> LocalDiskChunkStore::Put(const ChunkId& id, std::span<const std::byte> data) {
    auto actual_hash = common::Sha256::HashBytes(data);
    if (actual_hash != id.RawDigest()) {
        logger_->Error("ChecksumMismatch on Put for chunk " + id.ToHexString());
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::ChecksumMismatch, "Provided chunk data does not match the ChunkId"});
    }

    std::lock_guard<std::mutex> lock(LockFor(id));

    if (Exists(id)) {
        logger_->Debug("Chunk already exists, deduplicating: " + id.ToHexString());
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
    }

    std::filesystem::path shard_dir = ShardDirFor(id);
    std::error_code ec;
    std::filesystem::create_directories(shard_dir, ec);
    if (ec) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to create shard directory: " + ec.message()});
    }

    std::filesystem::path tmp_path = shard_dir / (id.ToHexString() + ".tmp." + common::GenerateUuidV4());
    std::filesystem::path final_path = FinalPathFor(id);

    int oflag = O_WRONLY | O_CREAT | O_TRUNC;
#ifndef _WIN32
    oflag |= O_CLOEXEC;
#endif

    ScopedFd fd(internal_open(tmp_path.string().c_str(), oflag, 0644));
    if (fd.get() < 0) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to open temporary file for write"});
    }

    if (!data.empty()) {
#ifdef _WIN32
        int written = ::_write(fd.get(), data.data(), static_cast<unsigned int>(data.size()));
#else
        ssize_t written = ::write(fd.get(), data.data(), data.size());
#endif
        if (written < 0 || static_cast<size_t>(written) != data.size()) {
            std::filesystem::remove(tmp_path, ec);
            return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
                {ChunkStoreError::IoError, "Failed to write chunk data"});
        }
    }

    if (internal_fsync(fd.get()) != 0) {
        std::filesystem::remove(tmp_path, ec);
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to fsync temporary file"});
    }
    
#ifdef _WIN32
    ::_close(fd.release());
#else
    ::close(fd.release());
#endif

    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        std::filesystem::remove(tmp_path, ec);
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to atomically rename chunk file"});
    }

#ifndef _WIN32
    // Fsync directory (Unix specific for durability)
    ScopedFd dir_fd(internal_open(shard_dir.string().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (dir_fd.get() >= 0) {
        if (internal_fsync(dir_fd.get()) != 0) {
            logger_->Warn("Failed to fsync directory " + shard_dir.string());
        }
    } else {
        logger_->Warn("Failed to open directory for fsync " + shard_dir.string());
    }
#else
    // Windows doesn't fsync directories like POSIX
#endif

    chunk_count_.fetch_add(1, std::memory_order_relaxed);
    bytes_used_.fetch_add(data.size(), std::memory_order_relaxed);

    logger_->Debug("Successfully wrote chunk " + id.ToHexString());
    return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
}

common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> LocalDiskChunkStore::Get(const ChunkId& id) {
    std::lock_guard<std::mutex> lock(LockFor(id));

    std::filesystem::path final_path = FinalPathFor(id);
    std::error_code ec;
    uintmax_t file_size = std::filesystem::file_size(final_path, ec);
    if (ec) {
        return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::NotFound, "Chunk file not found or inaccessible"});
    }

    std::vector<std::byte> data(file_size);
    int oflag = O_RDONLY;
#ifndef _WIN32
    oflag |= O_CLOEXEC;
#endif
    ScopedFd fd(internal_open(final_path.string().c_str(), oflag));
    if (fd.get() < 0) {
        return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to open chunk file for read"});
    }

    if (file_size > 0) {
#ifdef _WIN32
        int bytes_read = ::_read(fd.get(), data.data(), static_cast<unsigned int>(file_size));
#else
        ssize_t bytes_read = ::read(fd.get(), data.data(), file_size);
#endif
        if (bytes_read < 0 || static_cast<size_t>(bytes_read) != file_size) {
            return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Err(
                {ChunkStoreError::IoError, "Failed to read full chunk file"});
        }
    }

    auto actual_hash = common::Sha256::HashBytes(data);
    if (actual_hash != id.RawDigest()) {
        logger_->Error("CorruptionDetected for chunk " + id.ToHexString());
        return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::CorruptionDetected, "Chunk data on disk does not match its ID"});
    }

    logger_->Debug("Successfully read chunk " + id.ToHexString());
    return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Ok(std::move(data));
}

common::Result<std::monostate, ChunkStoreErrorDetail> LocalDiskChunkStore::VerifyOnly(const ChunkId& id) {
    auto res = Get(id);
    if (!res.IsOk()) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(res.Error());
    }
    return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
}

common::Result<std::monostate, ChunkStoreErrorDetail> LocalDiskChunkStore::Delete(const ChunkId& id) {
    std::lock_guard<std::mutex> lock(LockFor(id));

    std::filesystem::path final_path = FinalPathFor(id);
    std::error_code ec;
    uintmax_t size_before = std::filesystem::file_size(final_path, ec);
    bool existed = !ec;

    if (!existed) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
    }

    if (!std::filesystem::remove(final_path, ec) || ec) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err(
            {ChunkStoreError::IoError, "Failed to delete chunk file: " + ec.message()});
    }

    chunk_count_.fetch_sub(1, std::memory_order_relaxed);
    bytes_used_.fetch_sub(size_before, std::memory_order_relaxed);

    logger_->Debug("Successfully deleted chunk " + id.ToHexString());
    return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
}

bool LocalDiskChunkStore::Exists(const ChunkId& id) {
    std::lock_guard<std::mutex> lock(LockFor(id));
    std::error_code ec;
    return std::filesystem::exists(FinalPathFor(id), ec) && !ec;
}

ChunkStoreStats LocalDiskChunkStore::GetStats() {
    std::error_code ec;
    auto space = std::filesystem::space(data_root_, ec);
    uint64_t free = ec ? 0 : space.available;

    return {
        chunk_count_.load(std::memory_order_relaxed),
        bytes_used_.load(std::memory_order_relaxed),
        free
    };
}

std::vector<ChunkId> LocalDiskChunkStore::ListAllChunkIds() {
    std::vector<ChunkId> result;
    if (!std::filesystem::exists(data_root_)) {
        return result;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(data_root_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".chunk") {
            std::string hex = entry.path().stem().string();
            auto id_res = ChunkId::Parse(hex);
            if (id_res.IsOk()) {
                result.push_back(id_res.Value());
            }
        }
    }
    return result;
}

}  // namespace nimbus::storage_node
