#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 1024

struct transfer_args {
  int source_sock;
  int destination_sock;
};

char *rand_string(int len);
void *transfer(void *arguments);
void *handle_transfer(void *arguments);
ssize_t readn(int socket, void *vptr, size_t n);
ssize_t writen(int socket, const void *vptr, size_t n);
ssize_t readlineb(int socket, void *buf, size_t maxlen);
