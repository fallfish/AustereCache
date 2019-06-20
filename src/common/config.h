#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdint>
class Config
{
 public:
  static const uint32_t chunk_size = 8192 * 4; // 8k size chunk
  static const uint32_t sector_size = 8192; // 8k size sector
  static const uint32_t metadata_size = 512; // 512 byte size chunk
  static const uint32_t ca_length = 16; // content address length, 256 bytes if using sha256, else differs for different hash function
  static const uint64_t primary_device_size = 1024 * 1024 * 512;
  static const uint64_t cache_device_size = 1024 * 1024 * 300;

  // Each bucket has 32 slots. Each index has _n_buckets buckets,
  // Each slot represents one chunk 32K.
  // To store all lbas without eviction
  // _n_buckets = primary_storage_size / 32K / 32 = 512
  static const uint32_t lba_signature_len = 12;
  static const uint32_t lba_bucket_no_len = 9;
  static const uint32_t ca_signature_len = 12;
  static const uint32_t ca_bucket_no_len = 10;

};

#endif
