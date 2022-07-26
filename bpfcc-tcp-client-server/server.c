#include <string.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <getopt.h>
#include "../libcc/libcc.h"
#include "message.h"
#include <argp.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

static int cmd_csd, data_csd, cmd_sd, data_sd, rfd, wfd = -1;
static int do_create(struct message *m1, struct message *m2);
static int do_read(struct message *m1, struct message *m2);
static int do_write(struct message *m1, struct message *m2);
static int do_delete(struct message *m1, struct message *m2);
static int bind_socket(char *server_ipaddr, char *server_port);
static int initialize(char *server_ipaddr, char *server_port1, char *server_port2,
   char *sent_log_file, char *recv_log_file);
static int release(void);


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
    {"sndfile", 'S', "SNDLOG-FILE", 0, "snd log file"},
    {"rcvfile", 'R', "RCVLOG-FILE", 0, "rcv log file"},
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

void bandwidth_analyzer(void* __unused /* unused */){

}

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

static void sig_handler(int sig)
{
  release();
  exit(-1);
}

int main(int argc, char *argv[]){
  struct message m1, m2; /*incomming and outgoing message*/
  int r;   /* result code*/
  char c;
  /* Cleaner handling of Ctrl-C */
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  /* Parse command line arguments */
  int err;
  err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
  if (err)
    return err;
  char *server_addr = env.server_addr;
  char *port = env.port, *port2 = env.port2;
  char *send_log_file = env.send_log_file, *recv_log_file = env.recv_log_file;
  
  memset(&m1,0, sizeof(m1)); 
  memset(&m2,0, sizeof(m2));              
  initialize(server_addr, port, port2, send_log_file, recv_log_file);
  while(TRUE){
    struct sockaddr client_addr;
    int clen;
    if((cmd_csd = accept(cmd_sd, &client_addr, &clen)) < 0){
      fprintf(stderr, "Un petit problème lors du accept %d\n", errno);
      return -1;
    }
    if((data_csd = accept(data_sd, &client_addr, &clen)) < 0){
      fprintf(stderr, "Un petit problème lors du accept %d\n", errno);
      return -1;
    }
    
    pid_t pid = fork();
    if(pid == 0){
      close(cmd_sd);
      close(data_sd);
      rfd = -1; wfd = -1;
      // set congestion controler
      if(env.load_bpfcc)
        env.load_bpfcc();
      system(env.sysctl_cmd);
      settcpcc(cmd_csd,env.bpfcc);
      settcpcc(data_csd,env.bpfcc);
      fprintf(stderr, "get connection from (%d) (%d)\n", cmd_csd, data_csd);
      while(TRUE){           /*server runs forever*/
        ifri_receive(cmd_csd, &m1); /* block waiting for a message*/
        switch(m1.opcode){
        /*case CREATE: 
          r = do_create(&m1, &m2);
          break;*/
          case READ:
            r = do_read(&m1, &m2);
            break;
          case WRITE:
            r = do_write(&m1, &m2);
            break;
          /*case DELETE:
            r = do_delete(&m1, &m2);
            break;*/
          default:
            r = E_BAD_OPCODE;
        }
        m2.result = r;  /* return result to client */
        if(r > 0)
          m2.count = r;
        ifri_send(cmd_csd, &m2); /* send reply*/
      }
      break;
    }else{
      close(cmd_csd);
      close(data_csd);
    }
  }
  release();
}

static int do_create(struct message *m1, struct message *m2){
  return OK;
}

static int do_read(struct message *m1, struct message *m2){
  int fd, r = OK;
  double bytessent = 0;
  char buffer[16384];
  if((rfd == -1) && (rfd = open(m1->name, O_RDONLY | S_IRUSR | S_IWUSR ))<0){
    fprintf(stderr, "error when opening file %d %d %s\n",fd, errno, m1->name);
    return E_IO;
  }
  if(!m1->send_file_content){
    lseek(rfd, m1->offset, SEEK_SET);
    r = read(rfd, m2->data, m1->count);
    if(r < 0){
      fprintf(stderr, "error when reading file %d %d\n",fd, errno);
      return  E_IO;
    }
  }else{
    while((r =read(rfd, buffer, 16384))> 0){
      write(data_csd, buffer, r);
      bytessent+=r;
    }
  }
  return r;
}

static int do_write(struct message *m1, struct message *m2){
  int r;
  if( (wfd == -1) && (wfd = open(m1->name, O_CREAT | O_WRONLY | S_IRWXU )) < 0){
    fprintf(stderr, "do_write error when opening (%s), %d:%d\n", m1->name, wfd, errno);
    return E_IO;
  }
  lseek(wfd, m1->offset, SEEK_SET);
  r = write(wfd, m1->data, m1->count);
  if(r < 0)
     return  E_IO;
  return r;
}

static int do_delete(struct message *m1, struct message *m2){
  return OK;
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
   cmd_sd = bind_socket(server_ipaddr, server_port1);
   data_sd = bind_socket(server_ipaddr, server_port2);
   fprintf(stderr, "Connexion binded (%d) (%d)\n", cmd_sd, data_sd);   
}

static int release(void){
  if(env.unload_bpfcc)
    env.unload_bpfcc();
  return (close(data_sd) || close(cmd_sd) || close(data_csd) 
             || close(cmd_csd) || close(rfd) || close(wfd));
}

static int bind_socket(char *server_ipaddr, char *server_port){
  struct sockaddr server_addr;
  int salen;
  int sd = socket(AF_INET, SOCK_STREAM, 0);
   if(resolve_address(&server_addr, &salen, server_ipaddr, server_port, AF_INET, 
      SOCK_STREAM, IPPROTO_TCP)!= 0){
      fprintf(stderr, "Erreur de configuration de sockaddr\n");
      return -1;
   }
    if(bind(sd, &server_addr, salen)!=0){
      fprintf(stderr, "Un petit problème lors du bind: %d\n", errno);
      return -1;
    }
    if(listen(sd, 10)!=0){
      fprintf(stderr, "Un petit problème lors du listen %d\n", errno);
      return -1;
    }
    return sd;
}
