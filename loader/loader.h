#ifndef __LOADCUBIC_H__
#define __LOADCUBIC_H__
int load_bpf_prog2(uint8_t *prog_buff, int f_sz, int is_server);
int unload_bpf_cc2(void);
int load_structops_prog_from_file(char *path);
#endif //__LOADCUBIC_H__
