#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include "message.h"
#include "../libcc/libcc.h"
#include <argp.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

const char *argp_program_version = "tcp-client-server-bpfcc.0.0";
const char *argp_program_bug_address = "<oracekpakpo0@gmail.com>";
const char argp_program_doc[] =
    "BPFCC tcp-client-server application.\n"
    "\n"
    "server running under specific bpf controler congestion send/receive file to/from client \n"
    "USAGE: ./server --help \n";

static const struct argp_option opts[] = {
    {"ip", 's', "IP-ADDR", 0, "server ip address"},
    {"port", 'p', "PORT1", 0, "Port1"},
    {"Port", 'P', "PORT2", 0, "Port2"},
    {"input", 'i', "INPUT", 0, "input file"},
    {"output", 'o', "OUTPUT", 0, "output file"},
    {"fast", 'f', 0, 0, "enable fast transfer"},
    {"sndfile", 'S', "FILE-TO-SEND", 0, "file to send"},
    {"rcvfile", 'R', "FILE-TO-RECEIVE", 0, "file to receive"},
    {"bpfcc", 'c', "BPF-CC", 0, "bpf congestion controler"},
    {},
};

static struct env{
  struct list recv_list;
  struct list send_list;
  void (*bandwidth_analyzer)(void*); // called from log function
  char *server_addr;
  char *port;
  char *port2;
  char *send_log_file;
  char *recv_log_file;
  char *src_file;
  char *dst_file;
  int fast_transfer;
  void (*load_bpfcc)();
  void (*unload_bpfcc)();
  char* bpfcc;
  char sysctl_cmd[64]; // buffer overflow, check later: critical since i intend to run system(sysctl_cmd);
  struct{
    u8 bpf_cubic : 1;
    u8 bpf_reno : 1;
    u8 bpf_vegas: 1;
    u8 unused : 5;
  }flags;
} env;

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
  switch (key)
  {
  case 'p':
    if(strlen(arg) > 5 || !atoi(arg)){
      fprintf(stderr, "invalid port, between 0-65535\n");
      argp_usage(state);
    }
    env.port = arg;
    break;
  case 'P':
    if(strlen(arg) > 5 || !atoi(arg)){
      fprintf(stderr, "invalid port, between 0-65535\n");
      argp_usage(state);
    }
    env.port2 = arg;
    break;   
  case 'c':
    if(!strcmp(arg,"bpf_reno")){
      env.load_bpfcc = load_bpf_reno;
      env.unload_bpfcc = unload_bpf_reno;
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
    }
    else if(!strcmp(arg,"bpf_vegas")){
      env.load_bpfcc = load_bpf_vegas;
      env.unload_bpfcc = unload_bpf_vegas;
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
    } 
    else if(!strcmp(arg,"bpf_cubic")){
      env.load_bpfcc = load_bpf_cubic;
      env.unload_bpfcc = unload_bpf_cubic;
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
    }
    else if(!strcmp(arg,"cubic")){
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
      env.load_bpfcc = NULL;
      env.unload_bpfcc = NULL;
    }
    else if(!strcmp(arg,"reno")){
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
      env.load_bpfcc = NULL;
      env.unload_bpfcc = NULL;
    }
    else if(!strcmp(arg,"vegas")){
      env.bpfcc = arg;
      sprintf(env.sysctl_cmd,"sysctl -w net.ipv4.tcp_congestion_control=%s",arg);
      env.load_bpfcc = NULL;
      env.unload_bpfcc = NULL;
    }
    else{
      fprintf(stderr, "unsupported congestion controler\n");
      argp_usage(state);
    }
    break;
  case 's':
    env.server_addr = arg;
    break;
  case 'S':
    env.send_log_file = arg;
    break;
  case 'R':
    env.recv_log_file = arg;
    break;
  case 'i':
    env.src_file = arg;
    break;
  case 'o':
    env.dst_file = arg;
    break;
  case 'f':
    env.fast_transfer = 1;
    break;
  case ARGP_KEY_ARG:
    argp_usage(state);
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

static int settcpcc(int fd, const char *tcp_ca)
{
	int err;
	err = setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, tcp_ca, strlen(tcp_ca));
	if (err == -1){
    printf("setsockopt(fd, TCP_CONGESTION) errno:%d\n",errno);
    return -1;
  }
	return 0;
}

int cmd_sd, data_sd, wfd = -1;
static int copy(char *src, char *dst);
static int initialize(char *server_ipaddr, char *server_port1, char *server_port2, 
  char *sent_log_file, char *recv_log_file);
static int release(void);
static int connect_to_server(char *server_ipaddr, char *server_port);

static int copy(char *src, char *dst){
  struct message m1;
  long position;
  position = 0;
  do{
    memset(&m1, 0, sizeof(m1));
    m1.opcode = READ;
    m1.offset = position;
    m1.count = BUF_SIZE;
    m1.name_len = strlen(src);
    strncpy(m1.name, src, m1.name_len);
    ifri_send(cmd_sd, &m1);
    ifri_receive(cmd_sd, &m1);
    /*write the data just received to the destination file*/
    m1.opcode = WRITE;
    m1.offset = position;
    m1.count  = m1.result;
    m1.name_len = strlen(dst);
    strncpy(m1.name, dst, m1.name_len);
    ifri_send(cmd_sd, &m1);
    ifri_receive(cmd_sd, &m1);
    position+=m1.result;
  } while(m1.result > 0);
  
  return(m1.result >= 0 ? OK: m1.result);
}

static int copy_2(char *src, char *dst){
  struct message m1;
  long position;
  char buffer[16384];
  int r;
  double recvbytes = 0;
  if( (wfd == -1) && (wfd = open(dst, O_CREAT | O_WRONLY | S_IRWXU )) < 0){
    fprintf(stderr, "do_write error when opening (%s), %d:%d\n", 
           dst, wfd, errno);
    return E_IO;
  }
  position = 0;
  memset(&m1, 0, sizeof(m1));
  m1.opcode = READ;
  m1.offset = position;
  m1.count = BUF_SIZE;
  m1.name_len = strlen(src);
  strncpy(m1.name, src, m1.name_len);
  m1.send_file_content = 1;
  ifri_send(cmd_sd, &m1);
  do{
    while((r = read(data_sd, buffer, 16384)) < 0 && (errno == EINTR));
    if(r < 0){
      fprintf(stderr, "Erreur lors de la réception de données"
               "%s (%d)\n", strerror(errno), errno);
      return -1;
    }
    r = write(wfd, buffer, r);
    recvbytes+=r;
    set_recv_data(recvbytes);
  }while(r>0);
  print_recv_log();
  ifri_receive(cmd_sd, &m1); 
  position+=m1.result; 
  return(m1.result >= 0 ? OK: m1.result);
}

static int initialize(char *server_ipaddr, char *server_port1, char *server_port2, 
  char *sent_log_file, char *recv_log_file){
  int sfd = open(sent_log_file, O_CREAT | O_RDWR);
  if(sfd < 0){
      fprintf(stderr, "Erreur d'ouverture du fichier (%s) (%s)\n"
       , sent_log_file, strerror(errno) );
      return -1;
   }
   
   int rfd = open(recv_log_file, O_CREAT | O_RDWR);
   if(rfd < 0){
      fprintf(stderr, "Erreur d'ouverture du fichier (%s) (%s)\n"
       , recv_log_file, strerror(errno) );
      return -1;
   }
     
  init_params(rfd,sfd,NULL,NULL,NULL);
  if((cmd_sd = connect_to_server(server_ipaddr, server_port1))<0){
    fprintf(stderr, "Erreur lors de la connexion au server au" 
           "port (%d) (%s)\n", cmd_sd, server_port1);
    return -1;
  }
  
  if((data_sd = connect_to_server(server_ipaddr, server_port2))<0){
    fprintf(stderr, "Erreur lors de la connexion au server au" 
           "port (%d) (%s)\n", cmd_sd, server_port2);
    return -1;
  }
  fprintf(stderr, "Connexion reussi au serveur (%d) (%d)(%s) \n",
         cmd_sd, data_sd, strerror(errno));
  
  // set congestion controler
  if(env.load_bpfcc)
    env.load_bpfcc();
  
  system(env.sysctl_cmd);
  settcpcc(cmd_sd,env.bpfcc);
  settcpcc(data_sd,env.bpfcc);
  return OK;
}

static void sig_handler(int sig)
{
  release();
  exit(-1);
}

int main(int argc, char * argv[]){
  /* Cleaner handling of Ctrl-C */
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  int err;
  /* Parse command line arguments */
  err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
  if (err)
    return err;
  char *server_addr = env.server_addr;
  char *port = env.port, *port2 = env.port2;
  char *send_log_file = env.send_log_file, *recv_log_file = env.recv_log_file;
  char *src_file = env.src_file, *dst_file = env.dst_file;
  int fast_transfer = env.fast_transfer; 

  initialize(server_addr, port, port2, send_log_file, recv_log_file);
  if(!fast_transfer)
    copy(src_file, dst_file);
  else
    copy_2(src_file, dst_file);
  release();
  return 0;
}

static int release(void){
  if(env.unload_bpfcc)
    env.unload_bpfcc();
  return close(cmd_sd||data_sd);
}

static int connect_to_server(char *server_ipaddr, char *server_port){
  struct sockaddr server_addr;
  int salen, err;
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  if(resolve_address(&server_addr, &salen, server_ipaddr, server_port, 
      AF_INET, SOCK_STREAM, IPPROTO_TCP)!= 0){
      fprintf(stderr, "Erreur de configuration de sockaddr\n");
      return -1;
  }
  if((err = connect(sd, &server_addr, salen))!=0){
    fprintf(stderr, "Erreur de connection au server %d\n", errno);
    return -1;
  }
  //err = fcntl(sd, F_SETFL, O_NONBLOCK );
  return sd;
}
