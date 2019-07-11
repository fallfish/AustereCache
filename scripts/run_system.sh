#!/bin/bash

for multithread in 0
do
  for num_workers in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
  do
    for ca_bits in 1 2 3 4 6 8 10
    do
      for workload in  "dup-16384" "dup-1024" "dup-512" "dup-256" "dup-128" "dup-64" "dup-32" "dup-16" "dup-1"
      do
        for comp in 3
        do
          #echo "${comp} ${workload} ${multithread} ${num_workers} ${ca_bits}" | tee results/use_memory/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
          echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup --trace ../trace/compressibility_${comp}/${workload} --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers}"
          #echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup --trace ../trace/compressibility_${comp}/${workload} --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers}" | tee -a results/use_memory/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
          #stdbuf -oL sudo ./microbenchmarks/run_ssddup --trace ../trace/compressibility_${comp}/${workload} --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} | tee -a results/use_memory/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
          #echo ""
        done
      done
    done
  done
done
