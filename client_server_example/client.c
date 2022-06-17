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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../lib/lib.h"

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
     
  init_params(sfd, rfd);
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
  return OK;
}

int main(int argc, char * argv[]){
  char c;
  char *server_addr = NULL;
  char *port = NULL, *port2 = NULL;
  char *send_log_file = NULL, *recv_log_file = NULL;
  char *src_file = NULL, *dst_file = NULL;
  int fast_transfer = 0; 
  while ((c = getopt(argc, argv, "s:p:P:S:R:hi:o:f")) != -1){
    switch(c){
      case 's':
        server_addr = malloc(strlen(optarg));
        strcpy(server_addr, optarg);
        break;
      case 'p':
        if(strlen(optarg) > 5 || !atoi(optarg)){
          fprintf(stderr, "port between 0-65535\n");
          return 0;
        }
        port = malloc(strlen(optarg));
        strcpy(port, optarg);
        break;
      case 'P':
        if(strlen(optarg) > 5 || !atoi(optarg)){
          fprintf(stderr, "port between 0-65535\n");
          return 0;
        }
        port2 = malloc(strlen(optarg));
        strcpy(port2, optarg);
        break;
      case 'S':
        send_log_file = malloc(strlen(optarg));
        strcpy(send_log_file, optarg);
        break;
      case 'R':
        recv_log_file = malloc(strlen(optarg));
        strcpy(recv_log_file, optarg);
        break;
      case 'i':
        src_file = malloc(strlen(optarg));
        strcpy(src_file, optarg);
        break;
      case 'o':
        dst_file = malloc(strlen(optarg));
        strcpy(dst_file, optarg);
        break;
      case 'f':
        fast_transfer = 1;
        break;
      case 'h':
        fprintf(stderr, "USAGE: %s -s server_addr -p server_port -P port2"
                " -S send_log_file_path -R recv_log_file_path" 
                "-i src_file -o dst_file [-f]\n", argv[0]);
        return 0; 
    }
  }
  if(!server_addr || !port || !port2 || !send_log_file || !recv_log_file ||
     !src_file || !dst_file){
    fprintf(stderr, "USAGE: %s -s server_addr -p server_port -P port2"
                " -S send_log_file_path -R recv_log_file_path" 
                "-i src_file -o dst_file [-f] \n", argv[0]);
    return 0; 
  }
  
  initialize(server_addr, port, port2, send_log_file, recv_log_file);
  if(!fast_transfer)
    copy(src_file, dst_file);
  else
    copy_2(src_file, dst_file);
  release();
  return 0;
}

static int release(void){
  return close(cmd_sd) || close(data_sd);
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
  char* tcp_ca = "bpf_reno";
    if(setsockopt(sd,IPPROTO_TCP,TCP_CONGESTION,tcp_ca,strlen(tcp_ca)) != 0){
      fprintf(stderr, "Un petit problème lors de l'application du bpf_reno %d\n", errno);
      return -1;
    }
    
  err = fcntl(sd, F_SETFL, O_NONBLOCK );
  return sd;
}
