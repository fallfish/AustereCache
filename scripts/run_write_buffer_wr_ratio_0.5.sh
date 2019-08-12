#!/bin/bash

multithread=0
num_workers=1
chunk_size=32768
num_unique_chunks=16384
num_chunks=16384
num_requests=16384
content_distribution="zipf"
access_pattern_distribution="zipf"
wr_ratio=0.5
for content_skewness in 1.0
do
  for access_pattern_skewness in 1.0
  do
    for ca_bits in 1 2 3 4 5 6 7 8 9 10 11
    do
      for direct_io in 0 1
      do
        for write_buffer_size in 0 1 2 4 8
        do
          write_buffer_size=$((1024 * 256 * ${write_buffer_size}))
          mkdir -p results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}
          for comp in 3
          do
            #echo "${comp} ${workload} ${multithread} ${num_workers} ${ca_bits}" | tee results/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
            sync;
            echo 1 | sudo tee /proc/sys/vm/drop_caches
            #echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup --trace ../trace/compressibility_${comp}/${workload} --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} --access-pattern access_pattern/zipf_1.0_4kchunk_16krequest" | tee results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
            echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup \
              --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
              --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
              --access-pattern ../trace/access_pattern/${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}
              --wr-ratio ${wr_ratio}" \
              | tee results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
            stdbuf -oL sudo ./microbenchmarks/run_ssddup \
              --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
              --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
              --access-pattern ../trace/access_pattern/${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests} \
              --wr-ratio ${wr_ratio} \
              | tee -a results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}_${access_pattern_skewness}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
          done
        done
      done
    done
  done
done

access_pattern_distribution="uniform"
for content_skewness in 1.0
do
  for ca_bits in 1 2 3 4 5 6 7 8 9 10 11
  do
    for direct_io in 0 1
    do
      for write_buffer_size in 0 1 2 4 8
      do
        write_buffer_size=$((1024 * 256 * ${write_buffer_size}))
        mkdir -p results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}
        for comp in 3
        do
          #echo "${comp} ${workload} ${multithread} ${num_workers} ${ca_bits}" | tee results/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
          sync;
          echo 1 | sudo tee /proc/sys/vm/drop_caches
          #echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup --trace ../trace/compressibility_${comp}/${workload} --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} --access-pattern access_pattern/zipf_1.0_4kchunk_16krequest" | tee results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/${comp}_${workload}_${multithread}_${num_workers}_${ca_bits}
          echo "stdbuf -oL sudo ./microbenchmarks/run_ssddup \
            --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
            --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
            --access-pattern ../trace/access_pattern/${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests} \
            --wr-ratio ${wr_ratio}" \
            | tee results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
          stdbuf -oL sudo ./microbenchmarks/run_ssddup \
            --lba-bits 11 --ca-bits $ca_bits --multi-thread ${multithread} --num-workers ${num_workers} --direct-io ${direct_io} --write-buffer-size ${write_buffer_size} \
            --trace ../trace/compressibility_${comp}/${content_distribution}_${content_skewness}-chunk_size_${chunk_size}-num_unique_chunks_${num_unique_chunks}-num_chunks_${num_chunks} \
            --access-pattern ../trace/access_pattern/${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests} \
            --wr-ratio ${wr_ratio} \
            | tee -a results/direct_io_${direct_io}/wr_ratio_${wr_ratio}/write_buffer_${write_buffer_size}/compressibility_${comp}-content_${content_distribution}_${content_skewness}-access_pattern_${access_pattern_distribution}-num_chunks_${num_chunks}-num_requests_${num_requests}-ca_bits_${ca_bits}
        done
      done
    done
  done
done
