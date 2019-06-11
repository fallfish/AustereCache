#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdint>
class Config
{
 public:
  static const uint32_t chunk_size = 8192; // 8k size chunk
  static const uint32_t sector_size = 8192; // 8k size sector
  static const uint32_t metadata_size = 512; // 512 byte size chunk
  static const uint32_t ca_length = 16; // content address length, 256 bytes if using sha256, else differs for different hash function
};

#endif
