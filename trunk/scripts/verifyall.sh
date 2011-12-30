#!/bin/bash

# set the path so that we can run progs linked with gcc4
export LD_LIBRARY_PATH=/usr/grads/share/gcc411/lib:$LD_LIBRARY_PATH

# set the benchmark exe name
if [ -n $1"" ]; then
    prog=./bench/obj/Bench_$1
else
    prog=./bench/obj/Bench_rstm
fi

# if the program does not exist, then exit
if ! [ -f $prog ]; then
    echo "File "$prog" not found"
    exit
fi

# set the duration and keys
duration=5
keys=256

echo "Testing $prog against 7 benchmarks at 12 threading levels and 4 configurations."
echo "This will take $((4*7*12*$duration/60)) minutes"

for v in "vis-lazy" "vis-eager" "invis-lazy" "invis-eager"
  do
    for bm in "HashTable" "RBTree" "LinkedList" "Counter" "LFUCache" \
              "RandomGraph" "LinkedListRelease"
      do
      for threads in 1 2 3 4 6 8 10 12 16 20 24 28
        do
        $prog -B $bm -p $threads -d $duration -m $keys -V $v
      done
    done
done
