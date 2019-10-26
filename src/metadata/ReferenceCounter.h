//
// Created by 王秋平 on 10/26/19.
//

#ifndef SSDDUP_REFERENCECOUNTER_H
#define SSDDUP_REFERENCECOUNTER_H


#include <map>

class ReferenceCounter {

    std::map<uint64_t, uint32_t> counters_;

public:
    bool clear();
    bool query(uint64_t key);
    bool reference(uint64_t key);
    bool dereference(uint64_t key);

    static ReferenceCounter& getInstance() {
      static ReferenceCounter instance;
      return instance;
    }
};


#endif //SSDDUP_REFERENCECOUNTER_H
