#ifndef __BITMAP_H__
#define __BITMAP_H__
#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>
#include "utils/utils.h"
namespace cache {
  class Bitmap {
    public:
      struct Manipulator {
        explicit Manipulator(uint8_t *data) : data_(data) {}
        bool get(uint32_t index)
        {
          return data_[index >> 3u] & (1u << (index & 7u));
        }

        void set(uint32_t index)
        {
          data_[index >> 3u] |= (1u << (index & 7u));
        }

        void clear(uint32_t index)
        {
          data_[index >> 3u] &= ~(1u << (index & 7u));
        }

        inline void storeBits(uint32_t b, uint32_t e, uint32_t v) {
          uint32_t base = b >> 3u, base_e = e >> 3u;
          uint32_t shift_b = b & 7u, shift_e = e & 7u;
          if (base == base_e) {
            data_[base] &= ((1u << shift_b) - 1) | (~((1u << shift_e) - 1));
            data_[base] |= v << shift_b;
          } else {
            if (shift_b) {
              data_[base] &= (1u << shift_b) - 1;
              data_[base] |= (v << shift_b) & 0xffu;
              v >>= (8 - shift_b);
              ++base;
            }
            while (base < base_e) {
              data_[base] = v & 0xffu;
              v >>= 8u;
              ++base;
            }
            if (shift_e) {
              data_[base] &= ~((1u << shift_e) - 1);
              data_[base] |= v;
            }
          }
        }

        inline uint32_t getBits(uint32_t b, uint32_t e) {
          uint32_t v = 0;
          uint32_t base = b >> 3u, base_e = e >> 3u;
          uint32_t shift_b = b & 7u, shift_e = e & 7u;
          if (base == base_e) {
            v = data_[base] & ~(((1u << shift_b) - 1) | (~((1u << shift_e) - 1)));
            v >>= shift_b;
          } else {
            uint32_t shift = 0;
            if (shift_b) {
              v |= data_[base] >> shift_b;
              shift += (8 - shift_b);
              ++base;
            }
            while (base < base_e) {
              v |= data_[base] << shift;
              shift += 8;
              ++base;
            }
            v |= (data_[base] & ((1u << shift_e) - 1)) << shift;
          }
          return v;
        }

        inline uint32_t get32bits(uint32_t index) {
          return getBits(index * 32, (index + 1) * 32);
        }
        inline void set32bits(uint32_t index, uint32_t v) {
          storeBits(index * 32, (index + 1) * 32, v);
        }

        uint8_t *data_;
      };
      explicit Bitmap(uint32_t nBits) :
        nBits_(nBits),
        data_(std::make_unique<uint8_t[]>((nBits_ + 7) / 8))
      {
        memset(data_.get(), 0, (nBits_ + 7) / 8);
      }

      Manipulator getManipulator() {
        return Manipulator(data_.get());
      }

    public:
      uint32_t nBits_;
      std::unique_ptr< uint8_t[] > data_;
  };
}
#endif
