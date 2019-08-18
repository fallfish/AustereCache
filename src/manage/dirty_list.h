#ifndef __DIRTY_LIST__
#define __DIRTY_LIST__

#include "compression/compressionmodule.h"
#include "io/iomodule.h"

#include <map>
#include <list>

namespace cache {

  class DirtyList {

    public:
      DirtyList();
      void set_compression_module(std::shared_ptr<CompressionModule> compression_module);
      void set_io_module(std::shared_ptr<IOModule> io_module);

      static DirtyList* get_instance();

      struct EvictedBlock {
        uint64_t _ssd_data_location;
        uint32_t _len;
      };


      void add_latest_update(uint64_t lba, uint64_t ssd_data_location, uint32_t len);
      void add_evicted_block(uint64_t addr, uint32_t len);
      void flush();

    private:
      // logical block address to cache data pointer and length
      std::map<uint64_t, std::pair<uint64_t, uint32_t>> _latest_updates;
      std::list<EvictedBlock> _evicted_blocks;
      std::shared_ptr<CompressionModule> _compression_module;
      std::shared_ptr<IOModule> _io_module;
      uint64_t _dirty_list_size_limit;
  };
}

#endif
