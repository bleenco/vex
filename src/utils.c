#include "utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

char
*rand_string(int len)
{
  int i, x;
  char *dst = malloc(sizeof(char) * len);

  srand((unsigned int)time(NULL));

  for (i = 0; i < len; i++) {
    do {
      x = rand() % 128 + 0;
    } while (x < 'a' || x > 'z');
    dst[i] = (char)x;
  }
  dst[len] = '\0';
  return dst;
}

void
*transfer(void *arguments)
{
  struct transfer_args *args = arguments;
  ssize_t n;
  char buf[BUF_SIZE];

  while ((n = read(args->source_sock, buf, BUF_SIZE)) > 0) {
    write(args->destination_sock, buf, n);
  }

  shutdown(args->destination_sock, SHUT_RDWR);
  close(args->destination_sock);

  shutdown(args->source_sock, SHUT_RDWR);
  close(args->source_sock);

  return 0;
}

void
*handle_transfer(void *arguments)
{
  struct transfer_args *args = arguments;
  struct transfer_args args_in;
  struct transfer_args args_out;
  pthread_t tid[2];

  args_in.source_sock = args->source_sock;
  args_in.destination_sock = args->destination_sock;
  args_out.source_sock = args->destination_sock;
  args_out.destination_sock = args->source_sock;

  if (pthread_create(&tid[0], NULL, &transfer, (void *)&args_in) < 0) {
    printf("error creating thread.\n");
  }

  if (pthread_create(&tid[1], NULL, &transfer, (void *)&args_out) < 0) {
    printf("error creating thread.\n");
  }

  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);

  return 0;
}
