#ifndef __STATS_H__
#define __STATS_H__
#include <atomic>
#include "common/common.h"
#include <iomanip>
namespace cache {
  /*
   * class Stats is used to statistic in the data path.
   * It is a singleton class with std::atomic to ensure
   * thread-safety.
   */
  struct Stats {
    /*
     * Write - consist -- dup_Write   - cause - ca hit and ca match, lba hit and match
     *                 |
     *                 -- dup_content - cause - ca hit and ca match, lba not hit or match
     *                 |
     *                 -- not_dup     - cause - ca not hit
     *                                        |
     *                                        - ca hit but not match
     */
    std::atomic<uint64_t> _n_write_dup_write;
    std::atomic<uint64_t> _n_write_dup_content;
    std::atomic<uint64_t> _n_write_not_dup;

    // write and lba index information
    // ca not hit - the entry has never shown before or been evicted already
    // ca not match - reflect the number of collision
    std::atomic<uint64_t> _n_write_not_dup_ca_not_hit;
    std::atomic<uint64_t> _n_write_not_dup_ca_not_match;
    inline void add_write_stat(Chunk &c) {
      if (c._dedup_result == DUP_WRITE) {
        _n_write_dup_write.fetch_add(1, std::memory_order_relaxed);
      } else if (c._dedup_result == DUP_CONTENT) {
        _n_write_dup_content.fetch_add(1, std::memory_order_relaxed);
      } else if (c._dedup_result == NOT_DUP) {
        _n_write_not_dup.fetch_add(1, std::memory_order_relaxed);
        if (c._ca_hit == false) {
          _n_write_not_dup_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (c._verification_result == BOTH_LBA_AND_CA_NOT_VALID
              || c._verification_result == ONLY_LBA_VALID) {
            _n_write_not_dup_ca_not_match.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    }

    /*
     * Read - consist of +-- hit     - cause - lba hit, ca hit, and lba match
     *                   |
     *                   +-- not hit - cause - lba not hit (lba never shown or evicted already)
     *                                       |
     *                                       - lba hit but ca not hit (ca must be evicted already)
     *                                       |
     *                                       - lba hit, ca hit but lba not match (lba sig collision)
     *                               - produce           - dup
     *                               (the same as write) |
     *                                                   - not dup
     */
    // n_read_hit + n_read_not_hit = n_read
    // n_read_not_hit_dup_content + n_read_not_hit_not_dup = n_read_not_hit
    std::atomic<uint64_t> _n_read_hit;
    std::atomic<uint64_t> _n_read_not_hit;
    std::atomic<uint64_t> _n_read_not_hit_dup_content;
    std::atomic<uint64_t> _n_read_not_hit_not_dup;

    // read and index information
    // lba not hit - the entry has never shown before or been evicted already
    // ca not hit - the entry must be evicted already (ca -> dp is the most critical one that affects hit ratio)
    // lba not match - lba collision happens (lba sig collision may cause eviction or cause 
    std::atomic<uint64_t> _n_read_not_hit_lba_not_hit;
    std::atomic<uint64_t> _n_read_not_hit_ca_not_hit;
    std::atomic<uint64_t> _n_read_not_hit_lba_not_match;

    // ca not hit - the entry has never shown before or been evicted already
    // ca not match - reflect the number of collision
    // Note: ca not match causes a invalidation of the corresponding entry
    std::atomic<uint64_t> _n_read_not_hit_not_dup_ca_not_hit;
    std::atomic<uint64_t> _n_read_not_hit_not_dup_ca_not_match;
    inline void add_read_stat(Chunk &c) {
      if (c._lookup_result == HIT) {
        _n_read_hit.fetch_add(1, std::memory_order_relaxed);
      } else if (c._lookup_result == NOT_HIT) {
        _n_read_not_hit.fetch_add(1, std::memory_order_relaxed);

        if (c._lba_hit == false) {
          _n_read_not_hit_lba_not_hit.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (c._ca_hit == false) {
            _n_read_not_hit_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
          } else {
            // read request only match LBA
            if (c._verification_result == BOTH_LBA_AND_CA_NOT_VALID) {
              _n_read_not_hit_lba_not_match.fetch_add(1, std::memory_order_relaxed);
            }
          }
        }
      }
    }
    inline void add_read_post_dedup_stat(Chunk &c) {
      _n_read_not_hit_not_dup.fetch_add(1, std::memory_order_relaxed);
      if (c._ca_hit == false) {
        _n_read_not_hit_not_dup_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
      } else {
        if (c._verification_result == BOTH_LBA_AND_CA_NOT_VALID
            || c._verification_result == ONLY_LBA_VALID) {
          _n_read_not_hit_not_dup_ca_not_match.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }

    /*
     * statistics for evictions
     * 1. Number of evictions (caused by collision and full capacity)
     * 2. The distribution of hit and update for each bucket
     */
    std::atomic<uint64_t> _n_lba_index_eviction_caused_by_collision;
    std::atomic<uint64_t> _n_lba_index_eviction_caused_by_capacity;

    std::atomic<uint64_t> _n_ca_index_eviction_caused_by_collision;
    std::atomic<uint64_t> _n_ca_index_eviction_caused_by_capacity;

    std::unique_ptr<std::atomic<uint32_t> []> _n_updates_lba_buckets;
    std::unique_ptr<std::atomic<uint32_t> []> _n_updates_ca_buckets;
    std::unique_ptr<std::atomic<uint32_t> []> _n_hits_lba_buckets;
    std::unique_ptr<std::atomic<uint32_t> []> _n_hits_ca_buckets;


    /*
     * Description:
     *   LBA signature collision caused eviction could be detected
     *   in the metadata/bucket.cc:LBABucket::update procedure, 
     *   where stored CA signature is checked against
     *   the "real" (either fetched from a read hit or computed from a write/read not hit)
     *   CA signature, if not match, a collision is bound to happen.
     */
    inline void add_lba_index_eviction_caused_by_collision() {
      _n_lba_index_eviction_caused_by_collision.fetch_add(1, std::memory_order_relaxed);
    }

    /* 
     * Description:
     *   LBA eviction caused by capacity is triggered each time
     *   the space is full and we need to accommodate new entry.
     *   1. cache_policy.cc:LRUExecutor::allocate
     *   2. cachededup.cc:(XX)AddressIndex::allocate
     */
    inline void add_lba_index_eviction_caused_by_capacity() {
      _n_lba_index_eviction_caused_by_capacity.fetch_add(1, std::memory_order_relaxed);
    }

    /* 
     * Description:
     *   CA signature collision caused eviction could only happen in
     *   1. Write not dup - caused by CA hit but CA not valid
     *   2. Read not hit not dup - caused by CA hit but CA not valid
     */
    inline void add_ca_index_eviction_caused_by_collision() {
      _n_ca_index_eviction_caused_by_collision.fetch_add(1, std::memory_order_relaxed);
    }

    /* 
     * Description:
     *   CA eviction caused by capacity is triggered each time
     *   the space is full and we need to accommodate new entry.
     *   1. cache_policy.cc:CAClockExecutor::allocate
     *   2. cachededup.cc:(XX)FingerprintIndex::allocate
     */
    inline void add_ca_index_eviction_caused_by_capacity() {
      _n_ca_index_eviction_caused_by_capacity.fetch_add(1, std::memory_order_relaxed);
    }

    /*
     * Description:
     *
     *
     */
    inline void add_lba_index_bucket_update(uint32_t bucket_id) {
      _n_updates_lba_buckets[bucket_id].fetch_add(1, std::memory_order_relaxed);
    }
    inline void add_ca_index_bucket_update(uint32_t bucket_id) {
      _n_updates_ca_buckets[bucket_id].fetch_add(1, std::memory_order_relaxed);
    }
    inline void add_lba_index_bucket_hit(uint32_t bucket_id) {
      _n_hits_lba_buckets[bucket_id].fetch_add(1, std::memory_order_relaxed);
    }
    inline void add_ca_index_bucket_hit(uint32_t bucket_id) {
      _n_hits_ca_buckets[bucket_id].fetch_add(1, std::memory_order_relaxed);
    }

    // statistics for io module
    std::atomic<uint64_t> _n_metadata_bytes_written_to_ssd;
    std::atomic<uint64_t> _n_metadata_bytes_read_from_ssd;
    std::atomic<uint64_t> _n_data_bytes_written_to_ssd;
    std::atomic<uint64_t> _n_data_bytes_read_from_ssd;
    std::atomic<uint64_t> _n_bytes_written_to_hdd;
    std::atomic<uint64_t> _n_bytes_read_from_hdd;
    /* 
     * Description:
     *   We would statistic some numbers from io request.
     *   Called from io/iomodule.cc:IOModule::write/read
     *   type: 0 means HDD, 1 means SSD, the same as IOModule
     *   When length = 512 bytes, the data is bound to be a metadata,
     *   so I will count it into metadata write into SSD.
     */
    inline void add_io_request(uint32_t device_no, uint32_t type, uint64_t v) {
      if (device_no == 0) {
        if (type == 0) {
          _n_bytes_written_to_hdd.fetch_add(v, std::memory_order_relaxed);
        } else {
          _n_bytes_read_from_hdd.fetch_add(v, std::memory_order_relaxed);
        }
      } else {
        if (v == 512) {
          if (type == 0) {
            _n_metadata_bytes_written_to_ssd.fetch_add(v, std::memory_order_relaxed);
          } else {
            _n_metadata_bytes_read_from_ssd.fetch_add(v, std::memory_order_relaxed);
          }
        } else {
          if (type == 0) {
            _n_data_bytes_written_to_ssd.fetch_add(v, std::memory_order_relaxed);
          } else {
            _n_data_bytes_read_from_ssd.fetch_add(v, std::memory_order_relaxed);
          }
        }
      }
    }

    /*
     * Time Elapsed. Time consumed by each part of the system
     */
#define _(str) \
    double _time_elapsed_##str; \
    inline void add_time_elapsed_##str(double v) {\
      _time_elapsed_##str += v; \
    }
    //std::atomic<double> _time_elapsed_##str;
    //_time_elapsed_##str.fetch_add(v, std::memory_order_relaxed);
    _(compression);
    _(decompression);
    _(fingerprinting);
    _(dedup);
    _(lookup);
    _(update_index);
    _(io_ssd);
    _(io_hdd);
#undef _

    // compression level
    std::atomic<int> _compress_level[4];

    // stats in write buffer
    std::atomic<uint64_t> _n_bytes_written_to_write_buffer;
    std::atomic<uint64_t> _n_bytes_written_to_cache_disk;
    std::atomic<uint64_t> _n_bytes_written_to_primary_disk;
    std::atomic<uint64_t> _n_bytes_read_from_write_buffer;
    std::atomic<uint64_t> _n_bytes_read_from_cache_disk;
    std::atomic<uint64_t> _n_bytes_read_from_primary_disk;
    inline void add_bytes_written_to_write_buffer(uint64_t v) { _n_bytes_written_to_write_buffer.fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_written_to_cache_disk(uint64_t v) {   _n_bytes_written_to_cache_disk  .fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_written_to_primary_disk(uint64_t v) { _n_bytes_written_to_primary_disk.fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_write_buffer(uint64_t v) {  _n_bytes_read_from_write_buffer .fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_cache_disk(uint64_t v) {    _n_bytes_read_from_cache_disk   .fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_primary_disk(uint64_t v) {  _n_bytes_read_from_primary_disk .fetch_add(v, std::memory_order_relaxed); }

    static Stats* get_instance() {
      static Stats instance;
      return &instance;
    }

    Stats() {
#if !defined(CACHE_DEDUP)
      _n_updates_lba_buckets = std::make_unique<std::atomic<uint32_t> []>(1 << Config::get_configuration().get_lba_bucket_no_len());
      _n_updates_ca_buckets = std::make_unique<std::atomic<uint32_t> []>(1 << Config::get_configuration().get_ca_bucket_no_len());
      _n_hits_lba_buckets = std::make_unique<std::atomic<uint32_t> []>(1 << Config::get_configuration().get_lba_bucket_no_len());
      _n_hits_ca_buckets = std::make_unique<std::atomic<uint32_t> []>(1 << Config::get_configuration().get_ca_bucket_no_len());
#endif
      reset();
    }

    inline void add_compress_level(int compress_level) 
    {
      _compress_level[compress_level].fetch_add(1, std::memory_order_relaxed);
    }

    void reset()
    {
      _n_bytes_written_to_write_buffer .store(0, std::memory_order_relaxed);
      _n_bytes_written_to_cache_disk   .store(0, std::memory_order_relaxed);
      _n_bytes_written_to_primary_disk .store(0, std::memory_order_relaxed);
      _n_bytes_read_from_write_buffer  .store(0, std::memory_order_relaxed);
      _n_bytes_read_from_cache_disk    .store(0, std::memory_order_relaxed);
      _n_bytes_read_from_primary_disk  .store(0, std::memory_order_relaxed);
    }

    void dump()
    {
      std::cout << "Write: " << std::endl;
      std::cout << "    Num total write: " << _n_write_dup_write + _n_write_dup_content + _n_write_not_dup << std::endl
                << "    Num write dup write: " << _n_write_dup_write << std::endl
                << "    Num write dup content: " << _n_write_dup_content << std::endl
                << "    Num write not dup: " << _n_write_not_dup << std::endl
                << "        Num write not dup caused by ca not hit: " << _n_write_not_dup_ca_not_hit << std::endl
                << "        Num write not dup caused by ca not match: " << _n_write_not_dup_ca_not_match << std::endl
                << std::endl;

      std::cout << "Read: " << std::endl;
      std::cout << "    Num total read: " << _n_read_hit + _n_read_not_hit << std::endl
                << "    Num read hit: " << _n_read_hit << std::endl
                << "    Num read not hit: " << _n_read_not_hit << std::endl
                << "        Num read not hit caused by lba not hit: " << _n_read_not_hit_lba_not_hit << std::endl
                << "        Num read not hit caused by ca not hit: " << _n_read_not_hit_ca_not_hit << std::endl
                << "        Num read not hit caused by lba not match: " << _n_read_not_hit_lba_not_match << std::endl
                << "            Num read not hit dup content: " << _n_read_not_hit_dup_content << std::endl
                << "            Num read not hit not dup: " << _n_read_not_hit_not_dup << std::endl
                << "                Num read not hit not dup caused by ca not hit: " << _n_read_not_hit_not_dup_ca_not_hit << std::endl
                << "                Num read not hit not dup caused by ca not match: " << _n_read_not_hit_not_dup_ca_not_match << std::endl
                << std::endl;

      std::cout << "Index eviction statistics: " << std::endl
                << "    LBA Index eviction caused by hash collision: " << _n_lba_index_eviction_caused_by_collision << std::endl
                << "    LBA Index eviction caused by full capacity: " << _n_lba_index_eviction_caused_by_capacity << std::endl
                << "    CA Index eviction caused by hash collision: " << _n_ca_index_eviction_caused_by_collision << std::endl
                << "    CA Index eviction caused by full capacity: " << _n_ca_index_eviction_caused_by_capacity << std::endl
                << std::endl;

      std::cout << "IO statistics: " << std::endl
                << "    Num bytes metadata written to ssd: " <<          _n_metadata_bytes_written_to_ssd << std::endl
                << "    Num bytes metadata read from ssd: " << _n_metadata_bytes_read_from_ssd << std::endl
                << "    Num bytes data written to ssd: " << _n_data_bytes_written_to_ssd << std::endl
                << "    Num bytes data read from ssd: " << _n_data_bytes_read_from_ssd << std::endl
                << "    Num bytes written to hdd: " << _n_bytes_written_to_hdd << std::endl
                << "    Num bytes read from hdd: " << _n_bytes_read_from_hdd << std::endl
                << std::endl;

      std::cout << std::fixed << std::setprecision(0) << "Time Elapsed: " << std::endl
                << "    Time elpased for compression: " << _time_elapsed_compression << std::endl
                << "    Time elpased for decompression: " << _time_elapsed_decompression << std::endl
                << "    Time elpased for fingerprinting: " << _time_elapsed_fingerprinting << std::endl
                << "    Time elpased for dedup: " << _time_elapsed_dedup << std::endl
                << "    Time elpased for lookup: " << _time_elapsed_lookup << std::endl
                << "    Time elpased for update_index: " << _time_elapsed_update_index << std::endl
                << "    Time elpased for io_ssd: " << _time_elapsed_io_ssd << std::endl
                << "    Time elpased for io_hdd: " << _time_elapsed_io_hdd << std::endl
                << std::endl;

      std::cout << std::setprecision(2) << "Overall Stats: " << std::endl
                << "    Hit ratio: " << _n_read_hit * 1.0 / (_n_read_hit + _n_read_not_hit) * 100.0 << "%" << std::endl;

      std::cout << std::defaultfloat;

    }
  };
}

#endif
