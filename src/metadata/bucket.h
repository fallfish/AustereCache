#ifndef __BUCKET_H__
#define __BUCKET_H__
#include <cstdint>
#include <iostream>
#include <memory>
namespace cache {
  // Bucket is an abstraction of multiple key-value pairs (mapping)
  // bit level bucket
  class Bucket {
    public:
      Bucket(uint32_t n_bits_per_item, uint32_t n_items, uint32_t n_total_bytes);
      virtual ~Bucket();
      virtual int find(uint8_t *key, uint8_t *value) = 0;
      virtual void insert(uint8_t *key, uint8_t *value) = 0;

    protected:
      inline void bits_extract(uint32_t b, uint32_t e, uint32_t &v);
      inline void bits_encode(uint32_t b, uint32_t e, uint32_t v);
      uint32_t _n_bits_per_item, _n_items, _n_total_bytes;
      std::unique_ptr< uint8_t[] > _data;
  };

  /*
   * LBABucket: store multiple entries
   *            (lba signature -> ca signature + ca bucket index) pair
   *   find (ca signature + ca bucket index) given lba signature
   *   insert lba signature to (ca signature + ca bucket index)
   */
  class LBABucket : public Bucket {
    public:
      LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      ~LBABucket();

      int find(uint8_t *key, uint8_t *value);
      void insert(uint8_t *key, uint8_t *value);
    private:
      int _lru_pos;
      uint32_t _n_bits_per_key, _n_bits_per_value;
  };

  /*
   * CABucket: store multiple (ca signature -> ca bucket) pair
   */
  class CABucket : public Bucket {
    public:
      CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      ~CABucket();

      int find(uint8_t *key, uint8_t *value);
      void insert(uint8_t *key, uint8_t *value);
    private:
      int _lru_pos;
      uint32_t _n_bits_per_key, _n_bits_per_value;
  };
}
#endif
