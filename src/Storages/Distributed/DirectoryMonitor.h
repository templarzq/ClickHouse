#pragma once

#include <Core/BackgroundSchedulePool.h>
#include <Client/ConnectionPool.h>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <IO/ReadBufferFromFile.h>


namespace CurrentMetrics { class Increment; }

namespace DB
{

class IDisk;
using DiskPtr = std::shared_ptr<IDisk>;

class StorageDistributed;
class ActionBlocker;
class BackgroundSchedulePool;

/** Details of StorageDistributed.
  * This type is not designed for standalone use.
  */
class StorageDistributedDirectoryMonitor
{
public:
    StorageDistributedDirectoryMonitor(
        StorageDistributed & storage_,
        const DiskPtr & disk_,
        const std::string & relative_path_,
        ConnectionPoolPtr pool_,
        ActionBlocker & monitor_blocker_,
        BackgroundSchedulePool & bg_pool);

    ~StorageDistributedDirectoryMonitor();

    static ConnectionPoolPtr createPool(const std::string & name, const StorageDistributed & storage);

    void updatePath(const std::string & new_relative_path);

    void flushAllData();

    void shutdownAndDropAllData();

    static BlockInputStreamPtr createStreamFromFile(const String & file_name);

    /// For scheduling via DistributedBlockOutputStream
    bool scheduleAfter(size_t ms);

    /// system.distribution_queue interface
    struct Status
    {
        std::string path;
        std::exception_ptr last_exception;
        size_t error_count;
        size_t files_count;
        size_t bytes_count;
        bool is_blocked;
    };
    Status getStatus() const;

private:
    void run();

    std::map<UInt64, std::string> getFiles();
    bool processFiles(const std::map<UInt64, std::string> & files);
    void processFile(const std::string & file_path);
    void processFilesWithBatching(const std::map<UInt64, std::string> & files);

    void markAsBroken(const std::string & file_path) const;
    bool maybeMarkAsBroken(const std::string & file_path, const Exception & e) const;

    std::string getLoggerName() const;

    StorageDistributed & storage;
    const ConnectionPoolPtr pool;

    DiskPtr disk;
    std::string relative_path;
    std::string path;

    const bool should_batch_inserts = false;
    const bool dir_fsync = false;
    const size_t min_batched_block_size_rows = 0;
    const size_t min_batched_block_size_bytes = 0;
    String current_batch_file_path;

    struct BatchHeader;
    struct Batch;

    mutable std::mutex metrics_mutex;
    size_t error_count = 0;
    size_t files_count = 0;
    size_t bytes_count = 0;
    std::exception_ptr last_exception;

    const std::chrono::milliseconds default_sleep_time;
    std::chrono::milliseconds sleep_time;
    const std::chrono::milliseconds max_sleep_time;
    std::chrono::time_point<std::chrono::system_clock> last_decrease_time {std::chrono::system_clock::now()};
    std::atomic<bool> quit {false};
    std::mutex mutex;
    Poco::Logger * log;
    ActionBlocker & monitor_blocker;

    BackgroundSchedulePoolTaskHolder task_handle;

    CurrentMetrics::Increment metric_pending_files;

    friend class DirectoryMonitorBlockInputStream;
};

}
