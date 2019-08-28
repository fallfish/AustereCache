#ifndef __CONFIG_H__
#define __CONFIG_H__ 
#include <cstdint>
#include <cstring>
#include <cstdlib>
namespace cache {

class Config
{
 public:
  static Config& get_configuration() {
    static Config instance;
    return instance;
  }

  // getters
  uint32_t get_chunk_size() { return _chunk_size; }
  uint32_t get_sector_size() { return _sector_size; }
  uint32_t get_metadata_size() { return _metadata_size; }
  uint32_t get_ca_length() { return _ca_length; }
  uint32_t get_strong_ca_length() { return _strong_ca_length; }
  uint64_t get_primary_device_size() { return _primary_device_size; }
  uint64_t get_cache_device_size() { return _cache_device_size; }

  uint32_t get_lba_signature_len() { return _lba_signature_len; }
  uint32_t get_lba_bucket_no_len() { return _lba_bucket_no_len; }
  uint32_t get_lba_slots_per_bucket() { return _lba_slots_per_bucket; }
  uint32_t get_ca_signature_len() { return _ca_signature_len; }
  uint32_t get_ca_bucket_no_len() { return _ca_bucket_no_len; }
  uint32_t get_ca_slots_per_bucket() { return _ca_slots_per_bucket; }

  uint32_t get_max_num_global_threads() { return _max_num_global_threads; }
  uint32_t get_max_num_local_threads() { return _max_num_local_threads; }

  char *get_cache_device_name() { return _cache_device_name; }
  char *get_primary_device_name() { return _primary_device_name; }
  bool get_direct_io() { return _direct_io; }

  uint32_t get_fingerprint_algorithm() { return _fingerprint_algorithm; }
  uint32_t get_fingerprint_computation_method() { return _fingerprint_computation_method; }

  uint32_t get_write_buffer_size() { return _buffered_write_buffer_size; }
  
  bool get_multi_thread() { return _multi_thread; }


  // setters
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

  void set_max_num_global_threads(uint32_t max_num_global_threads) { _max_num_global_threads = max_num_global_threads; }
  void set_max_num_local_threads(uint32_t max_num_local_threads) { _max_num_local_threads = max_num_local_threads; }

  void set_cache_device_name(char *cache_device_name) { _cache_device_name = cache_device_name; }
  void set_primary_device_name(char *primary_device_name) { _primary_device_name = primary_device_name; }
  bool set_direct_io(bool v) { _direct_io = v; }

  void set_write_buffer_size(uint32_t v) { _buffered_write_buffer_size = v; }
  void set_multi_thread(bool v) { _multi_thread = v; }

  void set_fingerprint_algorithm(uint32_t v) { 
    if (v == 0) set_ca_length(20);
    else set_ca_length(16);
    _fingerprint_algorithm = v; 
  }
  void set_fingerprint_computation_method(uint32_t v) {
    if (v == 0) set_fingerprint_algorithm(0);
    else set_fingerprint_algorithm(1);
    _fingerprint_computation_method = v;
  }
  char *get_current_fingerprint() {
    return _current_fingerprint;
  }
  void set_current_fingerprint(char *fingerprint) {
    memcpy(_current_fingerprint, fingerprint, _ca_length);
  }
  char *get_current_data() {
    return _current_data;
  }
  void set_current_data(char *data, int len) {
    if (len > _chunk_size) len = _chunk_size;
    memcpy(_current_data, data, len);
  }
  int get_current_compressed_len() {
    return _current_compressed_len;
  }
  void set_current_compressed_len(int compressed_len) {
    _current_compressed_len = compressed_len;
  }

 private:
  Config() {
    // Initialize default configuration
    // Conf format from caller to be imported 
    _chunk_size = 8192 * 4;
    _sector_size = 8192;
    _metadata_size = 512;
    _ca_length = 16;
    _strong_ca_length = 20;
    _primary_device_size = 1024 * 1024 * 1024LL * 20;
    _cache_device_size = 1024 * 1024 * 1024LL * 20;

    // Each bucket has 32 slots. Each index has _n_buckets buckets,
    // Each slot represents one chunk 32K.
    // To store all lbas without eviction
    // _n_buckets = primary_storage_size / 32K / 32 = 512
    _lba_signature_len = 12;
    _lba_bucket_no_len = 11;
    _lba_slots_per_bucket = 32;
    _ca_signature_len = 12;
    _ca_bucket_no_len = 10;
    _ca_slots_per_bucket = 32;

    _multi_thread = false;
    _max_num_global_threads = 32;
    _max_num_local_threads = 8;

    // io related
    _cache_device_name = "./cache_device";
    _primary_device_name = "./primary_device";

    // NORMAL_DIST_COMPRESSION related
    _current_data = (char*)malloc(sizeof(char) * _chunk_size);
    set_fingerprint_computation_method(0);
    set_fingerprint_algorithm(1);
  }

  uint32_t _chunk_size; // 8k size chunk
  uint32_t _sector_size; // 8k size sector
  uint32_t _metadata_size; // 512 byte size chunk
  uint32_t _ca_length; // content address (fingerprint) length, 20 bytes if using SHA1
  uint32_t _strong_ca_length; // content address (fingerprint) length, 20 bytes if using SHA1

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

  // Multi threading related
  uint32_t _max_num_global_threads;
  uint32_t _max_num_local_threads;
  bool _multi_thread;

  // io related
  char *_primary_device_name;
  char *_cache_device_name;
  uint64_t _primary_device_size;
  uint64_t _cache_device_size;
  uint32_t _buffered_write_buffer_size = 0;
  bool _direct_io = false;

  // chunk algorithm
  // 0 - Strong hash (SHA1), 1 - Weak hash (MurmurHash3)
  uint32_t _fingerprint_algorithm;
  // 0 - Strong hash, 1 - Weak hash + memcmp, 2 - Weak hash + Strong hash
  // If computation_method is set to 1 or 2, the _fingerprint_algorithm field must be 1
  uint32_t _fingerprint_computation_method;

  // Used when replaying FIU trace, for each request, we would fill in the fingerprint value
  // specified in the trace rather than the computed one.
  char _current_fingerprint[20];

  // Used when using NORMAL_DIST_COMPRESSION
  char* _current_data;
  int _current_compressed_len;
};

}

#endif
