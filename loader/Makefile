OUTPUT := ../.output
CLANG := ${CLANG}
STRIP := ${STRIP}

ifeq ($(CLANG),)
CLANG := clang
endif

ifeq ($(STRIP),)
STRIP := llvm-strip
endif
  
INCLUDES := -I$(OUTPUT) -I.

CFLAGS += -Wall -O2 $(DBG) -fPIE

all: $(OUTPUT)/loader.o		

$(OUTPUT)/%.o : %.c | $(OUTPUT)
	$(CLANG) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean: $(OUTPUT)/loader.o
	rm $<