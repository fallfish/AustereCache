#include "managemodule.h"
namespace cache {

ManageModule::ManageModule(
  std::shared_ptr<IOModule> io_module,
  std::shared_ptr<MetadataModule> metadata_module) :
  _io_module(io_module), _metadata_module(metadata_module)
{}

int ManageModule::read(Chunk &c)
{
  if (c._lookup_result == READ_HIT) {
    _io_module->read(1,
      c._ssd_location + Config::metadata_size,
      c._compressed_buf,
      c._compress_level * Config::sector_size
      );
    c._compressed_len = c._metadata._compressed_len;
  } else {
    //std::cout << "Not Hit " << 
      //c._addr << " " << c._len << std::endl;
    _io_module->read(0, c._addr,
      c._buf,
      c._len);
  }
  return 0;
}

int ManageModule::write(Chunk &c)
{
  if (c._lookup_result == WRITE_DUP_WRITE) {
    // do nothing
  } else if (c._lookup_result == WRITE_DUP_CONTENT || c._lookup_result == WRITE_NOT_DUP) {
    // write through to the primary storage
    _io_module->write(0, c._addr, c._buf, c._len);
  }
  if (c._lookup_result == WRITE_DUP_CONTENT
      || c._lookup_result == WRITE_NOT_DUP
      || c._lookup_result == READ_NOT_HIT ) {
    // and update metadata structure in metadata module
    _metadata_module->update(c);
  }
  if (c._lookup_result == WRITE_NOT_DUP
      || c._lookup_result == READ_NOT_HIT) {
    _io_module->write(1,
      c._ssd_location + Config::metadata_size,
      c._compressed_buf,
      c._compress_level * Config::sector_size
    );
  }
  return 0;
}

//void ManageModule::update_metadata(cache::Chunk &c, LookupResult lookup_result)
//{
//  // update indexing
//  c.fingerprinting();
//  _metadata_module->update(c, lookup_result);
//}

}
