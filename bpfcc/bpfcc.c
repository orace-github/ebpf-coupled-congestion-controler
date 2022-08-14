#include <stdio.h>
#include <stdlib.h>
#include "../libcc/libcc.h"
#include <argp.h>
#include <string.h>

const char *argp_program_version = "bpfcc-loader-unloader";
const char *argp_program_bug_address = "<oracekpakpo0@gmail.com>";
const char argp_program_doc[] =
    "bpfcc loader, unloader application.\n"
    "\n"
    "USAGE: ./bpfcc --help \n";

static const struct argp_option opts[] = {
    {"operation", 'o', "OPERATION", 0, "operation = load | unload"},
    {"bpfcc", 'c', "BPF-CC", 0, "bpf congestion controler"},
    {},
};

static struct env{
  void (*load_bpfcc)();
  void (*unload_bpfcc)();
  int operation;
} env;

#define BPFCC_LOAD 0x01
#define BPFCC_UNLOAD 0x02

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
  switch (key)
  {
  case 'c':
    if(!strcmp(arg,"bpf_reno")){
      env.load_bpfcc = load_bpf_reno;
      env.unload_bpfcc = unload_bpf_reno;
    }
    else if(!strcmp(arg,"bpf_vegas")){
      env.load_bpfcc = load_bpf_vegas;
      env.unload_bpfcc = unload_bpf_vegas;
    } 
    else if(!strcmp(arg,"bpf_cubic")){
      env.load_bpfcc = load_bpf_cubic;
      env.unload_bpfcc = unload_bpf_cubic;
    }
    else{
      fprintf(stderr, "unsupported congestion controler\n");
      argp_usage(state);
    }
    break;
  case 'o':
    if(!strcmp(arg,"load")){
        env.operation = BPFCC_LOAD;
    }
    else if(!strcmp(arg,"unload")){
        env.operation = BPFCC_UNLOAD;
    } 
    else{
      fprintf(stderr, "unsupported operation\n");
      argp_usage(state);
    }
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static const struct argp argp = {
    .options = opts,
    .parser = parse_arg,
    .doc = argp_program_doc,
};


int main(int argc, char * argv[]){
    int err;
    /* Parse command line arguments */
    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err)
        return err;
    // load bpfcc
    if(env.operation == BPFCC_LOAD)
        env.load_bpfcc();
    // unload bpfcc
    if(env.operation == BPFCC_UNLOAD)
        env.unload_bpfcc();
    return 0;
}
