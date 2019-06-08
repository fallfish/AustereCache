#include "metaverification.h"
#include "metadatamodule.h"

namespace cache {
  uint32_t MetaVerification::verify(uint32_t lba, uint8_t *ca, uint32_t ssd_location)
  {
    // read metadata from _io_module
    Metadata metadata;
    _io_module->read(ssd_location, &metadata);

    // verify the validity and return corresponding flag
    bool lba_valid = false, ca_valid = false;
    for (uint32_t i = 0; i < 64; i++) {
      if (lba == metadata.lbas[i]) {
        lba_valid = true;
        break;
      }
    }
    ca_valid = (memcpy(ca, metadata.ca, 16) == 0);

    if (ca_valid && lba_valid)
      return 2;
    else if (ca_valid)
      return 1;
    else
      return 0;
  }
}
