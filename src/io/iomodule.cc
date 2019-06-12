#include "iomodule.h"
#include "device/device.h"

namespace cache {

IOModule::IOModule()
{


}

uint32_t IOModule::add_cache_device(char *filename, uint32_t type)
{
  std::unique<BlockDevice> device = std::make_unique<BlockDevice>();
  device->open(filename);

  _cache_device = std::move(device);
}

uint32_t IOModule::add_primary_device(char *filename)
{

}

}