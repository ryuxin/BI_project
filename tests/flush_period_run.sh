#!/bin/bash
if [ $# != 1 ]; then
  echo "Usage: $0 ntimer"
  exit 1
fi

# ntimer -1, 10, 100, 1000, 10000
#./flush_period.test $1 1 >> flush_period_output
#./flush_period.test $1 4 >> flush_period_output
#./flush_period.test $1 16 >> flush_period_output
./flush_period.test  -1 64 1 >> flush_period_output
#./flush_period.test $1 256 >> flush_period_output
#./flush_period.test $1 1024 >> flush_period_output
#./flush_period.test $1 4096 >> flush_period_output
#./flush_period.test $1 16384 >> flush_period_output
./flush_period.test -1 8192 1 >> flush_period_output
./flush_period.test -1 131072 1 >> flush_period_output
./flush_period.test -1 1048576 1 >> flush_period_output
#./flush_period.test $1 262144 >> flush_period_output
#./flush_period.test $1 1048576 >> flush_period_output

./flush_period.test 100 64 0 >> flush_period_output
./flush_period.test 100 8192 0 >> flush_period_output
./flush_period.test 100 131072 0 >> flush_period_output
./flush_period.test 100 1048576 0 >> flush_period_output

./flush_period.test 10 64 0 >> flush_period_output
./flush_period.test 10 8192 0 >> flush_period_output
./flush_period.test 10 131072 0 >> flush_period_output
./flush_period.test 10 1048576 0 >> flush_period_output

./flush_period.test -1 64 0 >> flush_period_output
./flush_period.test -1 8192 0 >> flush_period_output
./flush_period.test -1 131072 0 >> flush_period_output
./flush_period.test -1 1048576 0 >> flush_period_output

