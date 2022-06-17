#ifndef __LIB_H__
#define __LIB_H__
// TODO: #include "../bpf_bbr/bpfbbr.h"
#include "../bpf_cubic/bpfcubic.h"
#include "../bpf_reno/bpfreno.h"
#include "../bpf_vegas/bpfvegas.h"

enum bpfcc_t{
    BPF_CUBIC,
    BPF_RENO,
    BPF_VEGAS,
    BPF_BBR,
    BPF_UNSPEC, // error type
    };

// ## By default, bpf_reno is loaded ##
typedef struct {
    void (*load)(void);
    void (*unload)(void);
    enum bpfcc_t type;
}bpfcc_loader_ops_t;

int bpfcc_select(enum bpfcc_t);
void bpfcc_unload();
void bpfcc_load();
#endif //__LIB_H__