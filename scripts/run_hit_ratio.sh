#!/bin/bash

multithread=0
num_workers=1
chunk_size=32768
num_unique_chunks=16384
num_chunks=16384
num_requests=16384
content_distribution="zipf"
#for version in "ACDC"
for version in "ACDC" "DLRU" "DARC" "CDARC"
do
  if [[ ${version} = "ACDC" ]]
  then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=0 -DDARC=0 -DCDARC=0
    write_buffer_size=0
  elif [[ ${version} = "DLRU" ]]
  then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=1 -DDARC=0 -DCDARC=0
    write_buffer_size=0
  elif [[ ${version} = "DARC" ]]
  then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=0 -DDARC=1 -DCDARC=0
    write_buffer_size=0
  else
    cmake .. -DCMAKE_BUILD_TYPE=Release -DDLRU=0 -DDARC=0 -DCDARC=1
    write_buffer_size=$((1024 * 1024 * 2))
  fi
  make clean
  make -j

  mkdir -p results/${version}
  comp=3
  direct_io=0

  access_pattern_distribution="zipf"
  for content_skewness in 0.5 1.0
  do
    for access_pattern_skewness in 0.5 1.0
    do
      for ca_bits in 1 2 3 4 5 6 7 8 9 10 11
      do
        for comp in 3
        do
          sync;
          echo 1 | sudo tee /proc/sys/vm/drop_caches
          echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup \
            --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
            --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
            --access-pattern ../trace/access_pattern/${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}" \
            | tee results/${version}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
          stdbuf -oL sudo ./microbenchmarks/run_ssddup \
            --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
            --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
            --access-pattern ../trace/access_pattern/${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests} \
            | tee -a results/${version}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
        done
      done
    done
  done

  access_pattern_distribution="uniform"
  for content_skewness in 0.5 1.0
  do
    for ca_bits in 1 2 3 4 5 6 7 8 9 10 11
    do
      for comp in 3
      do
        sync;
        echo 1 | sudo tee /proc/sys/vm/drop_caches
        echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup \
          --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
          --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
          --access-pattern ../trace/access_pattern/${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests}" \
          | tee results/${version}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
        stdbuf -oL sudo ./microbenchmarks/run_ssddup \
          --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
          --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
          --access-pattern ../trace/access_pattern/${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests} \
          | tee -a results/${version}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
      done
    done
  done
done
