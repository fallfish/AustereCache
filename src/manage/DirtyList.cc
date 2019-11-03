#include "common/config.h"
#include "DirtyList.h"

#include <csignal>


namespace cache {

  DirtyList::DirtyList()
  {
    size_ = 1024;
  }

  void DirtyList::setCompressionModule(std::shared_ptr<CompressionModule> compressionModule)
  {
    compressionModule_ = std::move(compressionModule);
  }

  DirtyList& DirtyList::getInstance() {
    static DirtyList instance;
    return instance;
  }

  void DirtyList::addLatestUpdate(uint64_t lba, uint64_t cachedataLocation, uint32_t len)
  {
    // TODO: persist the dirty list
    latestUpdates_[lba] = std::make_pair(cachedataLocation, len);
    if (latestUpdates_.size() >= size_) {
      flush();
    }
  }

  /*
   * Description:
   *   Add a newly evicted block into the dirty list
   *   TODO: To avoid rewrite the same space (the ssd space been allocated to new request)
   *         We could persist to a special area in SSD and sync to HDD asynchronously.
   *          
   */
  void DirtyList::addEvictedChunk(uint64_t cachedataLocation, uint32_t len)
  {
    EvictedBlock evicted_block{};
    evicted_block.cachedataLocation_ = cachedataLocation;
    evicted_block.len_ = len;

    evictedBlocks_.push_back(evicted_block);
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
#elif defined(CDARC)
#else
#endif
}