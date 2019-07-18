#include <utility>

#include <cstring>
#include <cassert>
#include "bitmap.h"
#include "bucket.h"
#include "index.h"
#include "cache_policy.h"

namespace cache {


  Bucket::Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots,
      uint8_t *data, uint8_t *valid, CachePolicy *cache_policy, uint32_t bucket_id) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_slot(n_bits_per_key + n_bits_per_value), _n_slots(n_slots),
    _data(data), _valid(valid), _bucket_id(bucket_id)
  {
    if (cache_policy != nullptr)
      _cache_policy = cache_policy->get_executor(this);
  }

  Bucket::~Bucket()
  {
    if (_cache_policy != nullptr)
      delete _cache_policy;
  }

  uint32_t LBABucket::lookup(uint32_t lba_sig, uint32_t &ca_hash) {
    for (uint32_t slot_id = 0; slot_id < _n_slots; slot_id++) {
      if (!is_valid(slot_id)) continue;
      uint32_t lba_sig_ = get_k(slot_id);
      if (lba_sig_ == lba_sig) {
        ca_hash = get_v(slot_id);
        return slot_id;
      }
    }
    return ~((uint32_t)0);
  }

  void LBABucket::promote(uint32_t lba_sig) {
    uint32_t ca_hash_ = 0;
    uint32_t slot_id = lookup(lba_sig, ca_hash_);
    assert(slot_id != ~((uint32_t)0));
    _cache_policy->promote(slot_id);
  }

  void LBABucket::update(uint32_t lba_sig, uint32_t ca_hash, std::shared_ptr<CAIndex> ca_index) {
    uint32_t ca_hash_ = 0;
    uint32_t slot_id = lookup(lba_sig, ca_hash_);
    if (slot_id != ~((uint32_t)0)) {
      set_v(slot_id, ca_hash);
      set_invalid(slot_id);
    }

    // If is full, clear all obsoletes first
    // Warning: Number of slots is 32, so a uint32_t comparison is efficient.
    //          Any other design with larger number of slots should change this
    //          one.
    if (get_valid_32bits(0) == ~(uint32_t)0) {
      _cache_policy->clear_obsoletes(std::move(ca_index));
    }
    slot_id = _cache_policy->allocate();
    set_k(slot_id, lba_sig);
    set_v(slot_id, ca_hash);
    set_valid(slot_id);
    _cache_policy->promote(slot_id);
  }


  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  uint32_t CABucket::lookup(uint32_t ca_sig, uint32_t &n_slots_occupied)
  {
    uint32_t slot_id = 0;
    n_slots_occupied = 0;
    for ( ; slot_id < _n_slots; ) {
      if (!is_valid(slot_id)
          || ca_sig != get_k(slot_id)) {
        ++slot_id;
        continue;
      }
      while (slot_id < _n_slots
          && is_valid(slot_id)
          && ca_sig == get_k(slot_id)) {
        ++n_slots_occupied; 
        ++slot_id;
      }
      return slot_id - n_slots_occupied;
    }
    return ~((uint32_t)0);
  }


  void CABucket::promote(uint32_t ca_sig)
  {
    uint32_t slot_id = 0, compressibility_level = 0;
    slot_id = lookup(ca_sig, compressibility_level);
    assert(slot_id != ~((uint32_t)0));

    _cache_policy->promote(slot_id, compressibility_level + 1);
  }

  uint32_t CABucket::update(uint32_t ca_sig, uint32_t n_slots_to_occupy)
  {
    uint32_t n_slots_occupied = 0, value = 0;
    uint32_t slot_id = lookup(ca_sig, n_slots_occupied);
    if (slot_id != ~((uint32_t)0)) {
      for (uint32_t slot_id_ = slot_id;
          slot_id_ < slot_id + n_slots_occupied;
          ++slot_id_) {
        set_invalid(slot_id_);
      }
    }

    slot_id = _cache_policy->allocate(n_slots_to_occupy);
    for (uint32_t slot_id_ = slot_id;
        slot_id_ < slot_id + n_slots_to_occupy;
        ++slot_id_) {
      set_k(slot_id_, ca_sig);
      set_valid(slot_id_);
    }

    return slot_id;
  }

  void CABucket::erase(uint32_t ca_sig)
  {
    for (uint32_t index = 0; index < _n_slots; index++) {
      if (get_k(index) == ca_sig) {
        set_invalid(index);
      }
    }
  }

  Buckets::Buckets(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots, uint32_t n_buckets) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_slot(n_bits_per_key + n_bits_per_value), _n_slots(n_slots),
    _n_data_bytes_per_bucket(((n_bits_per_key + n_bits_per_value) * n_slots + 7) / 8),
    _n_valid_bytes_per_bucket((1 * n_slots + 7) / 8),
    _data(std::make_unique<uint8_t[]>(_n_data_bytes_per_bucket * n_buckets)),
    _valid(std::make_unique<uint8_t[]>(_n_valid_bytes_per_bucket * n_buckets)),
    _mutexes(std::make_unique<std::mutex[]>(n_buckets))
  {} 

  Buckets::~Buckets() {}
  void Buckets::set_cache_policy(std::unique_ptr<CachePolicy> cache_policy)
  { 
    _cache_policy = std::move(cache_policy);
  }

  LBABuckets::LBABuckets(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots, uint32_t n_buckets) :
    Buckets(n_bits_per_key, n_bits_per_value, n_slots, n_buckets)
  {}

  LBABuckets::~LBABuckets() {}

  CABuckets::CABuckets(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots, uint32_t n_buckets) :
    Buckets(n_bits_per_key, n_bits_per_value, n_slots, n_buckets)
  {
  }

  CABuckets::~CABuckets() {}

  //BlockBucket::BlockBucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    //Bucket(0, 2, n_slots)
  //{
    //// construct the indexing structure
    //// Use cuckoo hash to gain high throughput with small additional memory footprint
    //_index = std::make_unique<BitmapCuckooHashTable>(n_bits_per_key, n_bits_per_value, int(n_slots * 1.1));
    //_reference_counts = std::make_unique<uint16_t []>((n_slots + 63) / 64); 
    //_current_block_id = 0;
    //_valid = std::make_unique<Bitmap>(0, 1, n_slots);
  //}

  //uint32_t BlockBucket::lookup(uint32_t ca_sig, uint32_t &size, Chunk &c)
  //{
    //// search in the anxiliary lists with full CA
    //uint32_t slot_id;
    //if (_anxiliary_list.find(c._ca) != _anxiliary_list.end()) {
      //slot_id = _anxiliary_list.find(c._ca)->second;
    //} else {
      //slot_id = _index->lookup(ca_sig);
    //}
    //size = get_v(slot_id);
    //return slot_id;
  //}

  /**
   * @brief Promoting a ca signature means updating its status
   *        Requirement: the ca signature must be hit
   * @param ca_sig
   */
  //void BlockBucket::promote(uint32_t ca_sig)
  //{

  //}

  //// Must be NOT_DUP and allocate new slot
  //// Update the list
  //void BlockBucket::update(uint32_t ca_sig, uint32_t size, Chunk &c)
  //{
    //// First, check whether current block is full
    //uint32_t slot_id = _current_block_id * _n_slots_per_block,
             //end = (_current_block_id + 1) * _n_slots_per_block;
    //for ( ; slot_id < end; ) {
      //if (!_valid->get(slot_id)) break;
      //slot_id += get_v(slot_id);
    //}
    //if (end - slot_id < size) {
      //// Current block is full, we need to find a new block that is empty
      //uint32_t block_id = 0;
      //uint32_t n_blocks = _n_slots / _n_slots_per_block;
      //uint32_t n_32slots_per_block = _n_slots_per_block / 32; // assume the _n_slots_per_block align with 32;

      //for ( ; block_id < n_blocks; ++block_id) {
        //bool is_empty = true;
        //for (uint32_t i = block_id * n_32slots_per_block;
            //i < n_32slots_per_block; ++i) {
          //is_empty &= (_valid->get_32bits(i) == 0);
        //}
        //if (is_empty) break;
      //}

      //// no empty block, evict an old block
      //if (block_id == n_blocks) {
        //uint32_t candidate_block_id, candidate_block_score = ~0;
        //for (block_id = 0; block_id < n_blocks; ++block_id) {
          //uint32_t n_chunks = 0, score = 0;
          //for (uint32_t i = block_id * n_32slots_per_block;
              //i < n_32slots_per_block; ++i) {
            //n_chunks += __builtin_popcount(_valid->get_32bits(i));
          //}
          //score = n_chunks * _reference_counts[block_id];
          //if (score < candidate_block_score) {
            //candidate_block_id = block_id;
            //candidate_block_score = score;
          //}
        //}

        //block_id = candidate_block_id;
        //// clear valid bits
        //for (uint32_t i = block_id * n_32slots_per_block;
            //i < n_32slots_per_block; ++i) {
          //_valid->set_32bits(i, 0);
        //}

        //// later we will fetch the CAs out and clear those CAs
        //// in a background thread
      //}

      //if (block_id != current_block_id) {
        //current_block_id = block_id;
      //}
      //slot_id = block_id * _n_slots_per_block;
    //}

    //// find the slot
    //set_v(slot_id, size);
    //_valid->set(slot_id, 1);

    //// Note: if current ca_sig already has a slot allocated
    ////       and we have known it didnot match
    ////       It must be a collision.
    //if (_index->find(ca_sig) != ~0) {
      //_anxiliary_list[c._ca] = slot_no;
    //} 
  //}
}
