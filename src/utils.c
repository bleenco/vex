/**
 * @license
 * Copyright Bleenco GmbH. All Rights Reserved.
 *
 * Author: Jan Kuri <jan@bleenco.com>
 *
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file at https://github.com/bleenco/vex
 */

#include "utils.h"
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
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
  double total = 0;
  char id[100];
  char buf[BUF_SIZE], size[10];

  strcpy(id, args->id);

  while ((n = recv(args->source_sock, buf, BUF_SIZE, 0)) > 0) {
    total += n;
    send(args->destination_sock, buf, n, 0);
  }

  shutdown(args->destination_sock, SHUT_RDWR);
  close(args->destination_sock);

  shutdown(args->source_sock, SHUT_RDWR);
  close(args->source_sock);

  char *in_out = (args->data_in == 1) ? "in" : "out";
  char log[200];
  sprintf(log, "(%s) data transfered %s - %s\n", id, in_out, readable_fs(total, size));
  print_info(log);

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
  args_in.data_in = 1;
  strcpy(args_in.id, args->id);
  args_out.source_sock = args->destination_sock;
  args_out.destination_sock = args->source_sock;
  args_out.data_in = 0;
  strcpy(args_out.id, args->id);

  if (pthread_create(&tid[0], NULL, transfer, (void *)&args_in) < 0) {
    printf("error creating thread.\n");
  }

  if (pthread_create(&tid[1], NULL, transfer, (void *)&args_out) < 0) {
    printf("error creating thread.\n");
  }

  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);

  return 0;
}

conn_info_t
get_conn_info(struct sockaddr_in *addr)
{
  conn_info_t conn_info;
  char str[100];
  int port;

  inet_ntop(AF_INET, (struct sockaddr_in *)&addr->sin_addr, str, INET_ADDRSTRLEN);
  port = ntohs(addr->sin_port);
  strcpy(conn_info.hostname, str);
  conn_info.port = port;

  return conn_info;
}

void
print_info(char *msg)
{
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char strtime[64];
  strftime(strtime, sizeof(strtime), "%c", tm);
  printf("[%s] %s", strtime, msg);
}

char
*extract_text(char *text, char *pattern1, char *pattern2)
{
  char *target = NULL;
  char *start, *end;

  if ((start = strstr(text, pattern1))) {
    start += strlen(pattern1);
    if ((end = strstr(start, pattern2))) {
      target = (char *)malloc(end - start + 1);
      memcpy(target, start, end - start);
      target[end - start] = '\0';
    }
  }

  return target;
}

char
*readable_fs(double size, char *buf) {
  int i = 0;
  const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  while (size > 1024) {
    size /= 1024;
    i++;
  }
  sprintf(buf, "%.*f%s", i, size, units[i]);
  return buf;
}

ssize_t
readn(int fd, void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
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
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR) {
        nwritten = 0;
      } else {
        return -1;
      }
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}

ssize_t
readline(int sockd, void *vptr, size_t maxlen)
{
  ssize_t n, rc;
  char    c, *buffer;

  buffer = vptr;

  for (n = 1; n < maxlen; n++) {
    if ( (rc = read(sockd, &c, 1)) == 1 ) {
      *buffer++ = c;
      if ( c == '\n' ) {
  	    break;
      }
    } else if (rc == 0) {
      if ( n == 1 ) {
	      return 0;
      } else {
  	    break;
      }
    } else {
      if (errno == EINTR) {
  	    continue;
        exit(1);
      }
    }
  }

  *buffer = 0;
  return n;
}


ssize_t
writeline(int sockd, const void *vptr, size_t n)
{
  size_t      nleft;
  ssize_t     nwritten;
  const char *buffer;

  buffer = vptr;
  nleft  = n;

  while (nleft > 0) {
  	if ((nwritten = write(sockd, buffer, nleft)) <= 0) {
  	  if (errno == EINTR) {
  		  nwritten = 0;
      } else {
  	    exit(1);
  	  }
    }
  	nleft  -= nwritten;
  	buffer += nwritten;
  }

  return n;
}


int trim(char *buffer) {
  int n = strlen(buffer) - 1;

  while (!isalnum(buffer[n]) && n >= 0) {
    buffer[n--] = '\0';
  }
  return 0;
}


int
str_upper(char *buffer)
{
  while (*buffer) {
   *buffer = toupper(*buffer);
    ++buffer;
  }
  return 0;
}

void
clean_url(char *buffer)
{
  char asciinum[3] = {0};
  int i = 0, c;

  while (buffer[i]) {
	  if ( buffer[i] == '+' ) {
	    buffer[i] = ' ';
	  } else if (buffer[i] == '%') {
	    asciinum[0] = buffer[i+1];
	    asciinum[1] = buffer[i+2];
	    buffer[i] = strtol(asciinum, NULL, 16);
	    c = i+1;
	    do {
		    buffer[c] = buffer[c+2];
	    } while (buffer[2+(c++)]);
	  }
	++i;
  }
}

void
remove_substring(char *buffer, const char *removebuf)
{
  while ((buffer = strstr(buffer, removebuf)) != NULL) {
    memmove(buffer, buffer + strlen(removebuf), 1 + strlen((buffer + strlen(removebuf))));
  }
}
