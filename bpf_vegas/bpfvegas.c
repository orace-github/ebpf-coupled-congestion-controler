#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "../loader/loader.h"
#include "bpfvegas.h"
#include "bpfvegas.skel.h"
#include <unistd.h>
#include <string.h>

#define BICTCP_BETA_SCALE    1024	/* Scale factor beta calculation
					 * max_cwnd = snd_cwnd * beta
					 */
static struct bpfvegas_bpf *skel;

static char *ebpf_cc_binary_path  = "../.output/bpfvegas.bpf.o";
static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args){
    if (level == LIBBPF_DEBUG)
        return 0;
    return vfprintf(stderr, format, args);
}

static void bump_memlock_rlimit(void){
    struct rlimit rlim_new = {
        .rlim_cur	= RLIM_INFINITY,
        .rlim_max	= RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}

int bpf_vegas_init(void){
    int err;

    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);

    /* Bump RLIMIT_MEMLOCK to create BPF maps */
    bump_memlock_rlimit();

    /* Load and verify BPF application */
    skel = bpfvegas_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        return 1;
    }

    /* Load & verify BPF programs */
    err = bpfvegas_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }
	
    load_structops_prog_from_file(ebpf_cc_binary_path);

    /* Attach tracepoints */
    err = bpfvegas_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
    }

cleanup:
    return err < 0 ? -err : 0;
}

void bpf_vegas_stop(void){
   /* unload bpf_cubic cc */
   unload_bpf_cc2();
   /* Clean up */
   bpfvegas_bpf__destroy(skel);
}

void load_bpf_vegas(void){
   /* load bpf_vegas as kernel module*/
   bpf_vegas_init();
   /* set bpf_vegas as default cc*/
   //system("sysctl -w net.ipv4.tcp_congestion_control=bpf_vegas");
}

void unload_bpf_vegas(void){
   /* unload ebpf code module as kernel module*/
   bpf_vegas_stop();
}
