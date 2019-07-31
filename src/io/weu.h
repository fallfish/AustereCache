#ifndef __WEU_H__
#define __WEU_H__


uint64_t WriteEvictUnit::gen_id = 0;
struct WEULocation {
  uint64_t _weu_id;
  uint32_t _offset;
  uint32_t _len;
};

class WriteEvictUnit {
  static uint64_t global_gen_id;
  uint32_t _offset = 0;
  uint32_t _size = 0;
  struct {
    uint64_t _gen_id;
  } _header;
  uint8_t buffer[0];

  inline bool is_full(uint32_t len) {
    return (_offset + len) > _size;
  }
  void push_back(uint64_t addr, uint8_t *buf, uint32_t len)
  {
    assert(!is_full(len));
    memcpy(_buffer + _offset, buf, len);
    _offset += len;
  }
};

#endif
