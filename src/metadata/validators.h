#ifndef __VALIDATORS_H__
#define __VALIDATORS_H__

#include "common/config.h"
#include "utils/MurmurHash3.h"
namespace cache {
  //class Validator {
    //public:
      //static Validator *getInstance() {
        //static Validator *validator = nullptr;
        //if (validator == nullptr) {
          //validator = new Validator();
        //}
        //return validator;
      //}

      //bool validate_lba_collision(uint32_t bucket_id, uint32_t slot_id) {
        //return current_lba != lba_slots[bucket_id][slot_id];
      //}

      //bool validate_fp_collision(uint32_t bucket_id, uint32_t slot_id) {
        //std::cout << memcmp(fp_slots[bucket_id][slot_id], current_fp, Config::getInstance().getFingerprintLength()) << std::endl;
        //return memcmp(fp_slots[bucket_id][slot_id], current_fp, Config::getInstance().getFingerprintLength()) != 0;
      //}

      //void set_lba_slot(uint32_t bucket_id, uint32_t slot_id) {
        //lba_slots[bucket_id][slot_id] = current_lba;
      //}

      //void set_fp_slot(uint32_t bucket_id, uint32_t slot_id) {
        //memcpy(fp_slots[bucket_id][slot_id], current_fp, 20);
      //}

      //void set_current_lba(uint64_t lba) {
        //current_lba = lba;
      //}

      //void set_current_fp(uint8_t *fp) {
        //current_fp = fp;
      //}

      //void print_current_lba_hash(uint32_t bucket_id, uint32_t slot_id) {
      //}

      //void print_current_fp_hash(uint32_t bucket_id, uint32_t slot_id) {
        //uint32_t tmp;
        //MurmurHash3_x86_32(current_fp, Config::getInstance().getFingerprintLength(), 2, &tmp);
        //std::cout << std::hex << tmp << std::endl;
        //MurmurHash3_x86_32(fp_slots[bucket_id][slot_id], Config::getInstance().getFingerprintLength(), 2, &tmp);
        //std::cout << std::hex << tmp << std::endl;
      //}

      //uint64_t current_lba;
      //uint8_t  *current_fp;
      //uint64_t lba_slots[8192][32];
      //uint8_t  fp_slots[2048][32][20];
  //};

}
#endif
