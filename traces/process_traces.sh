#!/bin/bash

DIR="workspace"
GETINI="get_initial_state"
GETMOD="get_modified_trace"

filelist=("$GETINI.cc" "$GETMOD.cc" "homes.tar.gz" "mail-01.tar.gz" "mail-02.tar.gz" "mail-03.tar.gz" "mail-04.tar.gz" "mail-05.tar.gz" "mail-06.tar.gz" "mail-07.tar.gz" "mail-08.tar.gz" "mail-09.tar.gz" "mail-10.tar.gz" "web-vm.tar.gz")

echo "make sure these files are in the current directory:"
for ((K=0; K<${#filelist[@]}; K++)) 
do
  printf "checking ${filelist[$K]} ..."
  if [ -f "${filelist[$K]}" ]; then
    echo " ok"
  else
    echo " not found. exiting..."
    exit
  fi
done

mkdir -p $DIR

echo "0. Compiling..."
if ! g++ -std=gnu++0x ${GETINI}.cc -o $DIR/$GETINI; then
  echo "compile failed: ${GETINI}.cc"
  exit
fi
if ! g++ -std=gnu++0x $GETMOD.cc -o $DIR/$GETMOD -lcrypto -lm; then
  echo "compile failed: ${GETMOD}.cc"
  exit
fi

# 01-homes
printf "Extracting homes.tar.gz ... "
if [ -f "$DIR/homes-110108-112108.1.blkparse" ]; then
  echo "exists"
else
  tar -xzf homes.tar.gz --directory $DIR/
  echo "ok"
fi

## 02-mail
for ((i=1; i<=10; i++))
do
  NUM=`printf "%02d" $i`
  printf "Extracting mail-$NUM.tar.gz ... "
  if [ ! -f "$DIR/cheetah.cs.fiu.edu-110108-113008.`echo "scale=0; ${i}*2-1 + (${i}/6)" | bc -l`.blkparse" ]; then
    tar -xzf mail-$NUM.tar.gz --directory $DIR
    if [ $i -eq 1 ]; then
      sed -i '1,3d' $DIR/cheetah.cs.fiu.edu-110108-113008.1.blkparse
      sed -i '$d' $DIR/cheetah.cs.fiu.edu-110108-113008.1.blkparse
    fi
    echo "ok"
  else
    echo "exists"
  fi
done

# 03-webvm
printf "Extracting web-vm.tar.gz ... "
if [ -f "$DIR/webmail+online.cs.fiu.edu-110108-113008.1.blkparse" ]; then
  echo "exists"
else
  tar -xzf web-vm.tar.gz --directory $DIR/
  echo "ok"
fi

cd $DIR
for ((i = 13; i <= 17; i++))
do
  CNKSIZE="`echo "2^$(($i-10))" | bc -l`K"
  rm -rf chunkSize_$CNKSIZE
  mkdir chunkSize_$CNKSIZE
done

# 04. Start generating
traces=("homes" "mail" "webvm")
for ((K=0; K<${#traces[@]}; K++))
do
  for ((i = 13; i <= 17; i++))
  do
    TRACE=${traces[$K]}
    INI_FILE=${TRACE}_ini_chunkSize_$CNKSIZE.txt
    NEW_INI_FILE=${TRACE}_initial_state.txt
    CNKSIZE="`echo "2^$(($i-10))" | bc -l`K"

    echo "Chunk size: $CNKSIZE"
    echo "Generating initial state of $TRACE ..."
    if [ -f $INI_FILE ]; then
      mv $INI_FILE $NEW_INI_FILE 
    else
      ./$GETINI $TRACE $i
    fi
    echo "Generating modified trace of $TRACE ..."
    ./$GETMOD $TRACE $NEW_INI_FILE $i
    mv ${TRACE}_final* chunkSize_$CNKSIZE/ 
    mv $NEW_INI_FILE $INI_FILE 
  done
done
