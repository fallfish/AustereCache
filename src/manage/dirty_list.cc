#include "common/config.h"
#include "dirty_list.h"

#include <csignal>


namespace cache {

  DirtyList::DirtyList()
  {
    _dirty_list_size_limit = 1024;
  }

  void DirtyList::set_compression_module(std::shared_ptr<CompressionModule> compression_module)
  {
    _compression_module = std::move(compression_module);
  }

  void DirtyList::set_io_module(std::shared_ptr<IOModule> io_module)
  {
    _io_module = std::move(io_module);
  }

  DirtyList* DirtyList::get_instance() {
    static DirtyList instance;
    return &instance;
  }

  void DirtyList::add_latest_update(uint64_t lba, uint64_t ssd_data_location, uint32_t len)
  {
    // TODO: persist the dirty list
    _latest_updates[lba] = std::make_pair(ssd_data_location, len);
  }

  /*
   * Description:
   *   Add a newly evicted block into the dirty list
   *   TODO: To avoid rewrite the same space (the ssd space been allocated to new request)
   *         We could persist to a special area in SSD and sync to HDD asynchronously.
   *          
   */
  void DirtyList::add_evicted_block(uint8_t *fingerprint, uint64_t ssd_data_location, uint32_t len)
  {
    //std::cout << "Add evicted block! location: " << ssd_data_location << std::endl;
    EvictedBlock evicted_block;
    if (fingerprint != nullptr) {
      memcpy(evicted_block._fingerprint, fingerprint, Config::get_configuration().get_ca_length());
    }
    evicted_block._ssd_data_location = ssd_data_location;
    evicted_block._len = len;

    _evicted_blocks.push_back(evicted_block);

    flush();
  }

  /* 
   * Description:
   *   Flush the dirty data into the HDD.
   *   Flush should be triggered under two cases.
   *   1. We come across a newly evicted block.
   *   2. The length of the dirty list exceeds a limit.
   *   Firstly, check against the dirty list whether this block containing dirty data
   *   Secondly, if there is a dirty data block, read it from ssd
   *   Thirdly, write it to the hdd
   */
  void DirtyList::flush() {
    alignas(512) uint8_t compressed_data[Config::get_configuration().get_chunk_size()];
    alignas(512) uint8_t uncompressed_data[Config::get_configuration().get_chunk_size()];
    alignas(512) Metadata metadata;
    uint32_t sector_size = Config::get_configuration().get_sector_size();

    // Case 1: We have a newly evicted block.
    while (_evicted_blocks.size() != 0) {
      uint64_t ssd_data_location = _evicted_blocks.front()._ssd_data_location;
      uint32_t len = _evicted_blocks.front()._len;
      _evicted_blocks.pop_front();

      std::vector<uint64_t> lbas_to_flush;
      lbas_to_flush.clear();
      bool k = 0;
      for (auto pr : _latest_updates) {
        if (pr.second.first == ssd_data_location) {
          assert(pr.second.second == len);
          lbas_to_flush.push_back(pr.first);
        }
      }
      // Read cached metadata (compressed length)
      _io_module->read(1, ssd_data_location, 
          &metadata, Config::get_configuration().get_metadata_size());
      // Read cached data
      _io_module->read(1, ssd_data_location + Config::get_configuration().get_metadata_size(),
          compressed_data, len * Config::get_configuration().get_sector_size());
      // Decompress cached data
      memset(uncompressed_data, 0, 32768);
      _compression_module->decompress(compressed_data, uncompressed_data, 
          metadata._compressed_len, Config::get_configuration().get_chunk_size());

      for (auto lba : lbas_to_flush) {
        _io_module->write(0, lba, uncompressed_data,
            Config::get_configuration().get_chunk_size());
        _latest_updates.erase(lba);
      }
    }

    // Case 2: The length of the dirty list exceed a limit.
    if (_latest_updates.size() >= 4) {
      for (auto pr : _latest_updates) {
        uint64_t lba = pr.first;
        uint64_t ssd_data_location = pr.second.first;

        // Read cached metadata (compressed length)
        _io_module->read(1, ssd_data_location, 
            &metadata, Config::get_configuration().get_metadata_size());
        // Read cached data
        _io_module->read(1, ssd_data_location + Config::get_configuration().get_metadata_size(),
            compressed_data, pr.second.second * Config::get_configuration().get_sector_size());
        // Decompress cached data
        _compression_module->decompress(compressed_data, uncompressed_data, 
            metadata._compressed_len, Config::get_configuration().get_chunk_size());
        // Write uncompressed data into HDD
        _io_module->write(0, lba, uncompressed_data,
            Config::get_configuration().get_chunk_size());
      }
      _latest_updates.clear();
    }
  }
}

