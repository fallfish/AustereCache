#!/bin/bash
echo "compressibility 0 all random"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/no-dup
echo ""
echo "compressibility 1 all random"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/no-dup
echo ""
echo "compressibility 2 all random"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/no-dup
echo ""
echo "compressibility 3 all random"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/no-dup
echo ""
echo "compressibility 4 all random"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/no-dup
echo ""

echo "compressibility 0 unique"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-all
echo ""
echo "compressibility 1 unique"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-all
echo ""
echo "compressibility 2 unique"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-all
echo ""
echo "compressibility 3 unique"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-all
echo ""
echo "compressibility 4 unique"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-all
echo ""

echo "compressibility 0 dup 1024"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-1024
echo ""
echo "compressibility 1 dup 1024"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-1024
echo ""
echo "compressibility 2 dup 1024"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-1024
echo ""
echo "compressibility 3 dup 1024"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-1024
echo ""
echo "compressibility 4 dup 1024"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-1024
echo ""

echo "compressibility 0 dup 512"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-512
echo ""
echo "compressibility 1 dup 512"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-512
echo ""
echo "compressibility 2 dup 512"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-512
echo ""
echo "compressibility 3 dup 512"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-512
echo ""
echo "compressibility 4 dup 512"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-512
echo ""

echo "compressibility 0 dup 256"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-256
echo ""
echo "compressibility 1 dup 256"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-256
echo ""
echo "compressibility 2 dup 256"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-256
echo ""
echo "compressibility 3 dup 256"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-256
echo ""
echo "compressibility 4 dup 256"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-256
echo ""

echo "compressibility 0 dup 64"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-64
echo ""
echo "compressibility 1 dup 64"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-64
echo ""
echo "compressibility 2 dup 64"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-64
echo ""
echo "compressibility 3 dup 64"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-64
echo ""
echo "compressibility 4 dup 64"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-64
echo ""

echo "compressibility 0 dup 32"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-32
echo ""
echo "compressibility 1 dup 32"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-32
echo ""
echo "compressibility 2 dup 32"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-32
echo ""
echo "compressibility 3 dup 32"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-32
echo ""
echo "compressibility 4 dup 32"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-32
echo ""

echo "compressibility 0 dup 16"
./microbenchmarks/run_compression --trace ../trace/compressibility_0/dup-16
echo ""
echo "compressibility 1 dup 16"
./microbenchmarks/run_compression --trace ../trace/compressibility_1/dup-16
echo ""
echo "compressibility 2 dup 16"
./microbenchmarks/run_compression --trace ../trace/compressibility_2/dup-16
echo ""
echo "compressibility 3 dup 16"
./microbenchmarks/run_compression --trace ../trace/compressibility_3/dup-16
echo ""
echo "compressibility 4 dup 16"
./microbenchmarks/run_compression --trace ../trace/compressibility_4/dup-16
echo ""
