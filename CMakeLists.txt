cmake_minimum_required (VERSION 2.6)
project (SSDDup)
 
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories(include)
include_directories(src)

add_library(cache
  src/metadata/bucket.cc
  src/metadata/index.cc
  src/metadata/metaverification.cc
  src/metadata/metajournal.cc
  src/metadata/metadatamodule.cc

  src/chunk/chunkmodule.cc

  src/deduplication/deduplicationmodule.cc

  src/compression/compressionmodule.cc

  src/ssddup/ssddup.cc

  src/io/device/device.cc
  src/io/iomodule.cc

  src/manage/managemodule.cc

  src/utils/MurmurHash3.cpp
  )

set (OPENSSL_ROOT_DIR ${CMAKE_SOURCE_DIR}/third_party/openssl)
set (OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/third_party/openssl/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/lz4-1.9.1/lib)
link_directories(${CMAKE_SOURCE_DIR}/third_party/lz4-1.9.1/lib)
target_link_libraries(cache OpenSSL::SSL)
target_link_libraries(cache lz4)

################################
# Micro Benchmarks
################################
add_executable(compression_benchmark src/benchmark/compression.cc)
set_target_properties(compression_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(compression_benchmark cache)

add_executable(trace_gen src/benchmark/trace_gen.cc)
set_target_properties(trace_gen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/microbenchmarks")
target_link_libraries(trace_gen cache)

################################
# GTest
################################
ADD_SUBDIRECTORY(third_party/gtest-1.9.0)
#enable_testing()
include_directories(third_party/gtest-1.9.0/googletest/include)

################################
# Unit Tests
################################
# Add test cpp file
add_executable( runUnitTests src/test/main.cpp)
# Link test executable against gtest & gtest_main
target_link_libraries(runUnitTests gtest gtest_main cache)
#add_test( runUnitTests runUnitTests )

################################
# GLog
################################
ADD_SUBDIRECTORY(third_party/glog-0.4.0)
include_directories(third_party/gtest-1.9.0/googletest/include)
