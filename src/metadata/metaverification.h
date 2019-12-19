#ifndef __METAVERIFICATION_H__
#define __METAVERIFICATION_H__
#include "common/common.h"
#include <cstdint>
#include "io/iomodule.h"
#include "compression/compressionmodule.h"
#include "frequentslots.h"
#include "index.h"
namespace cache {
  class MetaVerification {
   public:
    MetaVerification(std::shared_ptr<LBAIndex> lbaIndex);
    VerificationResult verify(Chunk &chunk);
    void clean(Chunk &chunk);
    void update(Chunk &chunk);
   private:
    std::unique_ptr<FrequentSlots> frequentSlots_;
    std::shared_ptr<LBAIndex> lbaIndex_;
  };
}
#endif
