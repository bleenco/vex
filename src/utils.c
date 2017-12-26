#include "utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <errno.h>

char *
rand_string(int len)
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
transfer(int source_sock, int destination_sock)
{
  ssize_t n;
  char buf[BUF_SIZE];

  while ((n = recv(source_sock, buf, BUF_SIZE, 0)) > 0) {
    send(destination_sock, buf, n, 0);
  }

  if (n < 0) {
    exit(1);
  }

  shutdown(destination_sock, SHUT_RDWR);
  close(destination_sock);

  shutdown(source_sock, SHUT_RDWR);
  close(source_sock);
}

void
handle_transfer(int source_sock, int destination_sock)
{
  printf("%d %d\n", source_sock, destination_sock);

  if (fork() == 0) {
    transfer(source_sock, destination_sock);
    exit(0);
  }

  if (fork() == 0) {
    transfer(destination_sock, source_sock);
    exit(0);
  }
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
