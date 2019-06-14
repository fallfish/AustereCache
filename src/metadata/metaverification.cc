#include "common/config.h"
#include "common/common.h"
#include "metaverification.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<cache::IOModule> io_module) :
    _io_module(io_module)
  {}

  VerificationResult MetaVerification::verify(uint64_t lba, uint8_t *ca, uint64_t ssd_location)
  { // read metadata from _io_module
    _io_module->read(1, ssd_location, &_metadata, 512);

    // verify the validity and return corresponding flag
    bool lba_valid = false, ca_valid = false;
    for (uint32_t i = 0; i < 37; i++) {
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
    else if (lba_valid) {
      return VerificationResult::ONLY_LBA_VALID;
    } else {
      return VerificationResult::BOTH_LBA_AND_CA_NOT_VALID;
    }
  }

  void MetaVerification::update(uint64_t lba, uint8_t *ca, uint64_t ssd_location)
  {
    if (ssd_location == _ssd_location) {
      if (_metadata._num_lbas == 37) {
        for (uint32_t i = 0; i < 36; i++)
          _metadata._lbas[i] = _metadata._lbas[i + 1];
        _metadata._lbas[36] = lba;
      }
    } else {
      memset(_metadata._lbas, 0, sizeof(_metadata._lbas));
      _metadata._num_lbas = 1;
      _metadata._lbas[0] = lba;
      memcpy(_metadata._ca, ca, Config::ca_length);
    }
    _io_module->write(1, ssd_location, &_metadata, 512);
  }
}
