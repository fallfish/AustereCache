#include "chunkmodule.h"
#include "common/config.h"
#include "utils/MurmurHash3.h"
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <cstring>
#include <cassert>
namespace cache {

  void Chunk::fingerprinting() {
    assert(_len == Config::chunk_size);
    assert(_addr % Config::chunk_size == 0);
//    SHA1(_buf, _len, _ca);
//    MurmurHash3_x86_32(_ca, 20, 2, &_ca_hash);
    MurmurHash3_x64_128(_buf, _len, 0, _ca);
    MurmurHash3_x86_32(_ca, 16, 2, &_ca_hash);
//    MD5(_buf, _len, _ca);
//    MurmurHash3_x86_32(_ca, 6, 2, &_ca_hash);
    _has_ca = true;
    _ca_hash >>= 32 - (Config::ca_signature_len + Config::ca_bucket_no_len);
  }

  void Chunk::compute_lba_hash()
  {
    MurmurHash3_x86_32(&_addr, 4, 1, &_lba_hash);
    _lba_hash >>= 32 - (Config::lba_signature_len + Config::lba_bucket_no_len);
  }

  bool Chunk::is_partial() {
    return _len != Config::chunk_size;
  }

  Chunk Chunk::construct_read_chunk(uint8_t *buf) {
    Chunk c;
    c._addr = _addr - _addr % Config::chunk_size;
    c._len = Config::chunk_size;
    c._buf = buf;
    c._has_ca = false;
    memset(buf, 0, Config::chunk_size);
    c.compute_lba_hash();

    return c;
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::merge_write(const Chunk &c) {
    uint32_t chunk_size = Config::chunk_size;
    assert(_addr - _addr % chunk_size == c._addr - c._addr % chunk_size);

    memcpy(c._buf + _addr % chunk_size, _buf, _len);
    _len = chunk_size;
    _addr -= _addr % chunk_size;
    _buf = c._buf;
    compute_lba_hash();
  }

  void Chunk::merge_read(const Chunk &c) {
    memcpy(_buf, c._buf + _addr % Config::chunk_size, _len);
  }

  Chunker::Chunker(uint64_t addr, void *buf, uint32_t len) :
    _chunk_size(Config::chunk_size),
    _addr(addr), _len(len), _buf((uint8_t*)buf)
  {}

  bool Chunker::next(Chunk &c)
  {
    if (_len == 0) return false;

    uint32_t next_addr = 
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) < (_addr + _len) ?
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) : (_addr + _len);

    c._addr = _addr;
    c._len = next_addr - _addr;
    c._buf = _buf;
    c._has_ca = false;
    c.compute_lba_hash();

    _addr += c._len;
    _buf += c._len;
    _len -= c._len;

    return true;
  }

  ChunkModule::ChunkModule() {}
  Chunker ChunkModule::create_chunker(uint64_t addr, void *buf, uint32_t len)
  {
    Chunker chunker(addr, buf, len);
    return chunker;
  }
}
