#!/bin/bash
mkdir -p ../trace/access_pattern

num_chunks=16384
num_requests=16384
for skewness in 0.5 0.6 0.7 0.8 0.9 1.0
do
  for comp in 3
  do
    ./microbenchmarks/access_pattern_gen --num-chunks ${num_chunks} --num-requests ${num_requests} \
      --distribution zipf --skewness ${skewness} \
      --output-file ../trace/access_pattern/zipf_${skewness}-num_chunks_${num_chunks}-num_requests_${num_requests} &
  done
done
wait
for comp in 3
do
  ./microbenchmarks/access_pattern_gen --num-chunks ${num_chunks} --num-requests ${num_requests} \
    --distribution uniform \
    --output-file ../trace/access_pattern/uniform-num_chunks_${num_chunks}-num_requests_${num_requests} &
done
wait
