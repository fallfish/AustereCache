cmake_minimum_required (VERSION 3.14)
project (SSDDup)
 
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories(include)
include_directories(src)

list(APPEND CacheLibSources
        src/metadata/bucket.cc
        src/metadata/index.cc
        src/metadata/metaverification.cc
        src/metadata/metajournal.cc
        src/metadata/CachePolicy.cc
        src/metadata/cacheDedup/cacheDedupCommon.cc

        src/chunk/chunkmodule.cc

        src/deduplication/deduplicationmodule.cc

        src/compression/compressionmodule.cc

        src/ssddup/ssddup.cc

        src/io/device/device.cc
        src/io/iomodule.cc

        src/manage/managemodule.cc
        src/manage/DirtyList.cc

        src/utils/MurmurHash3.cpp
        src/io/writebuffer.cc
        src/metadata/ReferenceCounter.cc
        src/metadata/ReferenceCounter.h
        src/metadata/cacheDedup/DLRULBAIndex.cc
        src/metadata/cacheDedup/DLRUFPIndex.cc
        src/metadata/cacheDedup/DARCLBAIndex.cc
        src/metadata/cacheDedup/DARCFPIndex.cc
        src/metadata/cacheDedup/BucketDLRULBAIndex.cc
        src/metadata/cacheDedup/BucketDLRUFPIndex.cc
        src/metadata/cacheDedup/BucketDLRUFPIndex.h
        src/metadata/cacheDedup/BucketDLRULBAIndex.h
        src/metadata/cachePolicies/LRU.cc
        src/metadata/cachePolicies/BucketAwareLRU.cc
        src/metadata/cachePolicies/LeastReferenceCount.cc
        src/metadata/cachePolicies/CAClock.cc 
        src/metadata/cachePolicies/ThresholdRCClock.cc
        src/metadata/cachePolicies/CachePolicy.cc
        )

if (DLRU)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDLRU)
  list(APPEND CacheLibSources src/metadata/MetadataModuleDLRU.cc src/ssddup/ssddupNoCompression.cc src/manage/DirtyListCacheDedup.cc)
elseif (DARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDARC)
  list(APPEND CacheLibSources src/metadata/MetadataModuleDARC.cc src/ssddup/ssddupNoCompression.cc src/manage/DirtyListCacheDedup.cc)
elseif (CDARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DCDARC)
  list(APPEND CacheLibSources src/metadata/MetadataModuleCDARC.cc src/ssddup/ssddupCompression.cc src/metadata/cacheDedup/CDARCFPIndex.cc src/manage/DirtyListCDARC.cc)
elseif (BUCKETDLRU)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DBUCKETDLRU)
  list(APPEND CacheLibSources src/metadata/MetadataModuleBucketDLRU.cc src/ssddup/ssddupNoCompression.cc src/manage/DirtyListCacheDedup.cc)
else ()
  add_definitions(-DACDC)
  list(APPEND CacheLibSources src/metadata/MetadataModule.cc src/ssddup/ssddupCompression.cc src/manage/DirtyListACDC.cc)
endif()

add_library(cache ${CacheLibSources})

include_directories(${CMAKE_SOURCE_DIR}/third_party/openssl/include)
set (OPENSSL_ROOT_DIR ${CMAKE_SOURCE_DIR}/third_party/openssl)
set (OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/third_party/lz4-1.9.1/lib)
link_directories(${CMAKE_SOURCE_DIR}/third_party/lz4-1.9.1/lib)
target_link_libraries(cache OpenSSL::SSL lz4 pthread)

################################
# Micro Benchmarks
################################
add_executable(run_fiu src/benchmark/run_fiu.cc src/utils/cJSON.c)
set_target_properties(run_fiu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(run_fiu cache)

###############################################
## Buse Userspace Virtual Kernel Block Device
###############################################
# link_directories(${CMAKE_SOURCE_DIR}/third_party/BUSE)
# include_directories(third_party/BUSE)
# add_executable(buse_ssddup third_party/BUSE/ssddup.cc)
# target_link_libraries(buse_ssddup cache buse)

#################################
## GTest
#################################
#ADD_SUBDIRECTORY(third_party/gtest-1.9.0)
##enable_testing()
#include_directories(third_party/gtest-1.9.0/googletest/include)

#################################
## Unit Tests
#################################
## Add test cpp file
#add_executable( runUnitTests src/test/main.cpp)
## Link test executable against gtest & gtest_main
#target_link_libraries(runUnitTests gtest gtest_main cache)
##add_test( runUnitTests runUnitTests )

#################################
## GLog
#################################
#ADD_SUBDIRECTORY(third_party/glog-0.4.0)
#include_directories(third_party/gtest-1.9.0/googletest/include)
