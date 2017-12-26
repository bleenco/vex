#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 4096

struct transfer_args {
  int source_sock;
  int destination_sock;
};

char *rand_string(int len);
void *transfer(void *arguments);
void *handle_transfer(void *arguments);
