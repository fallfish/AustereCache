#include "compression_module.h"
#include "common/config.h"
#include "lz4.h"

#include "common/stats.h"
#include "utils/utils.h"

#include <iostream>
#include <cstring>
#include <csignal>

namespace cache {
CompressionModule& CompressionModule::getInstance() {
  static CompressionModule instance;
  return instance;
}

void CompressionModule::compress(Chunk &chunk)
{
  BEGIN_TIMER();

#ifdef CDARC
  chunk.compressedLen_ = LZ4_compress_default(
      (const char*)chunk.buf_, (char*)chunk.compressedBuf_,
      chunk.len_, chunk.len_ - 1);
#else // ACDC
  chunk.compressedLen_ = LZ4_compress_default(
      (const char*)chunk.buf_, (char*)chunk.compressedBuf_,
      chunk.len_, chunk.len_ * 0.75);
#endif

  if (!Config::getInstance().isSynthenticCompressionEnabled()) {
    chunk.compressedLen_ = 0;
  }

#ifdef CDARC
  if (chunk.compressedLen_ == 0) {
    chunk.compressedLen_ = chunk.len_;
    chunk.compressedBuf_ = chunk.buf_;
  }
#else // ACDC
  uint32_t numCompressionLevels = Config::getInstance().getCompressionLevels();
  if (chunk.compressedLen_ == 0) {
    chunk.compressedLevel_ = numCompressionLevels - 1;
  } else {
    uint32_t compressedLen = chunk.compressedLen_;
    chunk.compressedLevel_ = 0;
    while (compressedLen > 0 && compressedLen <= chunk.compressedLen_) {
      chunk.compressedLevel_ += 1;
      compressedLen -= Config::getInstance().getSectorSize();
    }
    --chunk.compressedLevel_;
  }
  if (chunk.compressedLevel_ == numCompressionLevels - 1) {
    chunk.compressedBuf_ = chunk.buf_;
  }
#endif
  END_TIMER(compression);
}

void CompressionModule::decompress(Chunk &chunk)
{
  BEGIN_TIMER();

#if defined(CDARC)
  if (chunk.compressedLen_ != chunk.len_) {
#else // ACDC
  if (chunk.compressedLen_ != 0) {
#endif
    if (!Config::getInstance().isFakeIOEnabled()) {
      LZ4_decompress_safe((const char*)chunk.compressedBuf_, (char*)chunk.buf_,
                          chunk.compressedLen_, chunk.len_);
    }
  }
  END_TIMER(decompression);
}

// Not used now
void CompressionModule::decompress(uint8_t *compressedBuf, uint8_t *buf, uint32_t compressedLen, uint32_t originalLen)
{
  BEGIN_TIMER();
#if defined(CDARC)
  if (compressedLen != originalLen) {
#else
  if (compressedLen != 0) {
#endif
    if (!Config::getInstance().isFakeIOEnabled()) {
      int res = LZ4_decompress_safe((const char*)compressedBuf, (char*)buf,
                                    compressedLen, originalLen);
    }
  } else {
    if (!Config::getInstance().isFakeIOEnabled()) {
      memcpy(buf, compressedBuf, originalLen);
    }
  }
  END_TIMER(decompression);
}

}
