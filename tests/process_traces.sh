#!/bin/bash

# This script helps you process all FIU IODedup traces 

DIR="../trace/fiu_trace"
GETINI="get_initial_state"
GETMOD="get_modified_trace"

echo "make sure these files are in the corrent directory:"

echo "$GETINI.cc $GETMOD.cc"
echo "homes.tar.gz  mail-01.tar.gz  mail-02.tar.gz  mail-03.tar.gz"
echo "mail-04.tar.gz  mail-05.tar.gz  mail-06.tar.gz  mail-07.tar.gz"
echo "mail-08.tar.gz  mail-09.tar.gz  mail-10.tar.gz  web-vm.tar.gz"
echo ""

mkdir -p $DIR

set -x

echo "0. Compiling..."
g++ -std=gnu++0x $GETINI.cc -o $DIR/$GETINI
g++ -std=gnu++0x $GETMOD.cc -o $DIR/$GETMOD -lcrypto

# 1. Homes
echo "1. Dealing with \"homes\"..."
echo "Extracting homes.tar.gz ..."
tar -xzf homes.tar.gz --directory $DIR/
cd $DIR

echo "Generating initial state of homes ..."
./$GETINI homes 21
echo "Generating modified trace of homes ..."
./$GETMOD homes homes_initial_state.txt 21
cd ..

# 2. Mail
echo "2. Dealing with \"mail\"..."
for ((i=1; i<=10; i++))
do
  NUM=`printf "%02d" $i`
  echo "Extracting mail-$NUM.tar.gz ..."
  tar -xzf mail-$NUM.tar.gz --directory $DIR/
done

cd $DIR
sed -i '1,3d' cheetah.cs.fiu.edu-110108-113008.1.blkparse

echo "Generating initial state of mail ..."
./$GETINI mail 21
echo "Generating modified trace of mail ..."
./$GETMOD mail mail_initial_state.txt 21

cd ..

# 3. webvm
echo "3. Dealing with \"webvm\"..."
NUM=`printf "%02d" $i`
echo "Extracting web-vm.tar.gz ..."
tar -xzf web-vm.tar.gz --directory $DIR/

cd $DIR
echo "Generating initial state of web-vm ..."
./$GETINI webvm 21
echo "Generating modified trace of web-vm ..."
./$GETMOD webvm webvm_initial_state.txt 21

rm *.blkparse
rm *_initial_state.txt
