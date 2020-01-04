#ifndef __STATS_H__
#define __STATS_H__
#include <atomic>
#include "common/common.h"
#include <iomanip>
#include <map>
#include <cassert>
#include "metadata/cachededup/common.h"
namespace cache {
  /*
   * class Stats is used to statistic in the data path.
   * It is a singleton class with std::atomic to ensure
   * thread-safety.
   */
struct Stats {
  public:
    static Stats& getInstance() {
      static Stats instance;
      return instance;
    }

    void release() {}

    void dump()
    {
      std::cout << "Write: " << std::endl;
      std::cout << "    Num total write: " << _n_write_dup_content + _n_write_not_dup << std::endl
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

      std::cout << "IO statistics: " << std::endl
                << "    Num bytes metadata written to ssd: " <<          _n_metadata_bytes_written_to_ssd << std::endl
                << "    Num bytes metadata read from ssd: " << _n_metadata_bytes_read_from_ssd << std::endl
                << "    Num total bytes data should written to ssd: " << _n_total_bytes_written_to_ssd << std::endl
                << "    Num bytes data written to ssd: " << _n_data_bytes_written_to_ssd << std::endl
                << "    Num bytes data read from ssd: " << _n_data_bytes_read_from_ssd << std::endl
                << "    Num bytes data written to write buffer: " << _n_bytes_written_to_write_buffer << std::endl
                << "    Num bytes data read from write buffer: " << _n_bytes_read_from_write_buffer << std::endl
                << "    Num bytes written to hdd: " << _n_bytes_written_to_hdd << std::endl
                << "    Num bytes read from hdd: " << _n_bytes_read_from_hdd << std::endl
                << std::endl;

      std::cout << std::fixed << std::setprecision(0) << "Time Elapsed: " << std::endl
                << "    Time elpased for compression: " << _time_elapsed_compression << std::endl
                << "    Time elpased for decompression: " << _time_elapsed_decompression << std::endl
                << "    Time elpased for computeFingerprint: " << _time_elapsed_fingerprinting << std::endl
                << "    Time elpased for dedup: " << _time_elapsed_dedup << std::endl
                << "    Time elpased for lookup: " << _time_elapsed_lookup << std::endl
                << "    Time elpased for update_index: " << _time_elapsed_update_index << std::endl
                << "    Time elpased for io_ssd: " << _time_elapsed_io_ssd << std::endl
                << "    Time elpased for io_hdd: " << _time_elapsed_io_hdd << std::endl
                << "    Time elpased for debug: " << _time_elapsed_debug << std::endl
                << std::endl;

      std::cout << std::setprecision(2) << "Overall Stats: " << std::endl
                << "    Hit ratio: " << _n_read_hit * 1.0 / (_n_read_hit + _n_read_not_hit) * 100.0 << "%" << std::endl
                << "    Dup ratio: " << 1.0 * (_n_write_dup_content + _n_read_not_hit_dup_content) / (_n_write + _n_read_not_hit) * 100.0 << "%" << std::endl
                << "    Dup ratio (not include read): " << 1.0 * _n_write_dup_content / _n_write  * 100.0 << "%" << std::endl;

      std::cout << std::defaultfloat;

    }

    inline void setCurrentRequestType(bool is_write) {
      if (is_write) _current_request_type = 1;
      else _current_request_type = 0;
    }

    inline void addReadLookupStatistics(Chunk &c) {
      if (c.lookupResult_ == HIT) {
        _n_read_hit.fetch_add(1, std::memory_order_relaxed);
      } else if (c.lookupResult_ == NOT_HIT) {
        _n_read_not_hit.fetch_add(1, std::memory_order_relaxed);

        if (c.hitLBAIndex_ == false) {
          _n_read_not_hit_lba_not_hit.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (c.hitFPIndex_ == false) {
            _n_read_not_hit_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
          } else {
            // read request only match LBA
            if (c.verficationResult_ == BOTH_LBA_AND_FP_NOT_VALID) {
              _n_read_not_hit_lba_not_match.fetch_add(1, std::memory_order_relaxed);
            } else {
              std::cout << c.hasFingerprint_ << std::endl;
              std::cout << c.verficationResult_ << std::endl;
              assert(0);
            }
          }
        }
      }
    }

    inline void add_read_post_dedup_stat(Chunk &c) {
      if (c.dedupResult_ == DUP_CONTENT) {
        _n_read_not_hit_dup_content.fetch_add(1, std::memory_order_relaxed);
      } else {
        _n_read_not_hit_not_dup.fetch_add(1, std::memory_order_relaxed);
        if (c.hitFPIndex_ == false) {
          _n_read_not_hit_not_dup_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (c.verficationResult_ == BOTH_LBA_AND_FP_NOT_VALID
              || c.verficationResult_ == ONLY_LBA_VALID) {
            _n_read_not_hit_not_dup_ca_not_match.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    }

    inline void add_write_stat(Chunk &c) {
      _n_write.fetch_add(1, std::memory_order_relaxed);
      if (c.dedupResult_ == DUP_CONTENT) {
        _n_write_dup_content.fetch_add(1, std::memory_order_relaxed);
      } else if (c.dedupResult_ == NOT_DUP) {
        _n_write_not_dup.fetch_add(1, std::memory_order_relaxed);
        if (!c.hitFPIndex_) {
          _n_write_not_dup_ca_not_hit.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (c.verficationResult_ == BOTH_LBA_AND_FP_NOT_VALID
              || c.verficationResult_ == ONLY_LBA_VALID) {
            _n_write_not_dup_ca_not_match.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    }


    int _current_request_type;

    inline int get_current_request_type() {
      return _current_request_type;
    }
    /*
     * Write - consist -- dup_Write   - cause - ca hit and ca match, lba hit and match
     *                 |
     *                 -- dup_content - cause - ca hit and ca match, lba not hit or match
     *                 |
     *                 -- not_dup     - cause - ca not hit
     *                                        |
     *                                        - ca hit but not match
     */
    std::atomic<uint64_t> _n_write;
    std::atomic<uint64_t> _n_write_dup_content;
    std::atomic<uint64_t> _n_write_not_dup;

    // write and lba index information
    // ca not hit - the entry has never shown before or been evicted already
    // ca not match - reflect the number of collision
    std::atomic<uint64_t> _n_write_not_dup_ca_not_hit;
    std::atomic<uint64_t> _n_write_not_dup_ca_not_match;

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

    /*
     * Time Elapsed. Time consumed by each part of the system
     */
#define _(str) \
    uint64_t _time_elapsed_##str; \
    inline void add_time_elapsed_##str(uint64_t v) {\
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
    _(debug);
#undef _

    // compression level
    std::atomic<int> _compress_level[4];

    // stats in write buffer
    std::atomic<uint64_t> _n_bytes_written_to_write_buffer;
    std::atomic<uint64_t> _n_bytes_read_from_write_buffer;

    // describe the total number of bytes should be written without dedup and compression
    std::atomic<uint64_t> _n_total_bytes_written_to_ssd; 
    std::atomic<uint64_t> _n_data_bytes_written_to_ssd;
    std::atomic<uint64_t> _n_data_bytes_read_from_ssd;

    std::atomic<uint64_t> _n_metadata_bytes_written_to_ssd;
    std::atomic<uint64_t> _n_metadata_bytes_read_from_ssd;

    std::atomic<uint64_t> _n_bytes_written_to_hdd;
    std::atomic<uint64_t> _n_bytes_read_from_hdd;

    inline void add_bytes_written_to_write_buffer(uint64_t v) { _n_bytes_written_to_write_buffer.fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_write_buffer(uint64_t v) {  _n_bytes_read_from_write_buffer .fetch_add(v, std::memory_order_relaxed); }

    inline void add_total_bytes_written_to_ssd(uint64_t v) { _n_total_bytes_written_to_ssd.fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_written_to_ssd(uint64_t v) {   _n_data_bytes_written_to_ssd  .fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_ssd(uint64_t v) {    _n_data_bytes_read_from_ssd   .fetch_add(v, std::memory_order_relaxed); }

    inline void add_metadata_bytes_written_to_ssd(uint64_t v) {   _n_metadata_bytes_written_to_ssd  .fetch_add(v, std::memory_order_relaxed); }
    inline void add_metadata_bytes_read_from_ssd(uint64_t v) {    _n_metadata_bytes_read_from_ssd   .fetch_add(v, std::memory_order_relaxed); }

    inline void add_bytes_written_to_hdd(uint64_t v) { _n_bytes_written_to_hdd.fetch_add(v, std::memory_order_relaxed); }
    inline void add_bytes_read_from_hdd(uint64_t v) {  _n_bytes_read_from_hdd .fetch_add(v, std::memory_order_relaxed); }

    inline void add_compress_level(int compress_level) 
    {
      _compress_level[compress_level].fetch_add(1, std::memory_order_relaxed);
    }

    void reset() {
      _n_write.store(0, std::memory_order_relaxed);
      _n_write_dup_content.store(0, std::memory_order_relaxed);
      _n_write_not_dup.store(0, std::memory_order_relaxed);
      _n_write_not_dup_ca_not_hit.store(0, std::memory_order_relaxed);
      _n_write_not_dup_ca_not_match.store(0, std::memory_order_relaxed);
      _n_read_hit.store(0, std::memory_order_relaxed);
      _n_read_not_hit.store(0, std::memory_order_relaxed);
      _n_read_not_hit_dup_content.store(0, std::memory_order_relaxed);
      _n_read_not_hit_not_dup.store(0, std::memory_order_relaxed);
      _n_read_not_hit_lba_not_hit.store(0, std::memory_order_relaxed);
      _n_read_not_hit_ca_not_hit.store(0, std::memory_order_relaxed);
      _n_read_not_hit_lba_not_match.store(0, std::memory_order_relaxed);
      _n_read_not_hit_not_dup_ca_not_hit.store(0, std::memory_order_relaxed);
      _n_read_not_hit_not_dup_ca_not_match.store(0, std::memory_order_relaxed);

      _n_total_bytes_written_to_ssd.store(0, std::memory_order_relaxed);
      _n_metadata_bytes_written_to_ssd.store(0, std::memory_order_relaxed);
      _n_metadata_bytes_read_from_ssd.store(0, std::memory_order_relaxed);
      _n_data_bytes_written_to_ssd.store(0, std::memory_order_relaxed);
      _n_data_bytes_read_from_ssd.store(0, std::memory_order_relaxed);
      _n_bytes_written_to_hdd.store(0, std::memory_order_relaxed);
      _n_bytes_read_from_hdd.store(0, std::memory_order_relaxed);
      _n_bytes_written_to_write_buffer.store(0, std::memory_order_relaxed);
      _n_bytes_read_from_write_buffer.store(0, std::memory_order_relaxed);

#define _(str) \
      _time_elapsed_##str = 0;
      _(compression);
      _(decompression);
      _(fingerprinting);
      _(dedup);
      _(lookup);
      _(update_index);
      _(io_ssd);
      _(io_hdd);
      _(debug);
#undef _
    }
  private:
      Stats() {
        reset();
      }
  };
}

#endif
