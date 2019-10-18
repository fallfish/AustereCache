#include "managemodule.h"
#include "writebuffer.h"
#include "common/stats.h"
#include "utils/utils.h"
#include <cassert>

namespace cache {

ManageModule::ManageModule(
  std::shared_ptr<IOModule> ioModule,
  std::shared_ptr<MetadataModule> metadataModule) :
  ioModule_(ioModule), metadataModule_(metadataModule)
{
#if defined(CDARC)
  weuSize_ = Config::getInstance()->getWriteBufferSize();
  currentCachedataLocation_ = 0;
  currentWEUId_ = 0;
#else
  if (Config::getInstance()->getWriteBufferSize() != 0) {
    writeBuffer_ = std::make_unique<WriteBuffer>(
      Config::getInstance()->getWriteBufferSize());
  }
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

#if defined(DLRU) || defined(DARC)
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
    len = (chunk.compressedLevel_ + 1) * Config::getInstance()->getSectorSize();
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

  if (writeBuffer_ != nullptr) {
    auto indexAndOffset = writeBuffer_->prepareRead(addr, len);
    auto index = indexAndOffset.first, offset = indexAndOffset.second;
    if (index != ~0u) {
      ioModule_->read(IN_MEM_BUFFER, offset, buf, len);
    }
    writeBuffer_->commitRead();
  } else {
    ioModule_->read(deviceType, addr, buf, len);
  }

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
    len = (chunk.compressedLevel_ + 1) * Config::getInstance()->getSectorSize();
#else
#if defined(DLRU) || defined(DARC)
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

          ioModule_->flush(evictedCachedataLocation, 0, weuSize_);
          weuToCachedataLocation_[currentWEUId_] = evictedCachedataLocation;
        } else {
          ioModule_->flush(currentCachedataLocation_, 0, weuSize_);
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
#if defined(WRITE_BACK_CACHE) == 0 // If enable write back cache
  if (generatePrimaryWriteRequest(chunk, deviceType, addr, buf, len)) {
    ioModule_->write(deviceType, addr, buf, len);
  }
#endif
  if (generateCacheWriteRequest(chunk, deviceType, addr, buf, len)) {
    if (writeBuffer_ != nullptr) {
      std::pair<uint32_t, uint32_t> indexAndOffset;
      uint32_t index, offset;
      do {
        indexAndOffset = writeBuffer_->prepareWrite(addr, buf, len);
        index = indexAndOffset.first;
        offset = indexAndOffset.second;

        if (index == ~0u) {
          auto toFlush = writeBuffer_->flush();
          for (WriteBuffer::Entry entry : toFlush) {
            ioModule_->flush(entry.addr_, entry.off_, entry.len_);
          }
        } else {
          ioModule_->write(IN_MEM_BUFFER, offset, buf, len);
          break;
        }
      } while (true);
      writeBuffer_->commitWrite(addr, offset, len, index);
    } else {
      ioModule_->write(deviceType, addr, buf, len);
    }
  }
  return 0;
}

void ManageModule::updateMetadata(Chunk &chunk)
{
  BEGIN_TIMER();
  metadataModule_->update(chunk);
  END_TIMER(update_index);
}

}
