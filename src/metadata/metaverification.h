#ifndef __METAVERIFICATION_H__
#define __METAVERIFICATION_H__
#include "common/common.h"
#include <cstdint>
#include "io/iomodule.h"
#include "compression/compressionmodule.h"
#include "frequentslots.h"
namespace cache {
  class MetaVerification {
   public:
    MetaVerification(std::shared_ptr<CompressionModule> compressionModule);
    VerificationResult verify(Chunk &chunk);
    void update(Chunk &chunk);
   private:
    // In single threaded where verify and update is strictly sequential,
    // we can read the corresponding metadata during verify
    // and update it then write with io_module
    // to avoid a second metadata read in update function.
//    uint32_t cachedataLocation_;

    std::unique_ptr<FrequentSlots> frequentSlots_;
    std::shared_ptr<CompressionModule> compressionModule_;
  };
}
#endif
