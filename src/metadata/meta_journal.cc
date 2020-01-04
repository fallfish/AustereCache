#include "meta_journal.h"
#include "metadata_module.h"
namespace cache {

MetaJournal::MetaJournal() {
  journal_ = fopen("./journal", "w+");
}

void MetaJournal::addUpdate(const cache::Chunk &c)
{
  if (Config::getInstance().isJournalEnabled()) {
    if ((c.dedupResult_ == DUP_CONTENT && c.verficationResult_ == ONLY_FP_VALID)
      || c.dedupResult_ == NOT_DUP) {
      //std::cout << c.addr_  << " " << c.cachedataLocation_ << std::endl;
      fwrite(&c.addr_, 8, sizeof(uint8_t), journal_);
      fwrite(&c.cachedataLocation_, 8, sizeof(uint8_t), journal_);
      fflush(journal_);
    }
  }
}

void MetaJournal::recover() {
  uint64_t lba, cachedataLocation;
  FILE *journal = fopen("./journal_", "r");
  std::vector<std::pair<uint64_t, uint64_t>> vec;
  std::vector<std::pair<uint64_t, uint64_t>> validPairs;
  std::set<uint64_t> lbas;
  while (true) {
    int n = fread(&lba, 8, sizeof(uint8_t), journal);
    //std::cout << n << std::endl;
    if (n == 0) break;
    n = fread(&cachedataLocation, 8, sizeof(uint8_t), journal);
    if (n == 0) break;
    //std::cout << lba  << " " << cachedataLocation << std::endl;
    vec.push_back(std::make_pair(lba, cachedataLocation));
  }
  for (int i = vec.size() - 1; i >= 0; --i) {
    if (lbas.find(vec[i].first) != lbas.end()) {
      continue;
    }
    validPairs.push_back(vec[i]);
    lbas.insert(vec[i].first);
  }

  std::map<uint64_t, Metadata> metadatas;
  for (int i = validPairs.size() - 1; i >= 0; --i) {
    std::pair<uint64_t, uint64_t> &pr = validPairs[i];
    uint64_t lba = pr.first;
    uint64_t cachedataLocation = pr.second;
    uint64_t metadataLocation = FPIndex::cachedataLocationToMetadataLocation(cachedataLocation);

    alignas(512) Metadata metadata;
    if (metadatas.find(metadataLocation) != metadatas.end()) {
      metadata = metadatas[metadataLocation];
    } else {
      IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, Config::getInstance().getMetadataSize());
      metadatas[metadataLocation] = metadata;
    }

    bool find = false;
    for (uint32_t i = 0; i < metadata.numLBAs_; ++i) {
      if (metadata.LBAs_[i] == lba) {
        find = true;
        break;
      }
    }

    if (find) {
      uint64_t lbaHash = Chunk::computeLBAHash(lba);
      uint64_t fpHash = Chunk::computeFingerprintHash(metadata.fingerprint_);
      uint32_t nSlotsOccupied = (metadata.compressedLen_ == 0) ? 
        Config::getInstance().getCompressionLevels() :
        (metadata.compressedLen_ + Config::getInstance().getSectorSize()) / Config::getInstance().getSectorSize();
      MetadataModule::getInstance().recoverFpIndex(fpHash, cachedataLocation, nSlotsOccupied);
      MetadataModule::getInstance().recoverLbaIndex(lbaHash, fpHash);
    }
  }
}
}
