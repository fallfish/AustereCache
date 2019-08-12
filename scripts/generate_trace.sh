#!/bin/bash
rm -r ../trace/compressibility_0
rm -r ../trace/compressibility_1
rm -r ../trace/compressibility_2
rm -r ../trace/compressibility_3
rm -r ../trace/compressibility_4
mkdir -p ../trace/compressibility_0
mkdir -p ../trace/compressibility_1
mkdir -p ../trace/compressibility_2
mkdir -p ../trace/compressibility_3
mkdir -p ../trace/compressibility_4

#for workload in 16384 1024 512 256 128 64 32 16 8 4 2 1
#do
  #for comp in {0..4}
  #do
    #./microbenchmarks/trace_gen --working-set-size 128M --compressibility ${comp} --output-file ../trace/compressibility_${comp}/dup-${workload} --duplication-factor ${workload} --distribution uniform &
  #done
  #wait
#done

#for workload in 0.5 1.0
#do
  #for comp in 3
  #do
    #./microbenchmarks/trace_gen --working-set-size 128M --compressibility ${comp} --output-file ../trace/compressibility_${comp}/zipf-skewness-${workload} --duplication-factor 1 --distribution zipf --skewness ${workload} &
  #done
  #wait
#done

chunk_size=32768
num_unique_chunks=16384
num_chunks=16384
for skewness in 0.5 0.6 0.7 0.8 0.9 1.0
do
  for comp in 3
  do
    ./microbenchmarks/trace_gen --chunk-size ${chunk_size} --num-unique-chunks ${num_unique_chunks} --num-chunks ${num_chunks} --compressibility ${comp} --distribution zipf --skewness ${skewness} --output-file ../trace/compressibility_${comp}/zipf_${skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} &
    #./microbenchmarks/trace_gen --working-set-size 128M --compressibility ${comp} --output-file ../trace/compressibility_${comp}/zipf-skewness-${workload} --duplication-factor 1 --distribution zipf --skewness ${workload} &
  done
done
wait
