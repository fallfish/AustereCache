#ifndef __METADATAMODULE_COMMON_H__
#define __METADATAMODULE_COMMON_H__
#include <cstdint>
namespace cache {

// 512 bytes
struct Metadata {
  uint8_t _ca[256];
  uint32_t _reference_count;
  uint32_t _lbas[37]; // 4 * 32
};

enum LookupResult {
  WRITE_DUP_WRITE, WRITE_DUP_CONTENT, WRITE_NOT_DUP,
  READ_HIT, READ_NOT_HIT, LOOKUP_UNKNOWN
};

enum VerificationResult {
  LBA_AND_CA_VALID, ONLY_CA_VALID, CA_NOT_VALID, VERIFICATION_UNKNOWN
};

}


#endif //METADATAMODULE_COMMON_H
