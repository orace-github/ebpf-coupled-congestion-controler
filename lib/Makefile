LIBCUBIC_CC := $(abspath ../bpf_cubic/.output/libcubiccc.a)
LIBRENO_CC := $(abspath ../bpf_reno/.output/librenocc.a)
LIBVEGAS_CC := $(abspath ../bpf_vegas/.output/libvegascc.a)
LIBBPF_OBJ := $(abspath ../bpf_cubic/.output/libbpf.a)

all: lib.o libebpfcc.a libbpf.a

lib.o: lib.c
	gcc -c $< -o $@

libebpfcc.a: lib.o $(CUBIC_OUTPUT)/loader.o $(CUBIC_OUTPUT)/bpfcubic.o \
$(RENO_OUTPUT)/loader.o $(RENO_OUTPUT)/bpfreno.o \
$(VEGAS_OUTPUT)/loader.o $(VEGAS_OUTPUT)/bpfvegas.o
	ar -rcs $@ $^

libbpf.a: 
	cp $(CUBIC_OUTPUT)/libbpf.a ./

clean:
	rm *.o
	rm *.a