#include <utility>

#include <cstring>
#include "bitmap.h"
#include "bucket.h"
#include "index.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_slot(n_bits_per_key + n_bits_per_value), _n_slots(n_slots),
    _n_total_bytes(((n_bits_per_key + n_bits_per_value) * n_slots + 7) / 8),
    _data(std::make_unique<Bitmap>((n_bits_per_key + n_bits_per_value) * n_slots))
  {} 

  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    Bucket(n_bits_per_key, n_bits_per_value, n_slots),
    _valid(std::make_unique<Bitmap>(n_slots))
  {
  }

  LBABucket::~LBABucket() {}

  uint32_t LBABucket::lookup(uint32_t lba_sig, uint32_t &ca_hash) {
    for (uint32_t slot_id = 0; slot_id < _n_slots; slot_id++) {
      uint32_t k = get_k(slot_id);
      if (k == lba_sig) {
        ca_hash = get_v(slot_id);
        return slot_id;
      }
    }
    return ~((uint32_t)0);
  }

  void LBABucket::advance(uint32_t slot_id)
  {
    uint32_t k = get_k(slot_id);
    uint32_t v = get_v(slot_id);
    for (uint32_t i = slot_id; i < _n_slots - 1; i++) {
      set_k(i, get_k(i + 1));
      set_v(i, get_v(i + 1));
      _valid->clear(i);
      if (_valid->get(i + 1))
        _valid->set(i);
    }
    set_k(_n_slots - 1, k);
    set_v(_n_slots - 1, v);
    _valid->set(_n_slots - 1);
  }

  uint32_t LBABucket::find_non_occupied_position(std::shared_ptr<CAIndex> ca_index)
  {
    uint32_t position = 0;
    bool valid = true;
    for (uint32_t i = 0; i < _n_slots; i++) {
      uint32_t v = get_v(i);
      if (!_valid->get(i)) continue;
      uint32_t size; uint64_t ssd_location; // useless variables
      // note here that v = 0 can result in too many evictions
      // needs ~15 lookup per update, further optimization needed
      // but correctness not impacted
      if (ca_index != nullptr)
        valid = ca_index->lookup(v, size, ssd_location);
      if (!valid) {
//        std::cout << "LBABucket::evict: " << v << std::endl;
//        if (v == 0) std::cout << "v = 0" << std::endl;
        set_k(i, 0), set_v(i, 0);
        _valid->clear(i);
        position = i;
      }
    }
    return position;
  }

  void LBABucket::update(uint32_t lba_sig, uint32_t ca_hash, std::shared_ptr<CAIndex> ca_index) {
    uint32_t v = 0;
    uint32_t index = lookup(lba_sig, v);
    if (index != ~((uint32_t)0)) {
      if (v != ca_hash) {
        set_v(index, ca_hash);
      }
    } else {
      index = find_non_occupied_position(std::move(ca_index));
      set_k(index, lba_sig);
      set_v(index, ca_hash);
      _valid->set(index);
    }
    advance(index);
    if (_valid->get(0)) {
      uint32_t v = 1;
    }
  }

  CABucket::CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    Bucket(n_bits_per_key, n_bits_per_value, n_slots)
  {
    _valid = std::make_unique< Bitmap >(1 * n_slots);
    _space = std::make_unique< Bucket >(0, 2, n_slots);
    _clock = std::make_unique< Bucket >(0, 2, n_slots);
    _clock_ptr = 0;
  }

  CABucket::~CABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  uint32_t CABucket::lookup(uint32_t ca_sig, uint32_t &size)
  {
    uint32_t index = 0, space = 0;
    while (index < _n_slots) {
      if (is_valid(index)) {
        if (get_k(index) == ca_sig) {
          size = get_space(index);
          return index;
        }
        index += space;
      }
      ++index;
    }
    return ~((uint32_t)0);
  }

  uint32_t CABucket::find_non_occupied_position(uint32_t size)
  {
    uint32_t index = 0, space = 0;
    // check whether there is a contiguous space
    while (index < _n_slots) {
      if (is_valid(index)) {
        space = 0;
        index += get_space(index) + 1;
      } else {
        ++space;
        ++index;
      }
      if (space == size + 1) break;
    }
    if (space < size + 1) {
      // No contiguous space, need to evict previous slot
      space = 0;
      index = _clock_ptr;
      while (1) {
        if (index == _n_slots) {
          space = 0;
          index = 0;
        }
        if (is_valid(index) && get_clock(index) != 0) {
          // the slot is valid and clock bits is not zero
          // cannot accommodate new item
          dec_clock(index);
          space = 0;
          index += get_space(index) + 1;
        } else {
          set_invalid(index);
          ++space;
          ++index;
        }
        if (space == size + 1) break;
      }
      _clock_ptr = index;
    }
    //std::cout << "CABucket::clock_ptr: " << _clock_ptr << std::endl;
    index -= space;
    return index;
  }

  uint32_t CABucket::update(uint32_t ca_sig, uint32_t size)
  {
    uint32_t size_ = 0, value = 0;
    uint32_t slot_id = lookup(ca_sig, size_);
    if (slot_id != ~((uint32_t)0)) {
      if (size_ == size) {
        inc_clock(slot_id);
        return slot_id;
      } else {
        set_invalid(slot_id);
      }
    }
    slot_id = find_non_occupied_position(size);
    //std::cout << "return slot_id in cabucket: " << slot_id << std::endl;
    set_k(slot_id, ca_sig);
    init_clock(slot_id);
    set_space(slot_id, size);
    set_valid(slot_id);

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
