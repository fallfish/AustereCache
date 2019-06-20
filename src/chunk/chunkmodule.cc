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

  void Chunk::preprocess_unaligned(uint8_t *buf) {
    _original_addr = _addr;
    _original_len = _len;
    _original_buf = _buf;

    _addr = _addr - _addr % Config::chunk_size;
    _len = Config::chunk_size;
    _buf = buf;

    memset(buf, 0, Config::chunk_size);
    compute_lba_hash();
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::merge_write() {
    uint32_t chunk_size = Config::chunk_size;
    assert(_addr - _addr % chunk_size == _original_addr - _original_addr % chunk_size);

    memcpy(_buf + _addr % chunk_size, _original_buf, _original_len);
    _len = chunk_size;
    _addr -= _addr % chunk_size;

    _has_ca = false;
    _verification_result = VERIFICATION_UNKNOWN;
    _lookup_result = LOOKUP_UNKNOWN;
    compute_lba_hash();
  }

  void Chunk::merge_read() {
    memcpy(_original_buf, _buf + _original_addr % Config::chunk_size, _original_len);
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

    c._lba_hit = false;
    c._ca_hit = false;
    c._verification_result = VERIFICATION_UNKNOWN;
    c._lookup_result = LOOKUP_UNKNOWN;
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
