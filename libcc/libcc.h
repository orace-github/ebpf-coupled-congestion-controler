#ifndef __LIB_H__
#define __LIB_H__
// TODO: #include "../bpf_bbr/bpfbbr.h"
#include "../bpf_cubic/bpfcubic.h"
#include "../bpf_reno/bpfreno.h"
#include "../bpf_vegas/bpfvegas.h"

enum bpfca_t{
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
    enum bpfca_t type;
}bpfca_loader_ops_t;

int bpfca_select(enum bpfca_t);
void bpfca_unload();
void bpfca_load();
#endif //__LIB_H__