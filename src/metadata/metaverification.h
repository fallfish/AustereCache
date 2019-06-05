#ifndef __METAVERIFICATION_H__
#define __METAVERIFICATION_H__

namespace cache {
  class MetaVerification {
   public:
    uint32_t verify(uint32_t lba, uint8_t *ca, uint32_t ssd_location);
   private:
     //std::shared_ptr<IOModule> _io_module;
  };
}
