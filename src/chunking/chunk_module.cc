#include "chunk_module.h"
#include "common/config.h"
#include "common/stats.h"
#include "utils/xxhash.h"
#include "utils/utils.h"
#include <isa-l_crypto.h>
#include <cstring>
#include <cassert>

namespace cache {

  void Chunk::computeFingerprint() {
    BEGIN_TIMER();
    assert(len_ == Config::getInstance().getChunkSize());
    assert(addr_ % Config::getInstance().getChunkSize() == 0);

    struct mh_sha1_ctx ctx;
    if (Config::getInstance().isTraceReplayEnabled()) {
      if (!Config::getInstance().isFakeIOEnabled()) {
        // If not enable Fake IO, we need to conduct the FP computation
        mh_sha1_init(&ctx);
        mh_sha1_update(&ctx, buf_, len_);
        mh_sha1_finalize(&ctx, fingerprint_);
      }
      Config::getInstance().getFingerprint(addr_, (char*)fingerprint_);
    } else {
      mh_sha1_init(&ctx);
      mh_sha1_update(&ctx, buf_, len_);
      mh_sha1_finalize(&ctx, fingerprint_);
    }
    hasFingerprint_ = true;

    // compute hash value of fingerprint
    fingerprintHash_ = computeFingerprintHash(fingerprint_);
    END_TIMER(fingerprinting);
  }

  uint64_t Chunk::computeFingerprintHash(uint8_t *fingerprint) {
    uint32_t signature, bucketId;
    bucketId = XXH32(fingerprint, Config::getInstance().getFingerprintLength(), 2);
    signature = XXH32(fingerprint, Config::getInstance().getFingerprintLength(), 101);
    return ((uint64_t)(bucketId % Config::getInstance().getnFpBuckets()) << Config::getInstance().getnBitsPerFpSignature())
      | (uint64_t)((signature & ((1u << Config::getInstance().getnBitsPerFpSignature()) - 1u)));
  }

  uint64_t Chunk::computeLBAHash(uint64_t addr)
  {
    uint64_t lbaHash = XXH64(&addr, 8, 3);
    lbaHash >>= 64 - (Config::getInstance().getnBitsPerLbaSignature() + Config::getInstance().getnBitsPerLbaBucketId());
    return
      ((uint64_t)((lbaHash >> Config::getInstance().getnBitsPerLbaSignature()) % Config::getInstance().getnLbaBuckets())
      << Config::getInstance().getnBitsPerLbaSignature()) |
      (lbaHash & ((1u << Config::getInstance().getnBitsPerLbaSignature()) - 1u));
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

    c.lbaHash_ = ~0ull;
    c.fingerprintHash_ = ~0ull;
    c.hitLBAIndex_ = false;
    c.hitFPIndex_ = false;
    c.dedupResult_ = DEDUP_UNKNOWN;
    c.lookupResult_ = LOOKUP_UNKNOWN;
    c.verficationResult_ = VERIFICATION_UNKNOWN;
    c.nSubchunks_ = 0;
    c.lbaHash_ = Chunk::computeLBAHash(c.addr_);

    addr_ += c.len_;
    buf_ += c.len_;
    len_ -= c.len_;

    return true;
  }

  /**
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
