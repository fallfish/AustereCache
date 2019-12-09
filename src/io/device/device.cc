#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <csignal>

#include "common/env.h"
#include "common/config.h"
#include "utils/utils.h"
#include "device.h"
#include "utils/MurmurHash3.h"

namespace cache {
  Device::~Device() {
    close(_fd);
  }

  BlockDevice::BlockDevice() {}

  int BlockDevice::open(char *filename, uint64_t size)
  {
    struct stat stat_buf;
    int handle = ::stat(filename, &stat_buf);
    if (handle == -1) {
      if (errno == ENOENT) {
        return open_new_device(filename, size);
      } else {
        // unexpected error
        return -1;
      }
    } else {
      return open_existing_device(filename, size, &stat_buf);
    }
  }

  int BlockDevice::write(uint64_t addr, uint8_t* buf, uint32_t len)
  {
    assert(addr % 512 == 0);
    assert(len % 512 == 0);
    if (addr + len > _size) {
      len -= addr + len - _size;
    }
    if (Config::getInstance().isFakeIOEnabled()) {
      if (len != 512)
        return len;
    }

    int n_written_bytes = 0;
    while (1) {
      int n = ::pwrite(_fd, buf, len, addr);
      if (n < 0) {
        std::cout << "addr: " << addr << " len: " << len << std::endl;
        std::cout << "BlockDevice::write " << std::strerror(errno) << std::endl;
        exit(-1);
      }
      n_written_bytes += n;
      if (n == len) {
        break;
      } else {
        buf += n;
        len -= n;
      }
    }
    return n_written_bytes;
  }

  int BlockDevice::read(uint64_t addr, uint8_t* buf, uint32_t len)
  {
#if !defined(CDARC)
    assert(addr % 512 == 0);
    assert(len % 512 == 0);
#endif

#if defined(CDARC)
    // if enabled direct io, the request must be aligned with the disk
    uint64_t realAddr = addr, realLen = len;
    if (Config::getInstance().isDirectIOEnabled()) {
      addr = addr - addr % 512;
      len = (len + 511) / 512 * 512;
    }
#endif

    if (addr + len > _size) {
      len -= addr + len - _size;
    }

    if (Config::getInstance().isFakeIOEnabled()) {
      if (len != 512)
        return len;
    }

    int n_read_bytes = 0;
    while (1) {
      int n = ::pread(_fd, buf, len, addr);
      if (n < 0) {
        std::cout << (long)buf << " " << addr << " " << len << std::endl;
        std::cout << "BlockDevice::read " << std::strerror(errno) << std::endl;
        exit(-1);
      }
      n_read_bytes += n;
      if (n == len) {
        break;
      } else {
        buf += n;
        len -= n;
      }
    }
#if defined(CDARC)
    // if enabled direct io, the request must be aligned with the disk
    if (Config::getInstance().isDirectIOEnabled()) {
      memmove(buf, buf + realAddr - addr, realLen);
      n_read_bytes = realLen;
    }
#endif
    return n_read_bytes;
  }

  BlockDevice::~BlockDevice()
  {
    close(_fd);
  }

  void BlockDevice::sync()
  {
    ::syncfs(_fd);
  }

  int BlockDevice::open_new_device(char *filename, uint64_t size)
  {
    std::cout << "BlockDevice::Open new device!" << std::endl;
    int fd = 0;
    if (size == 0) {
      // error: new file needs to be created, however the size given is 0.
      return -1;
    }
    if (Config::getInstance().isDirectIOEnabled())
      fd = ::open(filename, O_RDWR | O_DIRECT | O_CREAT, 0666);
    else
      fd = ::open(filename, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      // cannot create device with O_DIRECT
      fd = ::open(filename, O_RDWR | O_CREAT, 0666);
      if (fd < 0) {
        // cannot open the file
        return -1;
      }
    }
    if (::ftruncate(fd, size) < 0) {
      // cannot truncate the file into the given size
      return -1;
    }
    _fd = fd;
    _size = size;
    return 0;
  }

  int BlockDevice::open_existing_device(char *filename, uint64_t size, struct stat *statbuf)
  {
    int fd = 0;
    std::cout << "BlockDevice::Open existing device!" << std::endl;
    if (Config::getInstance().isDirectIOEnabled())
      fd = ::open(filename, O_RDWR | O_DIRECT);
    else
      fd = ::open(filename, O_RDWR);

    if (fd < 0) {
      std::cout << "No O_DIRECT!" << std::endl;
      // cannot open device with O_DIRECT;
      fd = ::open(filename, O_RDWR);
      if (fd < 0) {
        // cannot open file
        return -1;
      }
    }

    if (size == 0) {
      if (S_ISREG(statbuf->st_mode))
        size = statbuf->st_size;
      else
        size = get_size(fd);
    }
    _size = size;
    _fd = fd;
    return 0;
  }

  int BlockDevice::get_size(int fd)
  {
    uint32_t offset = 1024;
    uint32_t prev_offset = 0;
    char buffer[1024];
    char *read_buffer = (char*)(((long)buffer + 511) & ~511);

    while (1) {
      if (::lseek(fd, offset, SEEK_SET) == -1) {
        offset -= (offset - prev_offset) / 2;
        continue;
      }
      int nr = 0;
      if ( (nr = ::read(fd, read_buffer, 512)) != 512 ) {
        if (nr >= 0) {
          offset += nr;
          return offset;
        } else {
          if (prev_offset == offset)
            return offset;
          else
            offset -= (offset - prev_offset) / 2;
        }
      } else {
        prev_offset = offset;
        offset *= 2;
      }
    }
  }

//  MemoryBlockDevice::MemoryBlockDevice() {}
}
