#include "managemodule.h"
#include "io/writebuffer.h"
#include "common/stats.h"
#include "utils/utils.h"
#include <cassert>

namespace cache {
    ManageModule& ManageModule::getInstance() {
      static ManageModule instance;
      return instance;
    }

    ManageModule::ManageModule()
    {
#if defined(CDARC)
      weuSize_ = Config::getInstance().getWriteBufferSize();
  currentCachedataLocation_ = 0;
  currentWEUId_ = 0;
#else
#endif
    }

/**
 * Device number: 
 * 0 - Primary disk (HDD) 
 * 1 - Cache disk (SSD) 
 * 2 - WEU (in memory)
 */

    void ManageModule::generateReadRequest(
      cache::Chunk &chunk, cache::DeviceType &deviceType,
      uint64_t &addr, uint8_t *&buf, uint32_t &len)
    {
      if (chunk.lookupResult_ == HIT) {
#if defined(CACHE_DEDUP)

        #if defined(DLRU) || defined(DARC) || defined(BUCKETDLRU)
    deviceType = CACHE_DEVICE;
    addr = chunk.cachedataLocation_;
    buf = chunk.buf_;
    len = chunk.len_;
#elif defined(CDARC)
    if (currentWEUId_ == chunk.weuId_) {
      deviceType = IN_MEM_BUFFER;
      addr = chunk.weuOffset_;
    } else {
      deviceType = CACHE_DEVICE;
      addr = weuToCachedataLocation_[chunk.weuId_] + chunk.weuOffset_;
    }
    if (chunk.compressedLen_ == chunk.len_) {
      buf = chunk.buf_;
    } else {
      buf = chunk.compressedBuf_;
    }
    len = chunk.compressedLen_;
#endif

#else // ACDC
        deviceType = CACHE_DEVICE;
        if (chunk.compressedLen_ != 0) {
          buf = chunk.compressedBuf_;
        } else {
          buf = chunk.buf_;
        }
        addr = chunk.cachedataLocation_;
        len = (chunk.compressedLevel_ + 1) * Config::getInstance().getSectorSize();
#endif
      } else {
        deviceType = PRIMARY_DEVICE;
        addr = chunk.addr_;
        buf = chunk.buf_;
        len = chunk.len_;
      }
    }

    int ManageModule::read(Chunk &chunk)
    {
      DeviceType deviceType;
      uint64_t addr;
      uint8_t *buf;
      uint32_t len;
      generateReadRequest(chunk, deviceType, addr, buf, len);
      IOModule::getInstance().read(deviceType, addr, buf, len);

      return 0;
    }

    bool ManageModule::generatePrimaryWriteRequest(
      Chunk &chunk, DeviceType &deviceType,
      uint64_t &addr, uint8_t *&buf, uint32_t &len)
    {
      deviceType = PRIMARY_DEVICE;
      if (chunk.lookupResult_ == LOOKUP_UNKNOWN &&
          (chunk.dedupResult_ == DUP_CONTENT || chunk.dedupResult_ == NOT_DUP)) {
        addr = chunk.addr_;
        buf = chunk.buf_;
        len = chunk.len_;
        return true;
      }

      return false;
    }

    bool ManageModule::generateCacheWriteRequest(
      Chunk &chunk, DeviceType &deviceType,
      uint64_t &addr, uint8_t *&buf, uint32_t &len)
    {
      deviceType = CACHE_DEVICE;
      if (chunk.dedupResult_ == NOT_DUP) {
#if !defined(CACHE_DEDUP)
        addr = chunk.cachedataLocation_;
        buf = chunk.compressedBuf_;
        len = (chunk.compressedLevel_ + 1) * Config::getInstance().getSectorSize();
#else
#if defined(DLRU) || defined(DARC) || defined(BUCKETDLRU)
        addr = chunk.cachedataLocation_;
        buf = chunk.buf_;
        len = chunk.len_;
#elif defined(CDARC)
        uint64_t evictedCachedataLocation = -1;
        if (currentWEUId_ != chunk.weuId_) {
          if (chunk.evictedWEUId_ != currentWEUId_) {
            if (chunk.evictedWEUId_ != ~0u) {
              evictedCachedataLocation = weuToCachedataLocation_[chunk.evictedWEUId_];
              weuToCachedataLocation_.erase(chunk.evictedWEUId_);

              IOModule::getInstance().flush(evictedCachedataLocation, 0, weuSize_);
              weuToCachedataLocation_[currentWEUId_] = evictedCachedataLocation;
            } else {
              IOModule::getInstance().flush(currentCachedataLocation_, 0, weuSize_);
              weuToCachedataLocation_[currentWEUId_] = currentCachedataLocation_;
              currentCachedataLocation_ += weuSize_;
            }
          }

          currentWEUId_ = chunk.weuId_;
        }
        deviceType = IN_MEM_BUFFER;
        addr = chunk.weuOffset_;
        buf = chunk.compressedBuf_;
        len = chunk.compressedLen_;
#endif
#endif
        return true;
      }
      return false;
    }

    int ManageModule::write(Chunk &chunk)
    {
      DeviceType deviceType;
      uint64_t addr;
      uint8_t *buf;
      uint32_t len;

      if (Config::getInstance().getCacheMode() == CacheModeEnum::tWriteThrough) {
        if (generatePrimaryWriteRequest(chunk, deviceType, addr, buf, len)) {
          IOModule::getInstance().write(deviceType, addr, buf, len);
        }
      }

      if (generateCacheWriteRequest(chunk, deviceType, addr, buf, len)) {
        IOModule::getInstance().write(deviceType, addr, buf, len);
      }
      return 0;
    }

    void ManageModule::updateMetadata(Chunk &chunk)
    {
      BEGIN_TIMER();
        MetadataModule::getInstance().update(chunk);
      END_TIMER(update_index);
    }

}
