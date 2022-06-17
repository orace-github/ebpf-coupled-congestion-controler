#include <string.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "message.h"

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
static char logger[40][50];
static int logger_iter = 0; 

static int log_data(int fd, double nbytes, int *nMB, double *lasttime);

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
  log_data(recv_log, nb_bytes_recv, &recv_dataMB, &last_recv_time);
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
  log_data(sent_log, nb_bytes_sent, &sent_dataMB, &last_sent_time);
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

void init_params(int recv_log_, int sent_log_){
  start_time = gettime_ms();
  nb_bytes_sent = 0;
  nb_bytes_recv = 0;
  sent_dataMB = 0;
  recv_dataMB = 0;
  last_sent_time = 0;
  last_recv_time = 0;
  recv_log = recv_log_;
  sent_log = sent_log_;
}

static int log_data(int fd, double nbytes, int *nMB, double *lasttime){
  int ret = -1;
  if((int)(nbytes/10000000) > *nMB){ 
    (*nMB)++;
    double delta_time = (gettime_ms() - start_time)/1000;
    double bw = 10*1000/(delta_time - *lasttime);
    ret = dprintf(fd, "%.6f %.6f\n", delta_time/1000, bw);
    //ret = snprintf(logger[logger_iter++], 50, "%.6f %.6f\n", delta_time/1000, bw);
    *lasttime = delta_time;
  }
  return ret;
}

int set_recv_data(int recv_data){
  nb_bytes_recv = recv_data;
  return log_data(recv_log, nb_bytes_recv, &recv_dataMB, &last_recv_time);
}

int print_recv_log(void){
  for(int i = 0; i < logger_iter; i++)
    dprintf(recv_log, "%s", logger[i]);
}

int print_sent_log(void){
  for(int i = 0; i < logger_iter; i++)
    dprintf(sent_log, "%s", logger[i]);
}
