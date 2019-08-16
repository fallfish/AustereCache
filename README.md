## SSDDup
SSDDup is a cache system enhanced by compression and deduplication technology.
Compression and deduplication together achieve storage savings on workload in the cache device.

### Characteristics
1. Hashing-based memory efficient indexing structure for deduplication.
2. Multi-threading and lightweight locking mechanism supported by the indexing.

### Build
#### Import third_party libraries
```
mkdir third_party && cd third_party
```
##### LZ4 compression algorithms
```
wget https://github.com/lz4/lz4/archive/v1.9.1.zip
unzip v1.9.1.zip
cd lz4-1.9.1
make -j4
cd ..
```
##### OpenSSL
```
git clone https://github.com/openssl/openssl.git
cd openssl
./config && make -j4
cd ../..
```

#### Build SSDDup
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
cd ..
```

### Test
#### Generate traces
```
cd build
cp ../scripts/generate_trace.sh ./
bash generate_trace.sh
```

#### Run Microbenchmarks
```
cd build
./microbenchmarks/run_ssddup --help
```

An example:
```
./microbenchmarks/run_ssddup --trace ../trace/compressibility_3/dup-1 --ca-bits 11 --multi-thread 0 --num-workers 1
```
Default primary device is `./primary_device`, cache device is `./cache_device`
For details, please refer to `src/benchmarks/run_ssddup.cc` and other micro benchmarks.
