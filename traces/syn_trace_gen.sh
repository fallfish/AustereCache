#!/bin/bash

CC_FILE=syn_trace_gen.cc
echo "make sure this file is in the current directory:"
printf "checking $CC_FILE ..."
if [ -f "$CC_FILE" ]; then
  echo " ok"
  if ! g++ -std=c++11 $CC_FILE -o generate; then
    echo "compile failed: $CC_FILE"
    exit
  fi 
else
  echo " not found. exiting..."
  exit
fi

rw_ratio=("0.111111" "0.42857143" "1" "2.3333333" "9")
rw_file=("19" "37" "55" "73" "91")
dup_ratio=("0.2" "0.4" "0.6" "0.8")

shortWss=128 # MB
io=5 # GB
shortDisk=5 # GB
selection=3

max_addr=$((1073741824*$shortDisk))
tl=$(($io * 1024 * 32))  
wss=$(($shortWss * 1024 * 1024))
workType="uniform"
if [[ $selection -eq 1 ]]; then
  workType="hotCold"
elif [[ $selection -eq 2 ]]; then
  workType="seqUniform"
elif [[ $selection -eq 3 ]]; then
  workType="zipf"
fi

filePrefix="syn_traces"

rm -rf $filePrefix
mkdir $filePrefix

for ((i=0; i<${#rw_ratio[@]}; i++))
do
  for ((j=0; j<${#dup_ratio[@]}; j++))
  do
    dup=${dup_ratio[$j]}
    if [[ $i -ne 1 ]]; then
      dup=${dup_ratio[1]}
    fi
    filename=$filePrefix/rw${rw_file[$i]}_dup${dup}.txt

    ./generate --wss $wss --selection $selection --max_addr $max_addr --io_dup_ratio ${dup} --trace_length ${tl} --rw_ratio ${rw_ratio[$i]} --filename $filename

    if [[ $i -ne 1 ]]; then
      break
    fi
  done
done
