#include <string.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "message.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../libcc/libcc.h"
#define SL sizeof(long)

static double start_time;
static double nb_bytes_sent;
static int nb_bytes_recv;
static int sent_dataMB;
static int recv_dataMB;
static double last_sent_time;
static double last_recv_time;
static int recv_log; 
static int sent_log; 
struct list* recv_list; 
struct list* send_list; 
static char logger[40][50];
static int logger_iter = 0;
struct list_head* recv_it;
struct list_head* send_it;
static int cmd_csd;
static int data_csd;
static int bandwidth;
struct bpfcc flags;

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

static void switch_bpfcc(void){
  if(!flags.bpf_cubic){
    load_bpf_cubic();
    fprintf(stdout,"Switching on bpf_cubic...");
    system("sysctl -w net.ipv4.tcp_congestion_control=bpf_cubic");
    settcpcc(cmd_csd,"bpf_cubic");
    settcpcc(data_csd,"bpf_cubic");
    fprintf(stdout,"[OK]\n");
    flags.bpf_cubic = 1;
  }
  else if(!flags.bpf_vegas){
    load_bpf_vegas();
    fprintf(stdout,"Switching on bpf_vegas...");
    system("sysctl -w net.ipv4.tcp_congestion_control=bpf_vegas");
    settcpcc(cmd_csd,"bpf_vegas");
    settcpcc(data_csd,"bpf_vegas");
    fprintf(stdout,"[OK]\n");
    flags.bpf_vegas = 1;
  }
  else if(!flags.bpf_reno){
    load_bpf_reno();
    fprintf(stdout,"Switching on bpf_reno...");
    system("sysctl -w net.ipv4.tcp_congestion_control=bpf_reno");
    settcpcc(cmd_csd,"bpf_reno");
    settcpcc(data_csd,"bpf_reno");
    fprintf(stdout,"[OK]\n");
    flags.bpf_reno = 1;
  }
}

static void bandwidth_analyzer(void* __unused /* unused */){
  //fprintf(stdout,"Bandwidth analyzer\n");
  struct list_head* it;
  if(recv_it){
    //fprintf(stdout,"recv list\n");
    list_for_each(it,recv_it){
      //fprintf(stdout,"rate slice:@%p\n",it);
      rate_t* slice_1 = list_entry(it,rate_t,head);
      //printf("bw:%f tm:%f\n",slice_1->bw,slice_1->tm);
      if(slice_1->head.next){
        //fprintf(stdout,"Perform computation\n");
        rate_t* slice_2 = list_entry(slice_1->head.next,rate_t,head);
        if(slice_2->bw < slice_1->bw){
          // change controller congestion
          switch_bpfcc();
          recv_it = slice_1->head.next;
          break;
        }
      }
    }  
  }
  
  // probe send_log file
  if(send_it){
    //fprintf(stdout,"send list\n");
    list_for_each(it,send_it){
      rate_t* slice_1 = list_entry(it,rate_t,head);
      if(slice_1->head.next){
        //fprintf(stdout,"Perform computation\n");
        rate_t* slice_2 = list_entry(slice_1->head.next,rate_t,head);
        if(slice_2->bw < slice_1->bw){
          // change controller congestion
          switch_bpfcc();
          send_it = slice_1->head.next;
          break;
        }
      }
    }
  }
}

int log_data(int strm,int fd, double nbytes, int *nMB, double *lasttime);

int ifri_receive(int from, struct message *m){
  int ret;
  char *buff = malloc(sizeof(struct message));
  //while((ret = recv(from, buff, sizeof(struct message), 0)) < 0 && errno ==EINTR);
  ret = recv(from, buff, sizeof(struct message), 0);
  if(ret < 0)
    return (E_IO);
  memset(m, 0, sizeof(*m));
  memcpy(&m->source,   buff, SL);
  memcpy(&m->dest,     buff+SL, SL);
  memcpy(&m->opcode,   buff+SL+SL, SL);
  memcpy(&m->count,    buff+SL+SL+SL, SL);
  memcpy(&m->offset,   buff+SL+SL+SL+SL, SL);
  memcpy(&m->result,   buff+SL+SL+SL+SL+SL, SL);
  memcpy(&m->name_len, buff+SL+SL+SL+SL+SL+SL, SL);
  memcpy(&m->name,     buff+SL+SL+SL+SL+SL+SL+SL, m->name_len);
  memcpy(&m->data,     buff+SL+SL+SL+SL+SL+SL+SL+ m->name_len, m->count);
  memcpy(&m->send_file_content, buff+SL+SL+SL+SL+SL+SL+SL+ m->name_len+m->count, sizeof(int));
  nb_bytes_recv += ret;
  log_data(1,recv_log, nb_bytes_recv, &recv_dataMB, &last_recv_time);
  free(buff);
  return OK;
}

int ifri_send(int to, struct message *m){
  char *buff = malloc(sizeof(struct message));
  memcpy(buff, &m->source, SL);
  memcpy(buff+SL, &m->dest, SL);
  memcpy(buff+SL+SL, &m->opcode, SL);
  memcpy(buff+SL+SL+SL, &m->count, SL);
  memcpy(buff+SL+SL+SL+SL, &m->offset, SL);
  memcpy(buff+SL+SL+SL+SL+SL, &m->result, SL);
  memcpy(buff+SL+SL+SL+SL+SL+SL, &m->name_len, SL);
  memcpy(buff+SL+SL+SL+SL+SL+SL+SL, m->name, m->name_len);
  memcpy(buff+SL+SL+SL+SL+SL+SL+SL+m->name_len, m->data, m->count);
  memcpy(buff+SL+SL+SL+SL+SL+SL+SL+m->name_len+m->count, &m->send_file_content, sizeof(int));
  int ret = send(to, buff, sizeof(struct message),0);
  if(ret < 0)
    return E_IO;
  nb_bytes_sent += m->count;
  log_data(0,sent_log, nb_bytes_sent, &sent_dataMB, &last_sent_time);
  free(buff);
  return OK;
}

int resolve_address(struct sockaddr *sa, socklen_t *salen, const char *host, 
  const char *port, int family, int type, int proto){
  struct addrinfo hints, *res;
  int err;
  memset(&hints,0, sizeof(hints));
  hints.ai_family = family;
  hints.ai_socktype = type;
  hints.ai_protocol = proto;
  hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
  if((err = getaddrinfo(host, port, &hints, &res)) !=0 || res == NULL){
     fprintf(stderr, "failed to resolve address :%s:%s\n", host, port);
     return -1;
  }
  memcpy(sa, res->ai_addr, res->ai_addrlen);
  *salen = res->ai_addrlen;
  freeaddrinfo(res);
  return 0;
}

double gettime_ms(void){
  struct timeval tv1;
  gettimeofday(&tv1, NULL);
  return (tv1.tv_sec * 1000000 + tv1.tv_usec);
}

void init_params(int recv_log_, int sent_log_, int bandwidth_){
  //fprintf(stdout,"Initializing params\n");
  start_time = gettime_ms();
  nb_bytes_sent = 0;
  nb_bytes_recv = 0;
  sent_dataMB = 0;
  recv_dataMB = 0;
  last_sent_time = 0;
  last_recv_time = 0;
  recv_log = recv_log_;
  sent_log = sent_log_;
  bandwidth = bandwidth_;
  recv_list = malloc(sizeof(struct list));
  send_list = malloc(sizeof(struct list));
  init_list(recv_list);
  init_list(send_list);
  recv_it = &recv_list->head;
  send_it = &send_list->head;
}

void init_sockfd(int data_csd_, int cmd_csd_, struct bpfcc fl){
  //fprintf(stdout,"Init sockfd\n");
  data_csd = data_csd_;
  cmd_csd = cmd_csd_;
  flags = fl;
}

int log_data(int strm,int fd, double nbytes, int *nMB, double *lasttime){
  int ret = -1;
  if((int)(nbytes/10000000) > *nMB){
    //fprintf(stdout,"Log data...");
    (*nMB)++;
    double delta_time = (gettime_ms() - start_time)/1000;
    double bw = 10*1000/(delta_time - *lasttime);
    ret = dprintf(fd, "%.6f %.6f\n", delta_time/1000, bw);
    rate_t* rate = (rate_t*)malloc(sizeof(rate_t));
    if(rate){
      rate->bw = bw; rate->tm = delta_time/1000;
      //fprintf(stdout,"malloc passed\n");
    }
    //ret = snprintf(logger[logger_iter++], 50, "%.6f %.6f\n", delta_time/1000, bw);
    *lasttime = delta_time;
    if(strm == 0){
      list_insert(send_list,&rate->head);
      //fprintf(stdout,"New rate added\n");
    }else if(strm == 1){
      list_insert(recv_list,&rate->head);
      //fprintf(stdout,"New rate added\n");
    }else{
      //fprintf(stdout,"list failed\n");
    }
    if(bandwidth)
      bandwidth_analyzer(NULL);
    //fprintf(stdout,"[OK]\n");
  }
  return ret;
}

int set_recv_data(int recv_data){
  nb_bytes_recv = recv_data;
  return log_data(1,recv_log, nb_bytes_recv, &recv_dataMB, &last_recv_time);
}

int print_recv_log(void){
  for(int i = 0; i < logger_iter; i++)
    dprintf(recv_log, "%s", logger[i]);
}

int print_sent_log(void){
  for(int i = 0; i < logger_iter; i++)
    dprintf(sent_log, "%s", logger[i]);
}
