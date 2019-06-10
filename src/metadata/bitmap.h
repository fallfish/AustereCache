#ifndef __BITMAP_H__
#define __BITMAP_H__
#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>
namespace cache {
  class Bitmap {
    public:
      Bitmap(uint32_t num) :
        _num(num),
        _data(std::make_unique<uint8_t[]>((_num + 7) / 8))
      {
        memset(_data.get(), 0, (_num + 7) / 8);
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

      inline void store_bits(uint32_t b, uint32_t e, uint32_t v) {
        uint32_t base = b >> 3, base_e = e >> 3;
        uint32_t shift_b = b & 7, shift_e = e & 7;
        if (base == base_e) {
          _data[base] &= ((1 << shift_b) - 1) | (~((1 << shift_e) - 1));
          _data[base] |= v << shift_b;
        } else {
          //std::cout << "store v: " << v << std::endl;
          if (shift_b) {
            _data[base] &= (1 << shift_b) - 1;
            _data[base] |= (v << shift_b) & 0xff;
            v >>= (8 - shift_b);
            ++base;
          }
          while (base < base_e) {
            _data[base] = v & 0xff;
            v >>= 8;
            ++base;
          }
          if (shift_e) {
            _data[base] &= ~((1 << shift_e) - 1);
            _data[base] |= v;
          }
          //std::cout << "0: " << (uint32_t)_data[0] << " 1: " << (uint32_t)_data[1] << std::endl;
        }
      }

      inline uint32_t get_bits(uint32_t b, uint32_t e) {
        uint32_t v = 0;
        uint32_t base = b >> 3, base_e = e >> 3;
        uint32_t shift_b = b & 7, shift_e = e & 7;
        if (base == base_e) {
          v = _data[base] & ~(((1 << shift_b) - 1) | (~((1 << shift_e) - 1)));
          v >>= shift_b;
        } else {
          //std::cout << "base: " << base << " base_e: " << base_e << std::endl;
          //std::cout << "shift_b: " << shift_b << " shift_e: " << shift_e << std::endl;
          uint32_t shift = 0;
          if (shift_b) {
            v |= _data[base] >> shift_b;
            shift += (8 - shift_b);
            ++base;
          }
          //std::cout << "v: " << v << " base: " << base << std::endl;
          while (base < base_e) {
            v |= _data[base] << shift;
            shift += 8;
            ++base;
          }
          v |= (_data[base] & ((1 << shift_e) - 1)) << shift;
          //std::cout << "v: " << v << " base: " << base << std::endl;
        }
        return v;
      }
    private:
      uint32_t _num;
      std::unique_ptr< uint8_t[] > _data;
  };
}
#endif
