#!/bin/bash

FILE="webvm_new.csv"

rm -f $FILE

print_keyword_prev_suff() {
  KEYWORD_=$1
  PREV=$2
  P1=$3
  P2=$4
  P3=$5
  SUFF=$6
  PRINTNUM=$7
  KEYWORD2=$8

  if [[ $PRINTNUM == "" ]]; then
    PRINTNUM="\$NF"
  fi

  if [[ $KEYWORD2 == "" ]]; then
    echo "${KEYWORD_}:" >> $FILE
  else
    echo "${KEYWORD2}:" >> $FILE
  fi

  printf "$P1," >> $FILE
  grep -r "$KEYWORD_" ../results/${PREV}_${P1}*${SUFF} | awk -F 'ca|lba| |_|:|;' '{print '$PRINTNUM';}' | sed -e 'H;${x;s/\n/,/g;s/^,//;p;};d' >> $FILE

  if [[ $P2 != "" ]]; then
    printf "$P2," >> $FILE
    grep -r "$KEYWORD_" ../results/${PREV}_${P2}*${SUFF} | awk -F 'ca|lba| |_|:|;' '{print '$PRINTNUM';}' | sed -e 'H;${x;s/\n/,/g;s/^,//;p;};d' >> $FILE
  fi

  if [[ $P3 != "" ]]; then
    printf "$P3," >> $FILE
    grep -r "$KEYWORD_" ../results/${PREV}_${P3}*${SUFF} | awk -F 'ca|lba| |_|:|;' '{print '$PRINTNUM';}' | sed -e 'H;${x;s/\n/,/g;s/^,//;p;};d' >> $FILE
  fi
  echo "" >> $FILE
}

print_cache_size() {
  echo "Fingerprint Index capacity: (MiB)" >> $FILE
  printf "darc," >> $FILE
  SUM=2
  for ((i=1; i<=18; i++))
  do
    printf "$SUM," >> $FILE
    SUM=$(( $SUM * 2))
  done
  echo "" >> $FILE

  printf "dlru," >> $FILE
  SUM=2
  for ((i=1; i<=18; i++))
  do
    printf "$SUM," >> $FILE
    SUM=$(( $SUM * 2))
  done
  echo "" >> $FILE

  printf "ours,2," >> $FILE
  SUM=2
  for ((i=4; i<=18; i++))
  do
    printf "$SUM," >> $FILE
    SUM=$(( $SUM * 2))
  done
  echo "" >> $FILE
}

# Print Hit ratio
print_keyword_prev_suff "Hit ratio" "webvm" "darc" "dlru" "ours" "new"
print_keyword_prev_suff "Dup ratio" "webvm" "darc" "dlru" "ours" "new"
print_keyword_prev_suff "Dup write to the total write ratio:" "webvm" "darc" "dlru" "ours" "new"

# Print fingerprint size
print_cache_size

# Print VM and RSS
print_keyword_prev_suff "VM.*e+0" "webvm" "darc" "dlru" "ours" "new" "\$12" "VM"
print_keyword_prev_suff "VM.*e+0" "webvm" "darc" "dlru" "ours" "new" "\$16" "RSS"

# Print Num total write and read
print_keyword_prev_suff "Num total write" "webvm" "darc" "dlru" "ours" "new" 
print_keyword_prev_suff "Num total read" "webvm" "darc" "dlru" "ours" "new" 

# Print R/W from/to SSD
print_keyword_prev_suff "Num bytes data written to ssd" "webvm" "darc" "dlru" "ours" "new" 
print_keyword_prev_suff "Num bytes data read from ssd" "webvm" "darc" "dlru" "ours" "new" 
print_keyword_prev_suff "Num bytes metadata written to ssd" "webvm" "darc" "dlru" "ours" "new" 
print_keyword_prev_suff "Num bytes metadata read from ssd" "webvm" "darc" "dlru" "ours" "new" 
