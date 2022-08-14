OUTPUT=../.output/
all: bpfcc.o bpfcc

bpfcc.o: bpfcc.c
	gcc -c  bpfcc.c -o bpfcc.o

bpfcc: bpfcc.o
	gcc -g bpfcc.o -o bpfcc -L$(OUTPUT) -lebpfcc -lbpf -lelf -lz

clean:
	rm *.o
	rm bpfcc
