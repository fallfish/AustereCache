for t in {1..8}
do
  src=4294967296
  dst=$((64 * 1024 * 1024 * 1024))
  sed -i 's/'${src}'/'${dst}'/g' $t.json
  src=$(( ${t} * 512 * 1024 * 1024 ))
  dst=$(( ${t} * 8 * 1024 * 1024 * 1024 ))
  sed -i 's/'${src}'/'${dst}'/g' $t.json
done
  
