#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "bpfreno.h"
#include "bpfreno.skel.h"
#include <unistd.h>
#include <string.h>

#define BICTCP_BETA_SCALE    1024	/* Scale factor beta calculation
					 * max_cwnd = snd_cwnd * beta
					 */
static struct bpfreno_bpf *skel;

// static char *ebpf_cc_binary_path  = "../.output/bpfreno.bpf.o";
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

int bpf_reno_init(void){
    int err;
    struct bpf_link* link;
    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);

    /* Bump RLIMIT_MEMLOCK to create BPF maps */
    bump_memlock_rlimit();

    /* Load and verify BPF application */
    skel = bpfreno_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        return 1;
    }

    /* Load & verify BPF programs */
    err = bpfreno_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }
	
    //load_structops_prog_from_file(ebpf_cc_binary_path);

    /* Attach tracepoints /
    err = bpfreno_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
    }
    */
   link = bpf_map__attach_struct_ops(skel->maps.reno);
   if(!link){
       printf("bpf_map__attach_struct_ops failed");
       bpfreno_bpf__destroy(skel);
   }
cleanup:
    return err < 0 ? -err : 0;
}

void bpf_reno_stop(void){
   /* unload bpf_cubic cc */
   //unload_bpf_cc2();
   /* Clean up */
   bpfreno_bpf__destroy(skel);
}

void load_bpf_reno(void){
   /* load bpf_reno as kernel module*/
   bpf_reno_init();
   /* set bpf_reno as default cc*/
   system("sysctl -w net.ipv4.tcp_congestion_control=bpf_reno");
}

void unload_bpf_reno(void){
   /* unload ebpf code module as kernel module*/
   bpf_reno_stop();
}
