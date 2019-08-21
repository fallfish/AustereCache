#include "managemodule.h"
#include "common/stats.h"
#include "utils/utils.h"
#include <cassert>
namespace cache {

ManageModule::ManageModule(
  std::shared_ptr<IOModule> io_module,
  std::shared_ptr<MetadataModule> metadata_module) :
  _io_module(io_module), _metadata_module(metadata_module)
{
#if defined(CDARC)
  _weu_size = Config::get_configuration().get_write_buffer_size();
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

void ManageModule::preprocess_read(Chunk &c, uint32_t &device_no, uint64_t &addr, uint8_t *&buf, uint32_t &len)
{
  if (c._lookup_result == HIT) {
#if defined(CACHE_DEDUP)

#if defined(DLRU) || defined(DARC)
    device_no = 1;
    addr = c._ssd_location;
    buf = c._buf;
    len = c._len;
#elif defined(CDARC)
    if (_current_weu_id == c._weu_id) {
      device_no = 2;
      addr = c._weu_offset;
    } else {
      device_no = 1;
      addr = _weu_to_ssd_location[c._weu_id] + c._weu_offset;
    }
    if (c._compressed_len == c._len) {
      buf = c._buf;
    } else {
      buf = c._compressed_buf;
    }
    len = c._compressed_len;
#endif

#else // ACDC
    device_no = 1;
    c._compressed_len = c._metadata._compressed_len;
    if (c._compressed_len != 0) {
      buf = c._compressed_buf;
    } else {
      buf = c._buf;
    }
    addr = c._ssd_location + Config::get_configuration().get_metadata_size();
    len = (c._compress_level + 1) * Config::get_configuration().get_sector_size();
#endif
  } else {
    device_no = 0;
    addr = c._addr;
    buf = c._buf;
    len = c._len;
  }

}

int ManageModule::read(Chunk &c)
{
  uint32_t device_no;
  uint64_t addr;
  uint8_t *buf;
  uint32_t len;
  preprocess_read(c, device_no, addr, buf, len);
  _io_module->read(device_no, addr, buf, len);

  return 0;
}

bool ManageModule::preprocess_write_primary(
    Chunk &c, uint32_t &device_no,
    uint64_t &addr, uint8_t *&buf, uint32_t &len)
{
  device_no = 0;
  if ( c._lookup_result == LOOKUP_UNKNOWN &&
      (c._dedup_result == DUP_CONTENT || c._dedup_result == NOT_DUP)) {
    addr = c._addr;
    buf = c._buf;
    len = c._len;
    return true;
  }

  return false;
}

bool ManageModule::preprocess_write_cache(
    Chunk &c, uint32_t &device_no,
    uint64_t &addr, uint8_t *&buf, uint32_t &len)
{
  device_no = 1;
  if (c._dedup_result == NOT_DUP) {
#if defined(CACHE_DEDUP)

#if defined(DLRU) || defined(DARC)
    addr = c._ssd_location;
    buf = c._buf;
    len = c._len;
#elif defined(CDARC)
    uint64_t evicted_ssd_location = -1;
    if (_current_weu_id != c._weu_id) {
      if (c._evicted_weu_id != _current_weu_id) {
        if (c._evicted_weu_id != ~0) {
          evicted_ssd_location = _weu_to_ssd_location[c._evicted_weu_id];
          _weu_to_ssd_location.erase(c._evicted_weu_id);

          _io_module->flush(evicted_ssd_location);
          _weu_to_ssd_location[_current_weu_id] = evicted_ssd_location;
        } else {
          _io_module->flush(_current_ssd_location);
          _weu_to_ssd_location[_current_weu_id] = _current_ssd_location;
          _current_ssd_location += _weu_size;
        }
      }

      _current_weu_id = c._weu_id;
    }
    device_no = 2;
    addr = c._weu_offset;
    buf = c._compressed_buf;
    len = c._compressed_len;
#endif

#else  // ACDC
    addr = c._ssd_location + Config::get_configuration().get_metadata_size();
    buf = c._compressed_buf;
    len = (c._compress_level + 1) * Config::get_configuration().get_sector_size();
#endif
    return true;
  }
  return false;
}

int ManageModule::write(Chunk &c)
{
  uint32_t device_no;
  uint64_t addr;
  uint8_t *buf;
  uint32_t len;
#if !defined(WRITE_BACK_CACHE)
  if (preprocess_write_primary(c, device_no, addr, buf, len)) {
    //threadpool.doJob([this, device_no, addr, buf, len]() {
        _io_module->write(device_no, addr, buf, len);
        //});
  }
#endif
  if (preprocess_write_cache(c, device_no, addr, buf, len)) {
    //threadpool.doJob([this, device_no, addr, buf, len]() {
        _io_module->write(device_no, addr, buf, len);
        //});
  }
  return 0;
}

void ManageModule::update_metadata(Chunk &c)
{
  _metadata_module->update(c);
}

}
