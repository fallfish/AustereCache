#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

#include "device/device.h"
#include "utils/MurmurHash3.h"

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
    //for (auto addr = offset; addr < offset + len; addr += 4096) {
      //uint32_t _len = offset - addr;

      //uint8_t content_addr[16];
      //uint32_t lba_hash, ca_hash, lba_key, ca_key;
      //bool non_dup = true;
      //{
        //// compute fingerprint (content address),
        //// lba address hash value, and ca hash value
        //MurmurHash3_x64_128(buf + addr, 512, 0x4f3f2f1f, content_addr);
        //MurmurHash3_x86_32(&addr, 4, 0x3f2f1f4f, &lba_hash);
        //MurmurHash3_x86_32(content_addr, 16, 0x2f1f4f3f, &ca_hash);
        //lba_key = lba_hash & ((1 << 12) - 1) | (lba_hash >> 10);
        //ca_key = ca_hash & ((1 << 12) - 1) | (ca_hash >> 10);
      //}

      //uint32_t old_ca_key, cache_device_data_pointer;
      //bool lba_index_hit = false, ca_index_hit = false;

      //// lookup lba index and possiblely update old entry
      //lba_index_hit = _lba_index->lookup(&lba_key, (uint8_t*)&old_ca_key);
      //if (lba_index_hit) {
        //if (old_ca_key != ca_key)
          //non_dup = true;
      //}

      //ca_index_hit = _ca_index->lookup(&ca_key, (uint8_t*)&cache_device_data_pointer);
      //if (ca_index_hit) {
        //// retrive metadata from cache device
        //char metadata[512];
        //_cache_device->read(cache_device_data_pointer, 512, metadata);
        //// verify content address and lbas
        //if (verify_ca_and_lba()) {
          //non_dup = true;
        //}
      //}

      //if (non_dup) {
        //// update indexing
        //_lba_index->set(&lba_key, &ca_key);
        //_ca_index->set(&ca_key, &cache_device_data_pointer);
        //// construct metadata and write data to ssd
        //char metadata[512];
        //construct_metadata(metadata);
        //_cache_device->write(cache_device_data_pointer, 512, metadata);
        //_cache_device->write(cache_device_data_pointer + 512, _len, buf + addr - offset);
        //// write to hdd
        //_primary_device->write(addr, _len, buf + addr - offset);
      //}

    //}

  }

  void DeviceWithCache::read(uint32_t offset, uint32_t len, uint8_t* buf)
  {
    //for (uint32_t addr = offset; addr < offset + len; addr += 4096) {
      //uint32_t lba_hash, lba_key, ca_key;
      //bool _lba_index_hit = false, _ca_index_hit = false;
      //{
        //MurmurHash3_x86_32(&addr, 4, 0x3f2f1f4f, &lba_hash);
        //lba_key = lba_hash & ((1 << 12) - 1) | (lba_hash >> 10);
      //}

      //_lba_index_hit = _lba_index->lookup(&lba_key, &ca_key);
      //if (_lba_index_hit) {
        //_ca_index_hit = _ca_index->lookup(&ca_index, &cache_device_data_pointer);
        //if (_ca_index_hit) {
          //// retrive data from cache_device
        //}
      //}

      //if (!_ca_index_hit) {
        //// retrive data from primary device
      //}
    //}
  }
}
