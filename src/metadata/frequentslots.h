#ifndef __FREQUENT_SLOTS_H__
#define __FREQUENT_SLOTS_H__

#include "common/common.h"

namespace cache {

class FrequentSlots {
 public:
  FrequentSlots() {
    memset(_lbas, 0, sizeof(_lbas));
    memset(_meta, 0, sizeof(_meta));
  }

  void allocate_slot(uint32_t ca_hash)
  {
    int i;
    for (i = 0; i < 32; i++) {
      if (_meta[i]._ca_hash == ca_hash) {
        if (_lbas[i] == nullptr)
          _lbas[i] = new uint64_t[1024];
        break;
      }
    }
    if (i == 32) {
      _meta[31]._ca_hash = ca_hash;
      if (_lbas[31] == nullptr) {
        _lbas[31] = new uint64_t[1024];
      }
    }
    advance(i);
  }

  void lookup() {

  }
 private:
  void advance(int index) {
    uint32_t  tmp_ca_hash  = _meta[index]._ca_hash,
              tmp_n_lbas   = _meta[index]._n_lbas,
    uint64_t *tmp_lba_ptr  = _lbas[index];
    for (int i = index; i < 31; i++) {
      _meta[index]._ca_hash = _meta[index + 1]._ca_hash;
      _meta[index]._n_lbas  = _meta[index + 1]._n_lbas;
      _lbas[index]          = _lbas[index + 1];
    }
    _meta[31]._ca_hash = tmp_ca_hash;
    _meta[31]._n_lbas  = tmp_n_lbas;
    _lbas[31]          = tmp_lba_ptr;
  }

  struct {
    uint32_t _ca_hash;
    uint32_t _n_lbas;
  } _meta[32];
  uint64_t* _lbas[32];
}

}


#endif

