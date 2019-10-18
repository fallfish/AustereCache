#!/bin/bash

#set -x

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=20; i++))
#do
  #j=13
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/webvm_final_trace_%02d.txt 1 21 2> ../results/webvm_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/webvm_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=1 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=22; i++))
#do
  #j=13
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/webvm_final_trace_%02d.txt 1 21 2> ../results/webvm_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/webvm_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=1 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=22; i++))
#do
  #j=13
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/webvm_final_trace_%02d.txt 1 21 2> ../results/webvm_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/webvm_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done

#!/bin/bash

set -x

cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
make -j
for ((i=1; i<=19; i++))
do
  j=17
  NUM_i=`printf "%02d" $i`
  NUM_j=`printf "%02d" $j`
  ./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/mail_final_trace_%02d.txt 1 21 2> ../results/mail_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/mail_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
done

cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=1 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
make -j
for ((i=1; i<=22; i++))
do
  j=17
  NUM_i=`printf "%02d" $i`
  NUM_j=`printf "%02d" $j`
  ./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/mail_final_trace_%02d.txt 1 21 2> ../results/mail_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/mail_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
done

cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=1 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
make -j
for ((i=1; i<=22; i++))
do
  j=17
  NUM_i=`printf "%02d" $i`
  NUM_j=`printf "%02d" $j`
  ./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/mail_final_trace_%02d.txt 1 21 2> ../results/mail_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/mail_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
done

#set -x

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=20; i++))
#do
  #j=15
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/homes_final_trace_%02d.txt 1 21 2> ../results/homes_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/homes_ours_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=1 -DDLRU=0 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=22; i++))
#do
  #j=15
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/homes_final_trace_%02d.txt 1 21 2> ../results/homes_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/homes_darc_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done

#cmake -j .. -DCMAKE_BUILD_TYPE=Release -DREPLAY_FIU=1 -DDARC=0 -DDLRU=1 -DCDARC=0 -DFAKE_IO=1 -DNORMAL_DIST_COMPRESSION=0
#make -j
#for ((i=1; i<=22; i++))
#do
  #j=15
  #NUM_i=`printf "%02d" $i`
  #NUM_j=`printf "%02d" $j`
  #./microbenchmarks/run_fiu --ca-bits ${i} --lba-bits ${j} --multi-thread 0 --num-workers 1 --access-pattern ../trace/fiu_test/homes_final_trace_%02d.txt 1 21 2> ../results/homes_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}_statistics | tee ../results/homes_dlru_fakeio_noNormal_ca${NUM_i}_lba${NUM_j}
#done
