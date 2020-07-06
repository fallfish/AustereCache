## AustereCache
AustereCache is a memory-efficient SSD cache system enhanced by compression and deduplication. You can find more details
in our publication.

### Publication
* Qiuping Wang, Jinhong Li, Wen Xia, Erik Kruus, Biplob Debnath, and Patrick P. C. Lee.  
[Austere Flash Caching with Deduplication and Compression.](https://www.cse.cuhk.edu.hk/~pclee/www/pubs/atc20.pdf)  
USENIX ATC 2020.



### Build
#### Testbed Environment
1. Third party libraries: LZ4, ISA-L_crypto
2. OS: Ubuntu 16.04 with Linux kernel 4.4.0-170-generic
3. Compiler tools: cmake 3.15.2, gcc 5.4.0

#### Build third_party libraries
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
##### CacheDedup D-LRU
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=1
make
cd ..
```
##### CacheDedup D-ARC
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DDARC=1
make
cd ..
```
##### CacheDedup CD-ARC
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCDARC=1
make
cd ..
```

### Trace Generation
#### Process FIU traces

1. Download the FIU traces into the directory `traces/`. You can get the traces from http://iotta.snia.org/traces/390, namely **FIU IODedup**.
2. Go into the directory `traces/` and start processing.

```
cd traces
./process_traces.sh     # May take long time
cd ..
```

3. You can check the basic statistics of the generated traces using `calculate_basics.sh`.

```
cd traces
./calculate_basics.sh   # May take long time
cd ..
```

#### Generate synthetic traces

```
cd traces
./syn_trace_gen.sh
cd ..
```

### Run Demo
We prepare a generated synthetic trace sample with I/O deduplication ratio 50%, w/r ratio 7:3 (traces/sample.txt), along with
a configuration file (conf/sample.json).
After building the project, you can run the sample trace with following commands, or generate traces according to
the trace generation procedure.
For AC-D, D-LRU, and D-ARC
```
./run ../conf/sample_nocompression.json
```
For AC-DC and CD-ARC
```
./run ../conf/sample_compression.json
```
You could find the sample output of the demo program under ./conf/example_output.txt for AC-DC.
