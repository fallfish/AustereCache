#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <>

#include "device.h"
#include "utils/MurmurHash3.h"

namespace cache {
  Device::~Device() {}

  int BlockDevice::open(char *filename, uint32_t size)
  {
    struct stat stat_buf;
    int handle = ::stat(filename, &stat_buf);
    if (handle == -1) {
      if (errno == ENOENT) {
        open_new_device(filename, size);
      } else {
        // unexpected error
        return -1;
      }
    } else {
      open_existing_device(filename);
    }
  }

  int BlockDevice::write(uint8_t* buf, uint32_t len, uint32_t offset)
  {
    assert(offset % 512 == 0);
    assert(len % 512 == 0);
    //::write(_handler, buf, len);
    memcpy((uint8_t*)_device_buffer + offset, buf, len);
    //msync(_device_buffer, len, MS_SYNC);
  }

  int BlockDevice::read(uint8_t* buf, uint32_t len, uint32_t offset)
  {
    assert(offset % 512 == 0);
    assert(len % 512 == 0);
    memcpy(buf, (uint8_t*)_device_buffer + offset, len);
    //msync(_device_buffer, len, MS_SYNC);
  }

  BlockDevice::~BlockDevice()
  {
    close(_handler);
  }

  int BlockDevice::open_new_device(char *filename, uint32_t size)
  {
    int fid = 0;
    if (size == 0) {
      // error: new file needs to be created, however the size given is 0.
      return -1;
    }
    fid = ::open(filename, O_RDWR | O_DIRECT | O_CREAT, 0666);
    if (fid < 0) {
      // cannot create device with O_DIRECT
      fid = ::open(filename, O_RDWR | O_DIRECT | O_CREAT, 0666);
      if (fid < 0) {
        // cannot open the file
        return -1;
      }
    }
    if (::ftruncate64(fid, size) < 0) {
      // cannot truncate the fid into the given size
      return -1;
    }
    _handler = fid;
    _size = size;
  }

  int BlockDevice::open_existing_device(char *filename, uint32_t size, struct statbuf *statbuf)
  {
    int fid = 0;
    fid = ::open(filename, O_RDWR | O_DIRECT);
    if (fid < 0) {
      // cannot open device with O_DIRECT;
      fid = ::open(filename, O_RDWR);
      if (fid < 0) {
        // cannot open file
        return -1;
      }
    }

    if (size == 0) {
      if (S_ISREG(statbuf->st_mode))
        size = statbuf->st_size;
      else
        size = get_size(fid);
    }
    _size = size;
    _handler = fid;
  }

  uint32_t BlockDevice::get_size(int fid)
  {
    uint32_t offset = 1024;
    uint32_t prev_offset = 0;
    char buffer[1024];
    char *read_buffer = (char*)((long)buffer + 511) & ~511;

    while (1) {
      if (lseek(fid, offset, SEEK_SET) == -1) {
        offset -= (offset - prev_offset) / 2;
        continue;
      }
      int nr = 0;
      if ( (nr = read(fid, buffer, 512)) != 512 ) {
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
    return 0;
  }
}
