#include "managemodule.h"
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
  _current_ssd_location = 0;
  _current_weu_id = 0;
#endif
}

/**
 * Device number: 
 * 0 - Primary disk (HDD) 
 * 1 - Cache disk (SSD) 
 * 2 - WEU (in memory)
 */

void ManageModule::generateReadRequest(
      cache::Chunk &c, cache::DeviceType &deviceType,
      uint64_t &addr, uint8_t *&buf, uint32_t &len)
{
  if (c.lookupResult_ == HIT) {
#if defined(CACHE_DEDUP)

#if defined(DLRU) || defined(DARC)
    deviceType = CACHE_DEVICE;
    addr = c.cachedataLocation_;
    buf = c.buf_;
    len = c.len_;
#elif defined(CDARC)
    if (_current_weu_id == c.weuId_) {
      deviceType = WRITE_BUFFER;
      addr = c._weu_offset;
    } else {
      deviceType = CACHE_DEVICE;
      addr = _weu_to_ssd_location[c.weuId_] + c._weu_offset;
    }
    if (c.compressedLen_ == c.len_) {
      buf = c.buf_;
    } else {
      buf = c.compressedBuf_;
    }
    len = c.compressedLen_;
#endif

#else // ACDC
    deviceType = CACHE_DEVICE;
    c.compressedLen_ = c.metadata_.compressedLen_;
    if (c.compressedLen_ != 0) {
      buf = c.compressedBuf_;
    } else {
      buf = c.buf_;
    }
    addr = c.cachedataLocation_;
    len = (c.compressedLevel_ + 1) * Config::getInstance()->getSectorSize();
#endif
  } else {
    deviceType = PRIMARY_DEVICE;
    addr = c.addr_;
    buf = c.buf_;
    len = c.len_;
  }
}

int ManageModule::read(Chunk &chunk)
{
  DeviceType deviceType;
  uint64_t addr;
  uint8_t *buf;
  uint32_t len;
  generateReadRequest(chunk, deviceType, addr, buf, len);
  ioModule_->read(deviceType, addr, buf, len);

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
#if defined(CACHE_DEDUP)

#if defined(DLRU) || defined(DARC)
    addr = chunk.cachedataLocation_;
    buf = chunk.buf_;
    len = chunk.len_;
#elif defined(CDARC)
    uint64_t evicted_ssd_location = -1;
    if (_current_weu_id != chunk.weuId_) {
      if (chunk._evicted_weu_id != _current_weu_id) {
        if (chunk._evicted_weu_id != ~0) {
          evicted_ssd_location = _weu_to_ssd_location[chunk._evicted_weu_id];
          _weu_to_ssd_location.erase(chunk._evicted_weu_id);

          ioModule_->flush(evicted_ssd_location);
          _weu_to_ssd_location[_current_weu_id] = evicted_ssd_location;
        } else {
          ioModule_->flush(_current_ssd_location);
          _weu_to_ssd_location[_current_weu_id] = _current_ssd_location;
          _current_ssd_location += weuSize_;
        }
      }

      _current_weu_id = chunk.weuId_;
    }
    deviceType = WRITE_BUFFER;
    addr = chunk._weu_offset;
    buf = chunk.compressedBuf_;
    len = chunk.compressedLen_;
#endif

#else  // ACDC
    addr = chunk.cachedataLocation_;
    buf = chunk.compressedBuf_;
    len = (chunk.compressedLevel_ + 1) * Config::getInstance()->getSectorSize();
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
#if !defined(WRITE_BACK_CACHE)
  if (generatePrimaryWriteRequest(chunk, deviceType, addr, buf, len)) {
    ioModule_->write(deviceType, addr, buf, len);
  }
#endif
  if (generateCacheWriteRequest(chunk, deviceType, addr, buf, len)) {
    ioModule_->write(deviceType, addr, buf, len);
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
