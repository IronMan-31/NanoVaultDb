#include "global.hpp"

// Definitions of global variables
std::unordered_map<std::string, std::shared_ptr<PythonLikeJSONParser>> globalJsonCache;
std::unordered_map<std::string, MemoryEntry> memoryStore;
std::shared_mutex memoryMutex;
std::unordered_map<std::string, std::mutex> tableLocks;
std::unordered_map<int64_t, std::unique_ptr<IoUringQueue>> batchWriterFileMap;
std::mutex dbMutex;

std::atomic<bool> shuttingDown{false};
std::thread vacuumThread;

std::priority_queue<ExpiryNode, std::vector<ExpiryNode>, std::greater<>> expiryHeap;

std::mutex expiryMutex;
std::condition_variable expiryCV;
std::atomic<bool> memorySchedulerRunning{true};
SPSCQueue<web_socket_Packet, 1024> web_socket_queue;

std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::pair<TreeVariant, int64_t>>>> dbBtrees;

std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, std::vector<std::shared_ptr<TableGlobalColumnNode>>>>> globalTableCache;

std::unordered_map<std::string, std::string> API;
