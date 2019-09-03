#!/bin/bash

FILE="mail.csv"

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

  printf "ours,0.5," >> $FILE
  SUM=1
  for ((i=2; i<=18; i++))
  do
    printf "$SUM," >> $FILE
    SUM=$(( $SUM * 2))
  done
  echo "" >> $FILE
}

# Print Hit ratio
print_keyword_prev_suff "Hit ratio" "mail" "darc" "dlru" "ours" "lba13"
print_keyword_prev_suff "Dup ratio" "mail" "darc" "dlru" "ours" "lba13"
print_keyword_prev_suff "Dup write to the total write ratio:" "mail" "darc" "dlru" "ours" "lba13"

# Print fingerprint size
print_cache_size

# Print VM and RSS
print_keyword_prev_suff "VM.*e+" "mail" "darc" "dlru" "ours" "lba13" "\$11" "VM"
print_keyword_prev_suff "VM.*e+" "mail" "darc" "dlru" "ours" "lba13" "\$15" "RSS"

# Print Num total write and read
print_keyword_prev_suff "Num total write" "mail" "darc" "dlru" "ours" "lba13" 
print_keyword_prev_suff "Num total read" "mail" "darc" "dlru" "ours" "lba13" 

# Print R/W from/to SSD
print_keyword_prev_suff "Num bytes data written to ssd" "mail" "darc" "dlru" "ours" "lba13" 
print_keyword_prev_suff "Num bytes data read from ssd" "mail" "darc" "dlru" "ours" "lba13" 
print_keyword_prev_suff "Num bytes metadata written to ssd" "mail" "darc" "dlru" "ours" "lba13" 
print_keyword_prev_suff "Num bytes metadata read from ssd" "mail" "darc" "dlru" "ours" "lba13" 
