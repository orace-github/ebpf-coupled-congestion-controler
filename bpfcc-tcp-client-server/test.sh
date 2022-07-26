#!/bin/bash

for i in `seq 10`
do
   dest="/tmp/"
   out="/tmp/"
   rlog="/tmp/r"
   slog="/tmp/s"
   rlog+=$i
   slog+=$i
   out+=$i
   dest+=$i
   dest+=$4
   ./client -s $1 -p $2 -P $3 -i $4 -o $dest -S $rlog -R $slog -f  &
done
