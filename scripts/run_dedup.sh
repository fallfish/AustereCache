#!/bin/bash
ca_bits=$1

for workload in "dup-all" "dup-1024" "dup-512" "dup-256" "dup-128" "dup-64" "dup-32" "dup-16" "dup-1"
do
  for comp in {0..4}
  do
    echo "compressibility ${comp} ${workload}"
    ./microbenchmarks/run_dedup --trace ../trace/compressibility_${comp}/${workload} --ca-bits $ca_bits
    echo ""
  done
done
