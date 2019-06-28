#include "common/common.h"
#include "metaverification.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<cache::IOModule> io_module) :
    _io_module(io_module)
  {
    _frequent_slots = std::make_unique<FrequentSlots>();
  }

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
    if (metadata._num_lbas > 37) {
      lba_valid = _frequent_slots->query(c._ca_hash, c._addr);
    }

    // check ca
    // ca is valid only ca is valid and also
    // the content is the same by memcmp
    bool ca_valid = false;
    if (c._has_ca && memcmp(
          metadata._ca, c._ca,
          Config::get_configuration().get_ca_length()) == 0)
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
    c._verification_result = VERIFICATION_UNKNOWN;
    if (c._lookup_result == READ_NOT_HIT) {
       c._verification_result = verify(c);
    }
    if (c._verification_result == BOTH_LBA_AND_CA_VALID)
      return ;
    if (c._verification_result == ONLY_CA_VALID ||
        c._lookup_result == WRITE_DUP_CONTENT) {
      if (metadata._num_lbas >= 37) {
        if (metadata._num_lbas == 37)
          _frequent_slots->allocate(c._ca_hash);
        _frequent_slots->add(c._ca_hash, c._addr);
      } else {
        metadata._lbas[metadata._num_lbas] = c._addr;
      }
      metadata._num_lbas++;
    } else {
      _frequent_slots->remove(c._ca_hash);
      memset(&metadata, 0, sizeof(metadata));
      memcpy(metadata._ca, c._ca, Config::get_configuration().get_ca_length());
      metadata._lbas[0] = c._addr;
      metadata._num_lbas = 1;
      metadata._compressed_len = c._compressed_len;
      _io_module->write(1, ssd_location, &c._metadata, 512);
    }
    _io_module->write(1, ssd_location, &c._metadata, 512);
  }
}
