#include "common/config.h"
#include "common/common.h"
#include "metaverification.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<cache::IOModule> io_module) :
    _io_module(io_module)
  {}

  VerificationResult MetaVerification::verify(uint32_t lba, uint8_t *ca, uint32_t ssd_location)
  {
    // read metadata from _io_module
    _io_module->read(0, ssd_location, &_metadata, 512);

    // verify the validity and return corresponding flag
    bool lba_valid = false, ca_valid = false;
    for (uint32_t i = 0; i < 64; i++) {
      if (lba == _metadata._lbas[i]) {
        lba_valid = true;
        break;
      }
    }
    if (ca != nullptr)
      ca_valid = (memcmp(ca, _metadata._ca, Config::ca_length) == 0);

    if (ca_valid && lba_valid)
      return VerificationResult::LBA_AND_CA_VALID;
    else if (ca_valid)
      return VerificationResult::ONLY_CA_VALID;
    else
      return VerificationResult::CA_NOT_VALID;
  }
}
