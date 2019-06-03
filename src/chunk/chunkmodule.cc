#include "chunkmodule.h"
#include "common/config.h"
#include "utils/MurmurHash3.h"
#include <cstring>
#include <cassert>
namespace cache {

  void Chunk::fingerprinting() {
    assert(_len == Config::chunk_size);
    assert(_addr % Config::chunk_size == 0);
    MurmurHash3_x64_128(_buf, _len, 0, _ca);
    MurmurHash3_x86_32(&_addr, 4, 1, &_lba_hash);
    MurmurHash3_x86_32(_ca, 16, 2, &_ca_hash);
  }

  bool Chunk::is_partial() {
    return _len != Config::chunk_size;
  }

  Chunk Chunk::construct_read_chunk(uint8_t *buf) {
    Chunk c;
    c._addr = _addr - _addr % Config::chunk_size;
    c._len = Config::chunk_size;
    c._buf = buf;
    memset(buf, 0, Config::chunk_size);

    return c;
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::merge(const Chunk &c) {
    uint32_t chunk_size = Config::chunk_size;
    assert(_addr - _addr % chunk_size == c._addr - c._addr % chunk_size);

    memcpy(c._buf + _addr % chunk_size, _buf, _len);
    _len = chunk_size;
    _addr -= _addr % chunk_size;
    _buf = c._buf;
  }

  Chunker::Chunker(uint8_t *buf, uint32_t len, uint32_t addr)
  {
    _chunk_size = Config::chunk_size;
    _addr = addr;
    _len = len;
    _buf = buf;
    //_mode = mode;
  }

  bool Chunker::next(Chunk &c)
  {
    if (_len == 0) return false;

    uint32_t next_addr = 
      (_addr - _addr % _chunk_size + _chunk_size) - (_addr + _len) < 0 ?
      (_addr - _addr % _chunk_size + _chunk_size) : (_addr + _len);

    c._addr = _addr;
    c._len = next_addr - _addr;
    c._buf = _buf;

    _addr += c._len;
    _buf += c._len;
    _len -= c._len;

    return true;
  }

  ChunkModule::ChunkModule() {}
  Chunker create_chunker(uint32_t addr, uint32_t len, uint8_t *buf)
  {
    Chunker chunker(buf, len, addr);
    return chunker;
  }
}