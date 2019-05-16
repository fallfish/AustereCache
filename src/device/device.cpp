#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

#include "device/device.h"
#include "hashing/MurmurHash3.h"

namespace cache {
  Device::~Device() {}

  void BlockDevice::open(char *filename)
  {
    _handler = ::open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    ftruncate(_handler, 1024 * 1024);
    _device_buffer = ::mmap((void*)0, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, _handler, 0);
  }

  void BlockDevice::write(uint32_t offset, uint32_t len, uint8_t* buf)
  {
    assert(offset % 512 == 0);
    assert(len % 512 == 0);
    //::write(_handler, buf, len);
    memcpy((uint8_t*)_device_buffer + offset, buf, len);
    //msync(_device_buffer, len, MS_SYNC);
  }

  void BlockDevice::read(uint32_t offset, uint32_t len, uint8_t* buf)
  {
    assert(offset % 512 == 0);
    assert(len % 512 == 0);
    memcpy(buf, (uint8_t*)_device_buffer + offset, len);
    //msync(_device_buffer, len, MS_SYNC);
  }
  BlockDevice::~BlockDevice()
  {
    munmap(_device_buffer, 1024 * 1024);
    close(_handler);
  }

  DeviceWithCache::DeviceWithCache(
      std::unique_ptr<Device> primary_device,
      std::unique_ptr<Device> cache_device,
      std::unique_ptr<LBAIndex> lba_index,
      std::unique_ptr<CAIndex> ca_index
      ) :
    _primary_device(std::move(primary_device)),
    _cache_device(std::move(cache_device)),
    _lba_index(std::move(lba_index)),
    _ca_index(std::move(ca_index))
  {
  }

  /*
   *
   */
  void DeviceWithCache::write(uint32_t offset, uint32_t len, uint8_t *buf)
  {
    // process unaligned part
    // assume all address are aligned first
    {
    }


    // Lookup lba index
    for (auto addr = offset; addr < offset + len; addr += 512) {
      uint8_t content_addr[16];
      uint32_t lba_hash, ca_hash;
      {
        // compute fingerprint (content address),
        // lba address hash value, and ca hash value
        MurmurHash3_x64_128(buf + addr, 512, 0x4f3f2f1f, content_addr);
        MurmurHash3_x86_32(&addr, 4, 0x3f2f1f4f, &lba_hash);
        MurmurHash3_x86_32(content_addr, 16, 0x2f1f4f3f, &ca_hash);
      }

      //uint32_t ca_key, ssd_data_pointer;
      //bool lba_index_hit = false, ca_index_hit = false;

      lba_index_hit = _lba_index->lookup(&lba_hash, (uint8_t*)&ca_key);
      if (lba_index_hit && 
          ( ca_hash & ((1 << 12) - 1) 
            == ca_key & ((1 << 12) - 1)
            // verify that the ca_signature in the lba_index matches current content
          ) {
        ca_index_hit = _ca_index->lookup(ca_key, (uint8_t*)&ssd_data_pointer);
      }

      //// has not been cached
      if (!ca_index_hit) {
        _lba_index->set();
      } else {
        char metadata[512];
        _cache_device->read(ssd_data_pointer, 512, metadata);
        verify_metadata(metadata, lba, content_addr);
      }
    }

  }

  void DeviceWithCache::read(uint32_t offset, uint32_t len, uint8_t* buf)
  {
  }
}
