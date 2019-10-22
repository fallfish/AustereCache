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
    assert(len_ == Config::getInstance().getChunkSize());
    assert(addr_ % Config::getInstance().getChunkSize() == 0);

#if defined(REPLAY_FIU) && defined(FAKE_IO)
    memcpy(fingerprint_, Config::getInstance().getCurrentFingerprint(), Config::getInstance().getFingerprintLength());
#elif defined(REPLAY_FIU) && !defined(FAKE_IO)
    memset(buf_, 0, Config::getInstance().getChunkSize());
    for (uint32_t offset = 0;
         offset + Config::getInstance().getFingerprintLength() < Config::getInstance().getChunkSize();
         offset += Config::getInstance().getFingerprintLength()) {
      memcpy(buf_, Config::getInstance().getCurrentFingerprint(), Config::getInstance().getFingerprintLength());
    }
    if (Config::getInstance().getFingerprintAlg() == 0) {
      SHA1(buf_, len_, fingerprint_);
    } else if (Config::getInstance().getFingerprintAlg() == 1) {
      MurmurHash3_x64_128(buf_, len_, 0, fingerprint_);
    }
#elif !defined(REPLAY_FIU) && !defined(FAKE_IO) // Normal workload
    if (Config::getInstance().getFingerprintAlg() == 0) {
      SHA1(buf_, len_, fingerprint_);
    } else if (Config::getInstance().getFingerprintAlg() == 1) {
      MurmurHash3_x64_128(buf_, len_, 0, fingerprint_);
    }
#endif
    hasFingerprint_ = true;

    // compute hash value of fingerprint
    uint64_t tmp[2];
    MurmurHash3_x64_128(fingerprint_, Config::getInstance().getFingerprintLength(), 2, tmp);
    fingerprintHash_ = tmp[0];
    fingerprintHash_ >>= 64 - (Config::getInstance().getnBitsPerFPSignature() + Config::getInstance().getnBitsPerFPBucketId());
    fingerprintHash_ =
      (((fingerprintHash_ >> Config::getInstance().getnBitsPerFPSignature()) % Config::getInstance().getnFPBuckets())
      << Config::getInstance().getnBitsPerFPSignature()) |
        (fingerprintHash_ & ((1u << Config::getInstance().getnBitsPerFPSignature()) - 1u));

    END_TIMER(fingerprinting);
  }

  void Chunk::computeStrongFingerprint() {
    SHA1(buf_, len_, strongFingerprint_);
  }

  void Chunk::computeLBAHash()
  {
    uint64_t tmp[2];
    MurmurHash3_x64_128(&addr_, 8, 3, tmp);
    lbaHash_ = tmp[0];
    lbaHash_ >>= 64 - (Config::getInstance().getnBitsPerLBASignature() + Config::getInstance().getnBitsPerLBABucketId());
    lbaHash_ =
      (((lbaHash_ >> Config::getInstance().getnBitsPerLBASignature()) % Config::getInstance().getnLBABuckets())
      << Config::getInstance().getnBitsPerFPSignature()) |
      (lbaHash_ & ((1u << Config::getInstance().getnBitsPerLBASignature()) - 1u));
  }

  void Chunk::preprocessUnalignedChunk(uint8_t *buf) {
    originalAddr_ = addr_;
    originalLen_ = len_;
    originalBuf_ = buf_;

    addr_ = addr_ - addr_ % Config::getInstance().getChunkSize();
    len_ = Config::getInstance().getChunkSize();
    buf_ = buf;

    memset(buf, 0, Config::getInstance().getChunkSize());
    computeLBAHash();
  }

  // used for read-modify-write case
  // the base chunk is an unaligned write chunk
  // the delta chunk is a read chunk
  void Chunk::handleReadModifyWrite() {
    uint32_t chunk_size = Config::getInstance().getChunkSize();
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
    memcpy(originalBuf_, buf_ + originalAddr_ % Config::getInstance().getChunkSize(), originalLen_);
  }

  Chunker::Chunker(uint64_t addr, void *buf, uint32_t len) :
    chunkSize_(Config::getInstance().getChunkSize()),
    addr_(addr), len_(len), buf_((uint8_t*)buf)
  {}

  bool Chunker::next(Chunk &c)
  {
    if (len_ == 0) return false;

    uint64_t next_addr =
      ((addr_ & ~(chunkSize_ - 1)) + chunkSize_) < (addr_ + len_) ?
      ((addr_ & ~(chunkSize_ - 1)) + chunkSize_) : (addr_ + len_);

    c.addr_ = addr_;
    c.len_ = next_addr - addr_;
    c.buf_ = buf_;
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

    addr_ += c.len_;
    buf_ += c.len_;
    len_ -= c.len_;

    return true;
  }

  /**
   * (Commeted by jhli)
   * 1. Return the address of the next chunk;
   * 2. Move _addr, _buf to the next chunk, and decrease _len
   */
  bool Chunker::next(uint64_t &addr, uint8_t *&buf, uint32_t &len)
  {
    if (len_ == 0) return false;


    uint64_t next_addr =
      ((addr_ & ~(chunkSize_ - 1)) + chunkSize_) < (addr_ + len_) ?
      ((addr_ & ~(chunkSize_ - 1)) + chunkSize_) : (addr_ + len_);

    addr = addr_;
    len = next_addr - addr_;
    buf = buf_;

    addr_ += len;
    buf_ += len;
    len_ -= len;

    return true;
  }

  /**
   * (Commented by jhli)
   * A factory of class "Chunker". Used to create a Chunker class
   */
  ChunkModule::ChunkModule() = default;
  Chunker ChunkModule::createChunker(uint64_t addr, void *buf, uint32_t len)
  {
    Chunker chunker(addr, buf, len);
    return chunker;
  }

  ChunkModule& ChunkModule::getInstance() {
    static ChunkModule instance;
    return instance;
  }
}
