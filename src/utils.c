#include "utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
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

  while ((n = recv(args->source_sock, buf, BUF_SIZE, 0)) > 0) {
    send(args->destination_sock, buf, n, 0);
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
  struct transfer_args args_in, args_out;
  pthread_t thread_in, thread_out;

  args_in.source_sock = args->source_sock;
  args_in.destination_sock = args->destination_sock;
  args_out.source_sock = args->destination_sock;
  args_out.destination_sock = args->source_sock;

  if (pthread_create(&thread_in, NULL, &transfer, (void *)&args_in) < 0) {
    printf("error creating thread.\n");
  }

  if (pthread_create(&thread_out, NULL, &transfer, (void *)&args_out) < 0) {
    printf("error creating thread.\n");
  }

  if (pthread_join(thread_in, NULL) || pthread_join(thread_out, NULL)) {
    printf("error joining thread.\n");
  }

  return 0;
}

ssize_t
readn(int socket, void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = read(socket, ptr, nleft)) < 0) {
      if (errno == EINTR) {
        nread = 0;
      } else {
        return -1;
      }
    } else if (nread == 0) {
      break;
    }
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);
}

ssize_t
writen(int socket, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(socket, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR) {
        nwritten = 0;
      } else {
        return -1;
      }
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}

ssize_t
readlineb(int socket, void *vptr, size_t maxlen)
{
  ssize_t n, rc;
  char c, *ptr;

  ptr = vptr;
  for (n = 1; n < maxlen; n++) {
    again:
      if ((rc = read(socket, &c, 1)) == 1) {
        *ptr++ = c;
        if (c == '\n') {
          break;
        } else if (rc == 0) {
          *ptr = 0;
          return n - 1;
        } else {
          if (errno == EINTR) {
            goto again;
          }
          return -1;
        }
      }
  }

  *ptr = 0;
  return n;
}
