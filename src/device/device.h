#ifndef __DEVICE_H__
#define __DEVICE_H__
#include <cstdint>
#include "metadata/index.h"

namespace cache {
  class Device {
    public:
      virtual ~Device();
      virtual void write(uint32_t offset, uint32_t len, uint8_t* buf) = 0;
      virtual void read(uint32_t offset, uint32_t len, uint8_t* buf) = 0;
    protected:
      int _handler;
      void *_device_buffer;
  };

  class BlockDevice : public Device {
    public:
      ~BlockDevice();
      void write(uint32_t offset, uint32_t len, uint8_t* buf);
      void read(uint32_t offset, uint32_t len, uint8_t* buf);
      void open(char *filename);
  };

  /*
   * DeviceWithCache: manage a primary device and a cache device
   *  the main write/read path go through this class
   */
  class DeviceWithCache: public Device {
    public:
      DeviceWithCache(
          std::unique_ptr<Device> primary_device,
          std::unique_ptr<Device> cache_device,
          std::unique_ptr<LBAIndex> lba_index,
          std::unique_ptr<CAIndex> ca_index);
      void write(uint32_t offset, uint32_t len, uint8_t* buf);
      void read(uint32_t offset, uint32_t len, uint8_t* buf);

    private:
      std::unique_ptr<Device> _primary_device;
      std::unique_ptr<Device> _cache_device;
      std::unique_ptr<LBAIndex> _lba_index;
      std::unique_ptr<CAIndex> _ca_index;
  };

}


#endif
