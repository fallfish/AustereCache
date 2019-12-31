## Austere Cache
Austere Cache is a cache system enhanced by compression and deduplication technology.

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
make
cd ..
```
##### ISA-L Crypto
```
git clone https://github.com/intel/isa-l_crypto.git
cd isa-l_crypto
./autogen.sh
./configure --prefix=$(pwd)
make && make install
cd ..
```

#### Build the systems
##### Austere Cache
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ..
```
##### CacheDedup DLRU
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=1
make
cd ..
```
##### CacheDedup DARC
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DDARC=1
make
cd ..
```
##### CacheDedup CDARC
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCDARC=1
make
cd ..
```

### Trace Generation
#### Generate traces
```
```

### Run Demo
#### Expected Results
