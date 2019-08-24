[TOC]

## Libraries

+ LZ4 compression algorithms ("lz4.h")
+ OpenSSL (<openssl/md5.h>, <openssl/sha1.h>)
+ MurmurHash3



## SSDDup Architecture

(Sorted by alphabetical order)

+ Slot size: 8KiB + 512B
+ Bucket size: 32 \* (8KiB + 512B) = 272KiB
+ If CA Bits are 11, then the minimum cache (SSD) size should be (2^11) * 272KiB = 544MiB

### src/benchmark

Deals with tests & benchmarks for different modules.



### src/chunk - Chunk Module

#### `class Chunker`

A chunk iterator. It can deal with unaligned addresses. It mainly contains `next()` method to find the next chunk in a contiguous address range.

#### `class ChunkModule`

A factory that create chunks.

```c++
ChunkModule::ChunkModule() {}
Chunker ChunkModule::create_chunker(uint64_t addr, void *buf, uint32_t len)
{
	Chunker chunker(addr, buf, len);
	return chunker;
}
```



### src/compression - Compression Module

#### `class CompressionModule`

`compress()` compresses chunks and calculate the compression level.

+ Invokes `LZ4_compress_default()` to perform the compression algorithm.

`decompress()` decompresses chunks.

+ Invokes `LZ4_decompress_safe()` to perform the decompression algorithm.



### src/deduplication - Deduplication Module

Actually all the work is done by the **Metadata Module**.



### src/io - IO Module

`class BlockDevice`: Class for block devices (SSD or HDD). open/read/write operations.

`class Device`: Provide virtual functions for `BlockDevice`.



### src/manage - Manage Module

Fetch R/W address and R/W data from/to the **IOModule**.



### src/ssddup - Overall

`process_mem_usage()` reads process statistics from `/proc/self/stat`. 

Public methods `read()` and `write()` response requests by calling `internal_read()` and `internal_write()`.

#### `internal_read()`

The function can be divided into several steps:

+ Look up the CA index
+ Decompress data
+ Update metadata (if needed)



### src/metadata

#### cachededup_index.{cc|h}

Implement the D-LRU and D-ARC algorithms.

(Common) Functions:

+ `lookup()`: looks up an address in the cache
+ `promote()`: (Assumption: HIT) push an address to the front (the most recent accessed object)
+ `update()`: (Assumption: HIT/MISS) Add an address to the cache

Classes: 

+ `DLRU_SourceIndex`
+ `DLRU_FingerprintIndex`: A dedup-only design (?)
+ `DARC_SourceIndex`
+ `DLRU_FingerprintIndex`
+ `CDARC_FingerprintIndex`

#### metadatamodule.{cc|h} - MetaData Module

Members:

+ LBAIndex
+ CAIndex (Fingerprint index)

Functions:

+ `dedup()`
  + By using different `#define`'s, MetadataModule selects different algorithms to deal with the index.
+ `lookup()`
+ `update()`

#### index.{cc|h} - LBAIndex/CAIndex

Classes: 

+ `Index`
+ `CAIndex : Index`
+ `LBAIndex : Index`
