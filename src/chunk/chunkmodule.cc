#include "chunkmodule.h"
#include "common/config.h"
#include "common/stats.h"
#include "utils/MurmurHash3.h"
#include "utils/utils.h"
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <cstring>
#include <cassert>

#include "metadata/validators.h"

namespace cache {

  //Chunk::Chunk() {}

  void Chunk::TEST_fingerprinting() {
    Config *conf = Config::get_configuration();
    assert(_len == conf->get_chunk_size());
    assert(_addr % conf->get_chunk_size() == 0);
    MurmurHash3_x64_128(_buf, _len, 0, _ca);
    MurmurHash3_x86_32(_ca, conf->get_ca_length(), 2, &_fp_hash);
    _has_ca = true;
  }
  void Chunk::TEST_compute_lba_hash()
  {
    Config *conf = Config::get_configuration();
    MurmurHash3_x86_32(&_addr, 8, 1, &_lba_hash);
  }

  void Chunk::fingerprinting() {
    BEGIN_TIMER();
    Config *conf = Config::get_configuration();
    assert(_len == conf->get_chunk_size());
    assert(_addr % conf->get_chunk_size() == 0);
#ifdef REPLAY_FIU
    memcpy(_ca, conf->get_current_fingerprint(), conf->get_ca_length());
#else
    if (conf->get_fingerprint_algorithm() == 0) {
      SHA1(_buf, _len, _ca);
    } else if (conf->get_fingerprint_algorithm() == 1) {
      MurmurHash3_x64_128(_buf, _len, 0, _ca);
    }
#endif
    _has_ca = true;

    // compute ca hash
    uint64_t tmp[2];
    MurmurHash3_x64_128(_ca, conf->get_ca_length(), 2, tmp);
    _fp_hash = tmp[0];
    _fp_hash >>= 64 - (conf->get_fp_signature_len() + conf->get_fp_bucket_no_len());
    END_TIMER(fingerprinting);
  }

  void Chunk::compute_strong_ca() {
    SHA1(_buf, _len, _strong_ca);
  }

  void Chunk::compute_lba_hash()
  {
    Config *conf = Config::get_configuration();
    uint64_t tmp[2];
    MurmurHash3_x64_128(&_addr, 8, 3, tmp);
    _lba_hash = tmp[0];
    _lba_hash >>= 64 - (conf->get_lba_signature_len() + conf->get_lba_bucket_no_len());
  }

  void Chunk::preprocess_unaligned(uint8_t *buf) {
    Config *conf = Config::get_configuration();
    _original_addr = _addr;
    _original_len = _len;
    _original_buf = _buf;

    _addr = _addr - _addr % conf->get_chunk_size();
    _len = conf->get_chunk_size();
    _buf = buf;

    memset(buf, 0, conf->get_chunk_size());
    compute_lba_hash();
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::merge_write() {
    Config *conf = Config::get_configuration();
    uint32_t chunk_size = conf->get_chunk_size();
    assert(_addr - _addr % chunk_size == _original_addr - _original_addr % chunk_size);

    memcpy(_buf + _original_addr % chunk_size, _original_buf, _original_len);
    _len = chunk_size;
    _addr -= _addr % chunk_size;

    _has_ca = false;
    _verification_result = VERIFICATION_UNKNOWN;
    _lookup_result = LOOKUP_UNKNOWN;
    compute_lba_hash();
  }

  void Chunk::merge_read() {
    Config *conf = Config::get_configuration();
    memcpy(_original_buf, _buf + _original_addr % conf->get_chunk_size(), _original_len);
  }

  Chunker::Chunker(uint64_t addr, void *buf, uint32_t len) :
    _chunk_size(Config::get_configuration()->get_chunk_size()),
    _addr(addr), _len(len), _buf((uint8_t*)buf)
  {}

  bool Chunker::next(Chunk &c)
  {
    if (_len == 0) return false;

    uint64_t next_addr = 
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) < (_addr + _len) ?
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) : (_addr + _len);

    c._addr = _addr;
    c._len = next_addr - _addr;
    c._buf = _buf;
    c._has_ca = false;

    c._lba_hash = -1LL;
    c._fp_hash = -1LL;
    c._lba_hit = false;
    c._ca_hit = false;
    c._dedup_result = DEDUP_UNKNOWN;
    c._lookup_result = LOOKUP_UNKNOWN;
    c._verification_result = VERIFICATION_UNKNOWN;
    c._compress_level = 0;
    c.compute_lba_hash();

    _addr += c._len;
    _buf += c._len;
    _len -= c._len;

    return true;
  }

  /**
   * (Commeted by jhli)
   * 1. Return the address of the next chunk;
   * 2. Move _addr, _buf to the next chunk, and decrease _len
   */
  bool Chunker::next(uint64_t &addr, uint8_t *&buf, uint32_t &len)
  {
    if (_len == 0) return false;


    uint64_t next_addr = 
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) < (_addr + _len) ?
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) : (_addr + _len);

    addr = _addr;
    len = next_addr - _addr;
    buf = _buf;

    _addr += len;
    _buf += len;
    _len -= len;

    return true;
  }

  /**
   * (Commented by jhli)
   * A factory of class "Chunker". Used to create a Chunker class
   */
  ChunkModule::ChunkModule() {}
  Chunker ChunkModule::create_chunker(uint64_t addr, void *buf, uint32_t len)
  {
    Chunker chunker(addr, buf, len);
    return chunker;
  }
}
