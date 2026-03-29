#pragma once
#include "databaseSchemaReader.hpp"
#include <format>
#include "global.hpp"
#include "hft.hpp"
#include "json.hpp"
#include "utility.hpp"
#include "utils/types.hpp"
#include <charconv>
#include <cstdint>
#include <fcntl.h>
#include <liburing.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>



class IoUringQueue {
private:
  struct io_uring ring_;
  int fd_;
  off_t offset_;

public:
  explicit IoUringQueue(const std::string &filename, unsigned entries = 256) {
    int ret = io_uring_queue_init(entries, &ring_, 0);
    if (ret < 0)
      throw std::runtime_error("io_uring_queue_init failed");

    fd_ = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) {
      io_uring_queue_exit(&ring_);
      throw std::runtime_error("open failed");
    }

    offset_ = lseek(fd_, 0, SEEK_END);
    if (offset_ < 0) {
      close(fd_);
      io_uring_queue_exit(&ring_);
      throw std::runtime_error("lseek failed");
    }
  }

  ~IoUringQueue() {
    close(fd_);
    io_uring_queue_exit(&ring_);
  }

private:
  void wait_all(size_t expected, size_t expected_bytes = 0) {
    for (size_t i = 0; i < expected; i++) {
      io_uring_cqe *cqe;
      int r = io_uring_wait_cqe(&ring_, &cqe);
      if (r < 0)
        throw std::runtime_error("wait_cqe failed");

      if (cqe->res < 0)
        throw std::runtime_error("write failed");

      if (expected_bytes > 0 && static_cast<size_t>(cqe->res) != expected_bytes)
        throw std::runtime_error("partial write detected");

      io_uring_cqe_seen(&ring_, cqe);
    }
  }

  void submit_and_drain() {
    int ret = io_uring_submit(&ring_);
    if (ret < 0)
      throw std::runtime_error("submit failed");
    wait_all(static_cast<size_t>(ret));
  }

public:
  template <typename T>
  void batchWrite(const std::vector<T> &data) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

    if (data.empty())
      return;

    size_t submitted = 0;
    size_t pending = 0;

    while (submitted < data.size()) {
      auto *sqe = io_uring_get_sqe(&ring_);
      if (!sqe) {
        int ret = io_uring_submit(&ring_);
        if (ret < 0)
          throw std::runtime_error("submit failed");
        wait_all(static_cast<size_t>(ret));
        pending = 0;
        continue;
      }

      io_uring_prep_write(sqe, fd_, &data[submitted], sizeof(T), offset_);
      offset_ += sizeof(T);
      submitted++;
      pending++;
    }

    int ret = io_uring_submit(&ring_);
    if (ret < 0)
      throw std::runtime_error("submit failed");
    wait_all(static_cast<size_t>(ret));
  }

  void batchWrite(const std::vector<std::string> &data) {
    if (data.empty())
      return;

    size_t submitted = 0;
    size_t pending = 0;

    while (submitted < data.size()) {
      auto *sqe = io_uring_get_sqe(&ring_);
      if (!sqe) {
        int ret = io_uring_submit(&ring_);
        if (ret < 0)
          throw std::runtime_error("submit failed");
        wait_all(static_cast<size_t>(ret));
        pending = 0;
        continue;
      }

      const std::string &s = data[submitted];
      io_uring_prep_write(sqe, fd_, s.data(), s.size(), offset_);
      offset_ += s.size();
      submitted++;
      pending++;
    }

    int ret = io_uring_submit(&ring_);
    if (ret < 0)
      throw std::runtime_error("submit failed");
    wait_all(static_cast<size_t>(ret));
  }
};



namespace BatchWriter {

  static constexpr int64_t batchWriterQueueSize = 1024;
struct alignas(CACHELINE) batchWriterPacket{
    int64_t symbol;
    std::vector<int64_t>data;
};

// SPSCQueue<batchWriterPacket, batchWriterQueueSize> batchWriterPacketQueue;



void parseEnableNatchStatement(std::unique_ptr<EnableStatement> &&statement) {
 std::string currentDb = dbDirectoryPath + "/" + currentDatabase +  ".shivam" + ".db";
if (!MyUtility::checkIfFileExist(currentDb)) {
    throw std::runtime_error(
        std::format("the file {} does not exist", currentDb));
}

std::shared_ptr<JSONParser> parser = std::make_shared<JSONParser>();

if (!parser->loadFromFile(currentDb)) {
    std::cerr << "Failed to load file: " << currentDb << std::endl;
    throw std::runtime_error("Failed to load file: " + currentDb);
}

int ticks = statement->ticks;

JSONParser::JSONValue rootValue = parser->getObject(0);
JSONParser::JSONObject& rootObj = std::get<JSONParser::JSONObject>(rootValue.value);

JSONParser::JSONArray& tablesArray = std::get<JSONParser::JSONArray>(rootObj["tables"].value);

for (auto& tableVal : tablesArray) {
    JSONParser::JSONObject& table = std::get<JSONParser::JSONObject>(tableVal.value);
    
    std::string tableName = std::get<std::string>(table["name"].value);

    int64_t symbol = -1;
    auto symbolIt = table.find("symbol");
    if (symbolIt != table.end() && !std::holds_alternative<std::nullptr_t>(symbolIt->second.value)) {
        if (std::holds_alternative<int>(symbolIt->second.value)) {
            symbol = std::get<int>(symbolIt->second.value);
        } else if (std::holds_alternative<std::string>(symbolIt->second.value)) {
            symbol = std::stoll(std::get<std::string>(symbolIt->second.value));
        } else if (std::holds_alternative<double>(symbolIt->second.value)) {
            symbol = static_cast<int64_t>(std::get<double>(symbolIt->second.value));
        } else {
            throw std::runtime_error("Invalid type for symbol in JSON");
        }
    }
    std::cout<<"THE SYMBOL IS " << symbol<<"\n";
    if (symbol != -1) {
        std::cout<<"storage symbol and ticks is "<< symbol <<" "<<ticks<<"\n";
        HFT::symbolAccessArray[symbol].storageTicks = ticks;
    }

    if (tableName == statement->tableName) {
        
        table["ticks"] = JSONParser::JSONValue(ticks);
        break;
    }
}

parser->removeObject(0);
parser->appendValue(rootValue);
parser->saveToFile(currentDb);

}

bool writeHFTDataToIndexFile(int symbol){
    auto it = batchWriterFileMap.find(symbol);
    auto *__restrict entry = &HFT::symbolAccessArray[symbol];

    entry->count++;
    if(UNLIKELY(entry->count == entry->storageTicks)){
      entry->count = 0;
       if (it != batchWriterFileMap.end() && it->second) {
        std::vector data = HFT::symbolAccessArray[symbol].getWritingData();
        it->second->batchWrite(data);
        return true;
    }
    }

   

    std::cout<<" the symbol does not exist "<<symbol<<"\n";
    return false;
}

}; // namespace BatchWriter