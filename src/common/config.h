#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <ctype.h>
class Config
{
 public:
  static const uint32_t chunk_size = 8192; // 8k size chunk
  static const uint32_t sector_size = 8192; // 8k size sector
  static const uint32_t metadata_size = 512; // 512 byte size chunk
};

#endif
