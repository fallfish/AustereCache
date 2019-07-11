#!/bin/bash
rm ../trace/compressibility_0/*
rm ../trace/compressibility_1/*
rm ../trace/compressibility_2/*
rm ../trace/compressibility_3/*
rm ../trace/compressibility_4/*

for workload in 16384 1024 512 256 128 64 32 16 8 4 2 1
do
  for comp in {0..4}
  do
    ./microbenchmarks/trace_gen --working-set-size 512M --compressibility ${comp} --output-file ../trace/compressibility_${comp}/dup-${workload} --duplication-factor ${workload} &
  done
  wait
done
