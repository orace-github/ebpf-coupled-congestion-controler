LIBCUBIC_CC=../.output/bpfcubic.o
LIBRENO_CC=../.output/bpfreno.o
LIBVEGAS_CC=../.output/bpfvegas.o
LIBBPF_OBJ=../.output/libbpf.a

OUTPUT=../.output

all: $(OUTPUT)/libcc.o $(OUTPUT)/libebpfcc.a

$(OUTPUT)/libcc.o: libcc.c | $(OUTPUT)
	gcc -c $< -o $@

$(OUTPUT)/libebpfcc.a: $(OUTPUT)/libcc.o $(LIBCUBIC_CC) $(LIBRENO_CC) $(LIBVEGAS_CC) | $(OUTPUT)
	ar -rcs $@ $^

clean:
	rm $(OUTPUT)/libcc.o $(OUTPUT)/libebpfcc.a