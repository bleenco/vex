/**
 * @license
 * Copyright Bleenco GmbH. All Rights Reserved.
 *
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file at https://github.com/bleenco/vex
 */

#ifndef _VEX_UTILS_H
#define _VEX_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define BUF_SIZE 256000
#define MAXLEN 1024
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

typedef struct {
  const char *extension;
  const char *mime_type;
} mime_map;

char *rand_string(int len);
void *transfer(void *arguments);
void *handle_transfer(void *arguments);
conn_info_t get_conn_info(struct sockaddr_in *addr);
void print_info(char *msg);
char *extract_text(char *text, char *pattern1, char *pattern2);
char *readable_fs(double size, char *buf);
ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readline(int sockd, void *vptr, size_t maxlen);
ssize_t writeline(int sockd, const void *vptr, size_t n);
int str_upper(char *buffer);
int trim(char *buffer);
int str_upper(char *buffer);
void clean_url(char *buffer);
void remove_substring(char *buffer, const char *removebuf);

#endif // _VEX_UTILS_H
