#!/bin/bash

file_list=("homes" "mail" "webvm")

echo "file bits : W R W/R nIO WSS ndata"
for ((i=13; i<=17; i++))
do
  cd "workspace/chunkSize_`echo "2^$(($i-10))" | bc -l`K"
  CNKSIZE=`echo "2^${i}" | bc -l`

  for ((K=0; K<${#file_list[@]}; K++))
  do
    if [[ "`ls ${file_list[$K]}_final_trace* | wc -l`" -eq 0 ]]; then
      echo "Warning: traces \"${file_list[$K]}_final_trace*\" does not exist"
      continue
    fi
    file=${file_list[$K]}
    RW=`awk 'BEGIN {R=0; W=0;} {if ($3=="W") W++; else R++;} END {print W" "R" "W/R;}' ${file}_final_trace*`

    NIO=`wc -l ${file}_final_trace* | tail -n 1 | awk '{print $1;}'`
    NIO=`echo $NIO | awk '{print $1 * '"$CNKSIZE"' / 1024 / 1024 / 1024;}'`

    WSS=`awk '{print $1;}' ${file}_final_trace_* | sort | uniq | wc -l`
    WSS=`echo $WSS | awk '{print $1 * '"$CNKSIZE"' / 1024 / 1024 / 1024;}'`

    NDATA=`awk '{print $4;}' ${file}_final_trace_* | sort | uniq | wc -l`
    NDATA=`echo $NDATA | awk '{print $1 * '"$CNKSIZE"' / 1024 / 1024 / 1024;}'`
    echo "$file $i : $RW $NIO $WSS $NDATA"
  done

  cd ../../
done
