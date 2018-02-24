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
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>

typedef struct {
  int remote_conn;
  int local_conn;
  int in_use;
} tunnel_t;

typedef struct {
  char id[100];
  char remote_host[100];
  unsigned short remote_port;
  char local_host[100];
  unsigned short local_port;
  tunnel_t connections[10];
} client_t;

int create_connection(char *remote_host, int remote_port);
void create_tunnel(int index, char *remote_host, int remote_port, char *local_host, int local_port);
void parse_args(int argc, char **argv);
void usage();

client_t client;

int
main(int argc, char **argv)
{
  parse_args(argc, argv);

  signal(SIGPIPE, SIG_IGN);

  int remote_sock;
  ssize_t n;
  char buf[BUF_SIZE];

  remote_sock = create_connection(client.remote_host, client.remote_port);

  char log[200];
  sprintf(log, "successfully connected to %s:%hu, initializing ...\n", client.remote_host, client.remote_port);
  print_info(log);

  char *reqid = (!strncmp(client.id, "", strlen(client.id))) ? "random" : client.id;
  send_http_client_request(remote_sock, reqid);

  while ((n = read(remote_sock, buf, BUF_SIZE)) > 0) {
    if (strstr(buf, "[vex_data]")) {
      char delimit[] = " ";
      strtok(buf, delimit);
      char *id = strtok(NULL, delimit);
      char *domain = strtok(NULL, delimit);
      strcpy(client.id, id);
      sprintf(log, "client successfully initialized with id: %s\n", client.id);
      print_info(log);
      char port[10] = "";
      char host[150];
      if (client.remote_port != 80) {
        sprintf(port, ":%hu", client.remote_port);
      }
      sprintf(host, "%s%s%s", client.id, ".", domain);
      sprintf(log, "url: http://%s%s secure url: https://%s%s\n", host, port, host, port);
      print_info(log);
      memset(buf, 0, BUF_SIZE);
    } else if (strstr(buf, "[vex_tunnel]")) {
      create_tunnel(0, client.remote_host, client.remote_port, client.local_host, client.local_port);
      memset(buf, 0, BUF_SIZE);
    }
  }

  printf("Bye!\n");
  return 0;
}

int
create_connection(char *remote_host, int remote_port)
{
  int sock;

  struct sockaddr_in server_addr;
  struct hostent *server;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    exit(1);
  }

  if ((server = gethostbyname(remote_host)) == NULL) {
    exit(1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  server_addr.sin_port = htons(remote_port);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    exit(1);
  }

  return sock;
}

void
create_tunnel(int index, char *remote_host, int remote_port, char *local_host, int local_port)
{
  char log[100];
  sprintf(log, "establishing new tunnel ...\n");
  print_info(log);

  tunnel_t tunnel;
  pthread_t thread_id;
  struct transfer_args args;

  int remote_conn = create_connection(remote_host, remote_port);
  int local_conn = create_connection(local_host, local_port);

  tunnel.remote_conn = remote_conn;
  tunnel.local_conn = local_conn;
  client.connections[index] = tunnel;

  args.source_sock = remote_conn;
  args.destination_sock = local_conn;
  strcpy(args.id, client.id);

  if (pthread_create(&thread_id, NULL, handle_transfer, (void *)&args) < 0) {
    printf("error creating thread.\n");
  }
}

void
parse_args(int argc, char **argv)
{
  static struct option long_opt[] = {
    {"help",          no_argument,       NULL, 'h'},
    {"remote_host",   required_argument, NULL, 'r'},
    {"remote_port",   optional_argument, NULL, 'b'},
    {"local_port",    required_argument, NULL, 'p'},
    {"local_host",    required_argument, NULL, 'l'},
    {"subdomain_id",  optional_argument, NULL, 'i'},
    {NULL,            0,                 NULL, 0  }
  };

  strcpy(client.local_host, "localhost");
  client.local_port = 0;
  strcpy(client.remote_host, "");
  client.remote_port = 1234;
  int opt;

  while ((opt = getopt_long(argc, argv, "r:b:p:l:i:", long_opt, NULL)) != -1) {
    switch (opt) {
      case 'r':
        strcpy(client.remote_host, optarg);
      break;
      case 'b':
        if (1 != sscanf(optarg, "%hu", &client.remote_port)) {
          printf("bad port number: %s", optarg);
          usage();
        }
      break;
      case 'p':
        if (1 != sscanf(optarg, "%hu", &client.local_port)) {
          printf("bad port number: %s", optarg);
          usage();
        }
      break;
      case 'l':
        strcpy(client.local_host, optarg);
      break;
      case 'i':
        strcpy(client.id, optarg);
      break;
      default:
        usage();
    }
  }

  if (!strcmp(client.remote_host, "") || client.local_port == 0 || client.remote_port == 0) {
    usage();
  }
}

void
usage()
{
  printf("vex\n\n"
         "Usage: vex [-i <subdomain_id> -b <remote_port> -r <remote_host> -p <local_port> -l <local_host> [-h]]\n"
         "\n"
         "Options:\n"
         "\n"
         " -h                        Show this help message.\n"
         " -i <subdomain_id>         Send request to use this subdomain. Randomly generated if not provided.\n"
         " -b <remote_port>          Connect to this remote port. Default: 1234\n"
         " -r <remote_host>          Connect to this remote host. Mandatory.\n"
         " -p <local_port>           Port where your service is running. Mandatory.\n"
         " -p <local_host>           Host where your service is running. Default: localhost.\n"
         "\n"
         "Copyright (c) 2017 Bleenco GmbH http://bleenco.com \n");
  exit(1);
}
