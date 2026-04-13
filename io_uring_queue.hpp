#pragma once
#include <liburing.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "debug_macros.hpp"

class IoUringQueue {
private:
  struct io_uring ring_;
  int fd_;
  off_t offset_;
  std::string file_name;

public:
  explicit IoUringQueue(const std::string &filename, unsigned entries = 256) {
    int ret = io_uring_queue_init(entries, &ring_, 0);
    if (ret < 0)
      throw std::runtime_error("io_uring_queue_init failed");
    this->file_name = filename;
    fd_ = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
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

  template <typename T> void batchWrite(const std::vector<T> &data) {
    if (data.empty()) return;

    struct stat st;
    if (fstat(fd_, &st) == 0 && st.st_nlink == 0) {
        // HFT_DEBUG_FILE("batch.txt", "[IoUringQueue] File unlinked! Re-opening " + file_name);
        int new_fd = open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (new_fd >= 0) {
            close(fd_);
            fd_ = new_fd;
            offset_ = lseek(fd_, 0, SEEK_END);
        }
    }

    size_t bytesToWrite = data.size() * sizeof(T);
    auto *sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        if (pwrite(fd_, data.data(), bytesToWrite, offset_) == -1) {
            // HFT_DEBUG_FILE("batch.txt", "[IoUringQueue] pwrite fallback failed");
        }
        offset_ += bytesToWrite;
        return;
    }

    io_uring_prep_write(sqe, fd_, data.data(), bytesToWrite, offset_);
    offset_ += bytesToWrite;
    
    int ret = io_uring_submit(&ring_);
    if (ret < 0) {
        // HFT_DEBUG_FILE("batch.txt", "[IoUringQueue] io_uring_submit failed");
        return;
    }
    wait_all(1);
    fdatasync(fd_);
  }
  void batchWrite(const std::vector<std::string> &data);

private:
  void wait_all(size_t expected, size_t expected_bytes = 0);
  void submit_and_drain();
};
