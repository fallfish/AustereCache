cmake_minimum_required (VERSION 2.6)
project (SSDDup)
 
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories(include)
include_directories(src)

if (DLRU)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDLRU)
endif()
if (DARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDARC)
endif()
if (CDARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DCDARC)
endif()

add_library(cache
  src/metadata/bucket.cc
  src/metadata/index.cc
  src/metadata/metaverification.cc
  src/metadata/metajournal.cc
  src/metadata/metadatamodule.cc
  src/metadata/cache_policy.cc
  src/metadata/cachededup_index.cc

  src/chunk/chunkmodule.cc

  src/deduplication/deduplicationmodule.cc

  src/compression/compressionmodule.cc

  src/ssddup/ssddup.cc

  src/io/device/device.cc
  src/io/iomodule.cc

  src/manage/managemodule.cc

  src/utils/MurmurHash3.cpp
  )

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
add_executable(trace_gen src/benchmark/trace_gen.cc)
set_target_properties(trace_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(trace_gen cache)

add_executable(access_pattern_gen src/benchmark/access_pattern_gen.cc)
set_target_properties(access_pattern_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(access_pattern_gen cache)

add_executable(run_ssddup src/benchmark/run_ssddup.cc)
set_target_properties(run_ssddup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(run_ssddup cache)

add_executable(run_dedup src/benchmark/run_dedup.cc)
set_target_properties(run_dedup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(run_dedup cache pthread)

add_executable(run_compression src/benchmark/run_compression.cc)
set_target_properties(run_compression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(run_compression cache)

add_executable(run_chunk src/benchmark/run_chunk.cc)
set_target_properties(run_chunk PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(run_chunk cache)

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
