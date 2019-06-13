#include "managemodule.h"
namespace cache {

ManageModule::ManageModule(
  std::shared_ptr<IOModule> io_module,
  std::shared_ptr<MetadataModule> metadata_module) :
  _io_module(io_module), _metadata_module(metadata_module)
{}

int ManageModule::read(Chunk &c, LookupResult lookup_result, bool update_metadata)
{
  if (lookup_result == READ_HIT) {
    _io_module->read(0,
      c._ssd_location + Config::metadata_size,
      c._compressed_buf,
      c._compress_level * Config::sector_size
      );
  } else {
    _io_module->read(1, c._addr,
      c._buf,
      c._len);
    // update indexing
    if (update_metadata) {
      c.fingerprinting();
      _metadata_module->update(c, lookup_result);
    }
  }
}

int ManageModule::write(Chunk &c, LookupResult lookup_result)
{
  if (lookup_result == WRITE_DUP_WRITE) {
    // do nothing
  } else if (lookup_result == WRITE_DUP_CONTENT) {
    // write through to the primary storage
    _io_module->write(1, c._addr, c._buf, c._len);
    // and update metadata structure in metadata module
    _metadata_module->update(c, lookup_result);
  } else {
    // write through to the primary storage
    _io_module->write(1, c._addr, c._buf, c._len);
    // and update metadata structure in metadata module
    _metadata_module->update(c, lookup_result);
    _io_module->write(0,
      c._ssd_location + Config::metadata_size,
      c._compressed_buf,
      c._compress_level * Config::sector_size
      );
  }
}

}