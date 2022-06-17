PREFIX = /usr/local
DBG = -g -ggdb
INSTALL := install

MAKEFLAGS := --jobs=$(shell nproc)

ARCH := $(shell uname -m | sed 's/x86_64/x86/')

OUTPUT := ../.output
LIBBPF_SRC := $(abspath ../libbpf/src)
LIBBPF_OBJ := $(abspath ./$(OUTPUT)/libbpf.a)
LIBBPF_OBJDIR := $(abspath ./$(OUTPUT)/libbpf)
LIBBPF_DESTDIR := $(abspath ./$(OUTPUT))

BPFTOOL := $(abspath ../tools/bpftool)

CLANG := ${CLANG}
STRIP := ${STRIP}

ifeq ($(CLANG),)
CLANG := clang
endif

ifeq ($(STRIP),)
STRIP := llvm-strip
endif
  
INCLUDES := -I$(OUTPUT) -I. -Iebpf/

CFLAGS += -Wall -O2 $(DBG) -fPIE


PROGRAM=./$(OUTPUT)/libcubiccc.a

SOURCES = $(wildcard *.c) $(wildcard ../*.c)
OBJECTS = $(foreach wrd,$(subst .c,.o,$(SOURCES)),$(OUTPUT)/$(wrd))


all: $(LIBBPF_OBJ) $(OUTPUT)/bpfcubic.o $(OBJECTS)
	ar -rcs   $(PROGRAM) \
		$(OUTPUT)/bpfcubic.o \
		

$(OUTPUT) $(OUTPUT)/libbpf $(OUTPUT)/ebpf:
	mkdir -p $@

$(OUTPUT)/%.o: %.c %.h $(LIBBPF_OBJ) $(OUTPUT)/%.skel.h vmlinux.h | $(OUTPUT)
	$(CLANG) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT)
	$(BPFTOOL) gen skeleton $< > $@

$(OUTPUT)/%.bpf.o: %.bpf.c %.h $(LIBBPF_OBJ) vmlinux.h | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		$(INCLUDES) -c $(filter %.c,$^) -o $@ && \
		$(STRIP) -g $@

$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch]) | $(OUTPUT)/libbpf
	$(MAKE) -C $(LIBBPF_SRC) \
		BUILD_STATIC_ONLY=1 \
		OBJDIR=$(LIBBPF_OBJDIR) \
		DESTDIR=$(LIBBPF_DESTDIR) \
		INCLUDEDIR= LIBDIR= UAPIDIR= install
clean:
	rm -rf $(OUTPUT)/bpfcubic.* $(PROGRAM)

install: $(PROGRAM)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/bin
	$(INSTALL) $(PROGRAM) $(DESTDIR)$(prefix)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)

# keep intermediate (.skel.h, .bpf.o, etc) targets
.SECONDARY:
