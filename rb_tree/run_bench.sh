#!/bin/bash

# node num [1k 2k 4k 8k 16k 32k 64k 128k 256k 512k 1024k]
# [1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576]
# update r [0 5 10 .. 45 50]
# tot sockets [1 2, 3, 4 ... 8]

if [ $# != 6 ]; then
	echo "Usage: $0 start_par num_par num_node num_core num_obj update "
	exit 1
fi

for (( i=$1; i<$1+$2; i++ ))
do
	tf="out_"
	tf+=$i
#	bench="./rbtree_bi_bench.test"
	bench="./rbtree_rcu_bench.test"
	bench+=" -i "
	bench+=$i
	bench+=" -n "
	bench+=$3
	bench+=" -c "
	bench+=$4
	bench+=" -m "
	bench+=$5
	bench+=" -u "
	bench+=$6
	echo $bench > $tf
	$bench >> $tf &
done

sleep 10m

dir="cbtree_results/"
mkdir -p $dir
fn="n_"
fn+=$3
fn+="_c_"
fn+=$4
fn+="_m_"
fn+=$5
fn+="_u_"
fn+=$6
dir+=$fn
echo "$0 $1 $2 $3 $4 $5 $6" > $dir
echo "======================================" > $dir
for (( i=$1; i<$1+$2; i++ ))
do
	tf="out_"
	tf+=$i
	cat $tf >> $dir
#	rm $tf
done
