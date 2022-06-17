// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2020 Facebook */
#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "bpfcubic.h"
#include "bpfcubic.skel.h"
#include "../loader/loader.h"

#define BICTCP_BETA_SCALE    1024	/* Scale factor beta calculation
					 * max_cwnd = snd_cwnd * beta
					 */
static struct bpfcubic_bpf *skel;

static char *ebpf_cc_binary_path  = "../.output/bpfcubic.bpf.o";
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

int bpf_cubic_init(void){
    int err;

    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);

    /* Bump RLIMIT_MEMLOCK to create BPF maps */
    bump_memlock_rlimit();

    /* Load and verify BPF application */
    skel = bpfcubic_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        return 1;
    }

    /* Load & verify BPF programs */
    err = bpfcubic_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }
	
    load_structops_prog_from_file(ebpf_cc_binary_path);

    /* Attach tracepoints */
    err = bpfcubic_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
    }

    skel->bss->beta = 717;
    skel->bss->beta_scale  = 8*(BICTCP_BETA_SCALE+skel->bss->beta) / 3
				/ (BICTCP_BETA_SCALE - skel->bss->beta);
cleanup:
    return err < 0 ? -err : 0;
}

void bpf_cubic_stop(void){
   /* unload bpf_cubic cc */
   unload_bpf_cc2();
   /* Clean up */
   bpfcubic_bpf__destroy(skel);
}

void load_bpf_cubic(void){
   /* load bpf_cubic as kernel module*/
   bpf_cubic_init();
   /* set bpf_cubic as default cc*/
   //system("sysctl -w net.ipv4.tcp_congestion_control=bpf_cubic");
}

void unload_bpf_cubic(void){
   /* unload ebpf code module as kernel module*/
   bpf_cubic_stop();
}
