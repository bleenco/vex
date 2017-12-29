#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define BUF_SIZE 256000
#define h_addr h_addr_list[0]

struct conn_args {
  int sock;
  struct sockaddr_in sockaddr;
};

struct transfer_args {
  int source_sock;
  int destination_sock;
  char id[20];
  int data_in;
  int verbose;
};

typedef struct {
  char hostname[100];
  int port;
} conn_info_t;

char *rand_string(int len);
void *transfer(void *arguments);
void *handle_transfer(void *arguments);
conn_info_t get_conn_info(struct sockaddr_in *addr);
void print_info(char *msg);
char *extract_text(char *text, char *pattern1, char *pattern2);
char *readable_fs(double size, char *buf);
