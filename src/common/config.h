#ifndef __CONFIG_H__
#define __CONFIG_H__ 
#include <cstdint>
namespace cache {

class Config
{
 public:
  static Config& get_configuration() {
    static Config instance;
    return instance;
  }

  uint32_t get_chunk_size() { return _chunk_size; }
  uint32_t get_sector_size() { return _sector_size; }
  uint32_t get_metadata_size() { return _metadata_size; }
  uint32_t get_ca_length() { return _ca_length; }
  uint64_t get_primary_device_size() { return _primary_device_size; }
  uint64_t get_cache_device_size() { return _cache_device_size; }

  uint32_t get_lba_signature_len() { return _lba_signature_len; }
  uint32_t get_lba_bucket_no_len() { return _lba_bucket_no_len; }
  uint32_t get_lba_slots_per_bucket() { return _lba_slots_per_bucket; }
  uint32_t get_ca_signature_len() { return _ca_signature_len; }
  uint32_t get_ca_bucket_no_len() { return _ca_bucket_no_len; }
  uint32_t get_ca_slots_per_bucket() { return _ca_slots_per_bucket; }

  void set_chunk_size(uint32_t chunk_size) { _chunk_size = chunk_size; }
  void set_sector_size(uint32_t sector_size) { _sector_size = sector_size; }
  void set_metadata_size(uint32_t metadata_size) { _metadata_size = metadata_size; }
  void set_ca_length(uint32_t ca_length) { _ca_length = ca_length; }
  void set_primary_device_size(uint64_t primary_device_size) { _primary_device_size = primary_device_size; }
  void set_cache_device_size(uint64_t cache_device_size) { _cache_device_size = cache_device_size; }

  void set_lba_signature_len(uint32_t lba_signature_len) { _lba_signature_len = lba_signature_len; }
  void set_lba_bucket_no_len(uint32_t lba_bucket_no_len) { _lba_bucket_no_len = lba_bucket_no_len; }
  void set_lba_slots_per_bucket(uint32_t lba_slots_per_bucket) { _lba_slots_per_bucket = lba_slots_per_bucket; }
  void set_ca_signature_len(uint32_t ca_signature_len) { _ca_signature_len = ca_signature_len; }
  void set_ca_bucket_no_len(uint32_t ca_bucket_no_len) { _ca_bucket_no_len = ca_bucket_no_len; }
  void set_ca_slots_per_bucket(uint32_t ca_slots_per_bucket) { _ca_slots_per_bucket = ca_slots_per_bucket; }


 private:
  Config() {
    // Initialize default configuration
    // Conf format from caller to be imported 
    _chunk_size = 8192 * 4;
    _sector_size = 8192;
    _metadata_size = 512;
    _ca_length = 16;
    _primary_device_size = 1024 * 1024 * 512;
    _cache_device_size = 1024 * 1024 * 300;

    // Each bucket has 32 slots. Each index has _n_buckets buckets,
    // Each slot represents one chunk 32K.
    // To store all lbas without eviction
    // _n_buckets = primary_storage_size / 32K / 32 = 512
    _lba_signature_len = 12;
    _lba_bucket_no_len = 9;
    _lba_slots_per_bucket = 32;
    _ca_signature_len = 12;
    _ca_bucket_no_len = 10;
    _ca_slots_per_bucket = 32;
  }
  uint32_t _chunk_size; // 8k size chunk
  uint32_t _sector_size; // 8k size sector
  uint32_t _metadata_size; // 512 byte size chunk
  uint32_t _ca_length; // content address length, 256 bytes if using sha256, else differs for different hash function
  uint64_t _primary_device_size;
  uint64_t _cache_device_size;

  // Each bucket has 32 slots. Each index has _n_buckets buckets,
  // Each slot represents one chunk 32K.
  // To store all lbas without eviction
  // _n_buckets = primary_storage_size / 32K / 32 = 512
  uint32_t _lba_signature_len;
  uint32_t _lba_bucket_no_len;
  uint32_t _lba_slots_per_bucket;
  uint32_t _ca_signature_len;
  uint32_t _ca_bucket_no_len;
  uint32_t _ca_slots_per_bucket;

};

}

#endif
