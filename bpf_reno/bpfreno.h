#ifndef __BPFRENO_H__
#define __BPFRENO_H__
int bpf_reno_init();
void bpf_reno_stop(void);
void unload_bpf_reno(void);
void load_bpf_reno(void);
#endif //__BPFRENO_H__