cmake_minimum_required (VERSION 2.6)
project (SSDDup)
 
set (CMAKE_CXX_STANDARD 14)

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

  src/ssddup/ssddup.cc

  src/io/device/device.cc
  src/io/iomodule.cc

  src/manage/managemodule.cc

  src/utils/MurmurHash3.cpp
  )
set (OPENSSL_ROOT_DIR ${CMAKE_SOURCE_DIR}/third_party/openssl)
set (OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/openssl/include)
target_link_libraries(cache OpenSSL::SSL)

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
