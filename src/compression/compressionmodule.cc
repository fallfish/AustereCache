#include "compressionmodule.h"
#include "common/config.h"
#include "lz4.h"

#include "common/stats.h"
#include "utils/utils.h"

#include <iostream>
#include <cstring>
#include <csignal>

namespace cache {
CompressionModule& CompressionModule::getInstance() {
  CompressionModule instance;
  return instance;
}

void CompressionModule::compress(Chunk &chunk)
{
  BEGIN_TIMER();

#if defined(CDARC)
  chunk.compressedLen_ = LZ4_compress_default(
      (const char*)chunk.buf_, (char*)chunk.compressedBuf_,
      chunk.len_, chunk.len_ - 1);
  if (chunk.compressedLen_ == 0) {
    chunk.compressedLen_ = chunk.len_;
    chunk.compressedBuf_ = chunk.buf_;
  }
#else // ACDC
#if defined(FAKE_IO)
  chunk.compressedLen_ = 0;
#else
  chunk.compressedLen_ = LZ4_compress_default(
    (const char*)chunk.buf_, (char*)chunk.compressedBuf_,
    chunk.len_, chunk.len_ * 0.75);
#endif
  double compress_ratio = chunk.compressedLen_ * 1.0 / chunk.len_;
  if (compress_ratio > 0.75 || chunk.compressedLen_ == 0) {
    chunk.compressedLevel_ = 3;
    chunk.compressedBuf_ = chunk.buf_;
  } else {
    if (compress_ratio > 0.5) {
      chunk.compressedLevel_ = 2;
    } else if (compress_ratio > 0.25) {
      chunk.compressedLevel_ = 1;
    } else {
      chunk.compressedLevel_ = 0;
    }
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

#if defined(NORMAL_DIST_COMPRESSION)
    chunk.compressedLen_ = Config::getInstance().getCurrentCompressedLen();
    memcpy(chunk.compressedBuf_, Config::getInstance().getCurrentData(), chunk.compressedLen_);
#endif

#if !defined(FAKE_IO)
    LZ4_decompress_safe((const char*)chunk.compressedBuf_, (char*)chunk.buf_,
                        chunk.compressedLen_, chunk.len_);
#endif
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
#if !defined(FAKE_IO)
    int res = LZ4_decompress_safe((const char*)compressedBuf, (char*)buf,
                                  compressedLen, originalLen);
#endif
  } else {
    memcpy(buf, compressedBuf, originalLen);
  }
  END_TIMER(decompression);
}

}
