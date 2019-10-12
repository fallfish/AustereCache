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

  void Chunk::computeFingerprint() {
    BEGIN_TIMER();
    Config *conf = Config::getInstance();
    assert(len_ == conf->getChunkSize());
    assert(addr_ % conf->getChunkSize() == 0);
#ifdef REPLAY_FIU
    memcpy(fingerprint_, conf->get_current_fingerprint(), conf->get_ca_length());
#else
    if (conf->getFingerprintAlg() == 0) {
      SHA1(buf_, len_, fingerprint_);
    } else if (conf->getFingerprintAlg() == 1) {
      MurmurHash3_x64_128(buf_, len_, 0, fingerprint_);
    }
#endif
    hasFingerprint_ = true;

    // compute hash value of fingerprint
    uint64_t tmp[2];
    MurmurHash3_x64_128(fingerprint_, conf->getFingerprintLength(), 2, tmp);
    fingerprintHash_ = tmp[0];
    fingerprintHash_ >>= 64 - (conf->getnBitsPerFPSignature() + conf->getnBitsPerFPBucketId());
    END_TIMER(fingerprinting);
  }

  void Chunk::computeStrongFingerprint() {
    SHA1(buf_, len_, strongFingerprint_);
  }

  void Chunk::computeLBAHash()
  {
    Config *conf = Config::getInstance();
    uint64_t tmp[2];
    MurmurHash3_x64_128(&addr_, 8, 3, tmp);
    lbaHash_ = tmp[0];
    lbaHash_ >>= 64 - (conf->getnBitsPerLBASignature() + conf->getnBitsPerLBABucketId());
  }

  void Chunk::preprocessUnalignedChunk(uint8_t *buf) {
    Config *conf = Config::getInstance();
    originalAddr_ = addr_;
    originalLen_ = len_;
    originalBuf_ = buf_;

    addr_ = addr_ - addr_ % conf->getChunkSize();
    len_ = conf->getChunkSize();
    buf_ = buf;

    memset(buf, 0, conf->getChunkSize());
    computeLBAHash();
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::handleReadModifyWrite() {
    Config *conf = Config::getInstance();
    uint32_t chunk_size = conf->getChunkSize();
    assert(addr_ - addr_ % chunk_size == originalAddr_ - originalAddr_ % chunk_size);

    memcpy(buf_ + originalAddr_ % chunk_size, originalBuf_, originalLen_);
    len_ = chunk_size;
    addr_ -= addr_ % chunk_size;

    hasFingerprint_ = false;
    verficationResult_ = VERIFICATION_UNKNOWN;
    lookupResult_ = LOOKUP_UNKNOWN;
    computeLBAHash();
  }

  void Chunk::handleReadPartialChunk() {
    Config *conf = Config::getInstance();
    memcpy(originalBuf_, buf_ + originalAddr_ % conf->getChunkSize(), originalLen_);
  }

  Chunker::Chunker(uint64_t addr, void *buf, uint32_t len) :
    _chunk_size(Config::getInstance()->getChunkSize()),
    _addr(addr), _len(len), _buf((uint8_t*)buf)
  {}

  bool Chunker::next(Chunk &c)
  {
    if (_len == 0) return false;

    uint64_t next_addr = 
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) < (_addr + _len) ?
      ((_addr & ~(_chunk_size - 1)) + _chunk_size) : (_addr + _len);

    c.addr_ = _addr;
    c.len_ = next_addr - _addr;
    c.buf_ = _buf;
    c.hasFingerprint_ = false;

    c.lbaHash_ = -1LL;
    c.fingerprintHash_ = -1LL;
    c.hitLBAIndex_ = false;
    c.hitFPIndex_ = false;
    c.dedupResult_ = DEDUP_UNKNOWN;
    c.lookupResult_ = LOOKUP_UNKNOWN;
    c.verficationResult_ = VERIFICATION_UNKNOWN;
    c.compressedLevel_ = 0;
    c.computeLBAHash();

    _addr += c.len_;
    _buf += c.len_;
    _len -= c.len_;

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
  Chunker ChunkModule::createChunker(uint64_t addr, void *buf, uint32_t len)
  {
    Chunker chunker(addr, buf, len);
    return chunker;
  }
}
