#! /bin/bash

cd bpf_cubic/ && make clean && make
cd ../bpf_reno/ && make clean && make
cd ../bpf_vegas/ && make clean && make

