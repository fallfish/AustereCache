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
        src/metadata/meta_verification.cc
        src/metadata/meta_journal.cc
        src/metadata/cachededup/common.cc

        src/chunking/chunk_module.cc

        src/deduplication/deduplication_module.cc

        src/compression/compression_module.cc

        src/austere_cache/austere_cache.cc

        src/io/device/device.cc
        src/io/io_module.cc
        src/io/writebuffer.cc

        src/manage/manage_module.cc
        src/manage/dirtylist.cc

        src/utils/MurmurHash3.cpp
        src/metadata/reference_counter.cc
        src/metadata/cachededup/dlru_lbaindex.cc
        src/metadata/cachededup/dlru_fpindex.cc
        src/metadata/cachededup/darc_lbaindex.cc
        src/metadata/cachededup/darc_fpindex.cc
        src/metadata/cachededup/bucketdlru_lbaindex.cc
        src/metadata/cachededup/bucketdlru_fpindex.cc
        src/metadata/cache_policies/lru.cc
        src/metadata/cache_policies/bucket_aware_lru.cc
        src/metadata/cache_policies/least_reference_count.cc
        src/metadata/cache_policies/cache_policy.cc
        )

if (DLRU)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDLRU)
  list(APPEND CacheLibSources src/metadata/metadata_module_dlru.cc src/austere_cache/austere_cache_no_compression.cc src/manage/dirtylist_cachededup.cc)
elseif (DARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DDARC)
  list(APPEND CacheLibSources src/metadata/metadata_module_darc.cc src/austere_cache/austere_cache_no_compression.cc src/manage/dirtylist_cachededup.cc)
elseif (CDARC)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DCDARC)
  list(APPEND CacheLibSources src/metadata/metadata_module_cdarc.cc src/austere_cache/austere_cache_compression.cc src/metadata/cachededup/cdarc_fpindex.cc src/manage/dirtylist_cdarc.cc)
elseif (BUCKETDLRU)
  add_definitions(-DCACHE_DEDUP)
  add_definitions(-DBUCKETDLRU)
  list(APPEND CacheLibSources src/metadata/metadata_module_dlru.cc src/austere_cache/austere_cache_no_compression.cc src/manage/dirtylist_cachededup.cc)
else ()
  add_definitions(-DACDC)
  list(APPEND CacheLibSources src/metadata/metadata_module.cc src/austere_cache/austere_cache_compression.cc src/manage/dirtylist_austerecache.cc)
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
