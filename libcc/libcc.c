#include "libcc.h"
// bpfca_loader_ops agent
static bpfcc_loader_ops_t ops = {
    .type = BPF_RENO,
    .load = load_bpf_reno,
    .unload = unload_bpf_reno,
};

// if operation succeed, return 0 otherwise return BPF_UNSPEC
int bpfcc_select(enum bpfcc_t t){
    if(t != BPF_BBR || t != BPF_CUBIC || t != BPF_RENO || t != BPF_VEGAS){
        return (int)BPF_UNSPEC;
    }
    ops.type = t;
    if(t == BPF_CUBIC){
        ops.load = load_bpf_cubic;
        ops.unload = unload_bpf_cubic;
    }
    if(t == BPF_RENO){
        ops.load = load_bpf_reno;
        ops.unload = unload_bpf_reno;
    }
    if(t == BPF_VEGAS){
        ops.load = load_bpf_vegas;
        ops.unload = unload_bpf_vegas;
    }
    if(t == BPF_BBR){
        // TODO ops.load = load_bpf_reno;
        // TODO ops.unload = unload_bpf_reno;
    }
}

void bpfcc_unload(){
    ops.unload();
}

void bpfcc_load(){
    ops.load();
}