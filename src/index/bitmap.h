#ifndef __BITMAP_H__
#define __BITMAP_H__
#include <cstdint>
#include <cstring>
namespace cache {
  class Bitmap {
    public:
      Bitmap(uint32_t num) :
        _num(num),
        _data(new uint8_t[(_num + 7) / 8])
      {
        memset(_data, 0, (_num + 7) / 8);
      }

      bool get(uint32_t index)
      {
        return _data[index >> 3] & (1 << (index & 7));
      }

      void set(uint32_t index)
      {
        _data[index >> 3] |= (1 << (index & 7));
      }

      void clear(uint32_t index)
      {
        _data[index >> 3] &= ~(1 << (index & 7));
      }

    private:
      uint32_t _num;
      uint8_t *_data;
  };
}
#endif
