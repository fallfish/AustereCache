#include "common/common.h"
#include "metaverification.h"
#include "cache_policy.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<cache::IOModule> io_module,
      std::shared_ptr<CompressionModule> compression_module) :
    _io_module(io_module), _compression_module(compression_module)
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
    // Compute SHA1 and Compare Full fingerprint
    if (c._has_ca && memcmp(
          metadata._ca, c._ca,
          Config::get_configuration().get_ca_length()) == 0)
      ca_valid = true;

    // To release computation burden for fingerprint computation
    // We come up with three methods to compare the fingerprint value
    // Method 1: Compute a strong fingerprint (SHA1) which could be slow
    // Method 2: Compute a weak fingerprint (xxhash).
    //           To avoid higher collision rate,
    //           when matched, fetch real data from the cache, decompress,
    //           and do memcmp.
    // Method 3: Compute a weak fingerprint.
    //           To avoid higher collision rate,
    //           when matched, compute a strong fingerprint (SHA1)
    //           and compare again against the strong fingerprint.
    //{
      //if (ca_valid == true) {
        //uint32_t chunk_size = Config::get_configuration().get_chunk_size(),
                 //metadata_size = Config::get_configuration().get_metadata_size(),
                 //sector_size = Config::get_configuration().get_sector_size();
        //uint32_t fingerprint_computation_method = Config::get_configuration().get_fingerprint_computation_method();

        //if (fingerprint_computation_method == 1) {
        //// Method 1: use weak hash + memcmp + decompression
          //uint8_t compressed_data[chunk_size];
          //uint8_t decompressed_data[chunk_size];
          //// Fetch compressed data, decompress, and compare data content
          //if (metadata._compressed_len != 0) {
            //_io_module->read(1, ssd_location + 512, compressed_data,
                //(metadata._compressed_len + sector_size - 1) / sector_size * sector_size);
            //_compression_module->decompress(compressed_data, decompressed_data,
                //metadata._compressed_len, chunk_size);
          //} else {
            //_io_module->read(1, ssd_location + 512, decompressed_data, chunk_size);
          //}
          //ca_valid = (memcmp(decompressed_data, c._buf, chunk_size) == 0);
        //} else if (fingerprint_computation_method == 2) {
        //// Method 2: use weak hash + strong hash
          //c.compute_strong_ca();
          //ca_valid = (memcmp(c._strong_ca, metadata._strong_ca, 
                //Config::get_configuration().get_strong_ca_length()) == 0);
        //}
      //}
    //}

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

    if (c._dedup_result == DUP_CONTENT) {
      if (c._verification_result == BOTH_LBA_AND_CA_VALID) {
        return ;
      }
      if (metadata._num_lbas >= 37) {
        if (metadata._num_lbas == 37)
          _frequent_slots->allocate(c._ca_hash);
        _frequent_slots->add(c._ca_hash, c._addr);
      } else {
        metadata._lbas[metadata._num_lbas] = c._addr;
      }
      metadata._num_lbas++;
      _io_module->write(1, ssd_location, &c._metadata, 512);
    } else {
      _frequent_slots->remove(c._ca_hash);
      memset(&metadata, 0, sizeof(metadata));
      memcpy(metadata._ca, c._ca, Config::get_configuration().get_ca_length());
      metadata._lbas[0] = c._addr;
      metadata._num_lbas = 1;
      metadata._compressed_len = c._compressed_len;
      if (Config::get_configuration().get_fingerprint_computation_method() == 2) {
        c.compute_strong_ca();
        memcpy(metadata._strong_ca, c._strong_ca, Config::get_configuration().get_strong_ca_length());
      }
      _io_module->write(1, ssd_location, &c._metadata, 512);
    }
  }
}
