#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <sys/types.h>          
#include <sys/socket.h>
#include <stdbool.h>
/*Definitions needed by clients and servers**/
#define TRUE            1
#define MAX_PATH      255 /*Maximum length of file name*/
#define BUF_SIZE     1024 /* How much data to transfer at once*/
#define SERVER_ADDR  "127.0.0.1" /* File server's network address*/
#define SERVER_PORT  "4443"

/* definitions of the allowed operations*/
#define CREATE 1         /*Create a new file*/
#define READ   2          /* read date from a file and return it*/
#define WRITE 3          /* write data to a file*/
#define DELETE 4         /* delete an existing file*/
#define OK 0             /*Operation performed correctly*/
#define E_BAD_OPCODE -1  /* Unknown operation requested*/
#define E_BAD_PARAM -2   /* error in a parameter*/
#define E_IO        -3   /* disk error or other I/O error*/

/*definition of the message format*/
struct message{
  long source;     /* sender identity*/
  long dest;       /* receiver's identity */
  long opcode;     /* requested operation */
  long count;      /* number of bytes to transfert */
  long offset ;    /* position if the file to start I/O*/
  long result;     /* result of the operation */
  long name_len;   /* name len */
  char name[MAX_PATH];  /*name of the file being operated on */
  char data[BUF_SIZE];   /* data to be read or written*/
  int send_file_content;
};

int ifri_receive(int from, struct message *m);
int ifri_send(int to, struct message *m);
int resolve_address(struct sockaddr *sa, socklen_t *salen, const char *host,
   const char *port, int family, int type, int proto);
double gettime_ms(void);
void init_params(int recv_log_, int sent_log_);
int set_recv_data(int recv_data);
int print_recv_log(void);
int print_sent_log(void);
#endif
