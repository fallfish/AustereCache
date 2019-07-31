#include "managemodule.h"
namespace cache {

ManageModule::ManageModule(
  std::shared_ptr<IOModule> io_module,
  std::shared_ptr<MetadataModule> metadata_module) :
  _io_module(io_module), _metadata_module(metadata_module)
{}

int ManageModule::read(Chunk &c)
{
  if (c._lookup_result == HIT) {
#ifdef CACHE_DEDUP
    _io_module->read(1,
        c._ssd_location,
        c._buf,
        c._len
        );
#else
    c._compressed_len = c._metadata._compressed_len;
    if (c._compressed_len != 0) {
      _io_module->read(1,
        c._ssd_location + Config::get_configuration().get_metadata_size(),
        c._compressed_buf,
        (c._compress_level + 1) * Config::get_configuration().get_sector_size()
        );
    } else {
      _io_module->read(1,
        c._ssd_location + Config::get_configuration().get_metadata_size(),
        c._buf,
        (c._compress_level + 1) * Config::get_configuration().get_sector_size()
        );
      c._compressed_buf = c._buf;
    }
#endif
  } else {
    _io_module->read(0, c._addr,
      c._buf,
      c._len);
  }

  return 0;
}

int ManageModule::write(Chunk &c)
{
  if (c._dedup_result == DUP_WRITE) {
    // do nothing
    return 0;
  } else if (
      // if lookup unknown, the request is a read request, no need to update
      // primary storage
      c._lookup_result == LOOKUP_UNKNOWN &&
      (c._dedup_result == DUP_CONTENT || c._dedup_result == NOT_DUP)
      ) {
    // write through to the primary storage
    _io_module->write(0, c._addr, c._buf, c._len);
  }
  if (c._dedup_result == NOT_DUP) {
#ifdef CACHE_DEDUP
    _io_module->write(1,
      c._ssd_location,
      c._buf,
      c._len
    );
#else
    _io_module->write(1,
      c._ssd_location + Config::get_configuration().get_metadata_size(),
      c._compressed_buf,
      (c._compress_level + 1) * Config::get_configuration().get_sector_size()
    );
#endif
  }
  return 0;
}

void ManageModule::update_metadata(Chunk &c)
{
  _metadata_module->update(c);
}

}
