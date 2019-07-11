#!/bin/bash
for i in 11
do
  echo $i
  stdbuf -oL sudo bash run_system.sh $i  | tee result_ca_bucket_bits_${i}
done
