#include "common/config.h"
#include "common/common.h"
#include "metaverification.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<cache::IOModule> io_module) :
    _io_module(io_module)
  {}

  VerificationResult MetaVerification::verify(Chunk &c)
  {
    // read metadata from _io_module
    uint64_t &lba = c._addr;
    auto &ca = c._ca;
    uint64_t &ssd_location = c._ssd_location;
    Metadata &metadata = c._metadata;
    _io_module->read(1, ssd_location, &metadata, 512);

    // check lba
    bool lba_valid = false;
    if (metadata._num_lbas > 1) {
      uint32_t o = c._addr;
//      std::cout << "number of lbas: " << metadata._num_lbas << std::endl;
    }
    for (uint32_t i = 0; i < metadata._num_lbas; i++) {
      if (metadata._lbas[i] == c._addr) {
        lba_valid = true;
        break;
      }
    }

    // check ca
    // ca is valid only ca is valid and also
    // the content is the same by memcmp
    bool ca_valid = false;
    if (c._has_ca && memcmp(metadata._ca, c._ca, Config::ca_length) == 0)
      ca_valid = true;

    if (lba_valid && ca_valid)
      return BOTH_LBA_AND_CA_VALID;
    else if (lba_valid)
      return ONLY_LBA_VALID;
    else if (ca_valid)
      return ONLY_CA_VALID;
    else
      return BOTH_LBA_AND_CA_NOT_VALID;
  }

  void MetaVerification::update(Chunk &c)
  {
    uint64_t &lba = c._addr;
    auto &ca = c._ca;
    uint64_t &ssd_location = c._ssd_location;
    Metadata &metadata = c._metadata;
    VerificationResult verification_result = VERIFICATION_UNKNOWN;
    if (c._lookup_result == READ_NOT_HIT) {
       verification_result = verify(c);
    }
    if (verification_result == BOTH_LBA_AND_CA_VALID)
      return ;
    if (verification_result == ONLY_CA_VALID ||
        c._lookup_result == WRITE_DUP_CONTENT) {
      if (metadata._num_lbas == 37) {
        for (uint32_t i = 0; i < 36; i++) {
          metadata._lbas[i] = metadata._lbas[i + 1];
        }
        metadata._lbas[36] = c._addr;
      } else {
        metadata._lbas[metadata._num_lbas++] = c._addr;
      }
    } else {
      memset(&metadata, 0, sizeof(metadata));
      memcpy(metadata._ca, c._ca, Config::ca_length);
      metadata._lbas[0] = c._addr;
      metadata._num_lbas = 1;
      metadata._compressed_len = c._compressed_len;
    }
    _io_module->write(1, ssd_location, &metadata, 512);
  }
}
