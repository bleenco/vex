/**
 * @license
 * Copyright Bleenco GmbH. All Rights Reserved.
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
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

typedef struct {
  int http_conn;
  int tunnel_conn;
  int status;
} tunnel_t;

typedef struct {
  char id[50];
  int main_conn;
  char buf[BUF_SIZE];
  int ready;
  int next_tunnel_id;
  int last_id;
  int is_disconnected;
  tunnel_t *connections[];
} client_t;

typedef struct {
  unsigned short local_port;
  int server_sock;
  char domain[100];
  int num_clients;
  int active_client_id;
  conn_info_t conn_info;
  client_t *clients[];
} server_t;

struct handle_args {
  int sock;
  conn_info_t conn_info;
};

struct watch_args {
  int sock;
  client_t *client;
};

int create_socket(int port);
void *handle_client(void *client_sock);
void init_client(int client_sock, client_t *client, char *reqid);
void init_tunnel(int remote_conn, client_t *client, conn_info_t conn_info);
void *watch_client(void *arguments);
void *read_conn(void *arguments);
void parse_args(int argc, char **argv);
int get_client_id(char *subdomain);
void print_help();

server_t server;
pthread_mutex_t mutex_lock;

int
main(int argc, char **argv)
{
  parse_args(argc, argv);

  int client_sock;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);

  server.server_sock = create_socket(server.local_port);
  server.num_clients = 0;
  pthread_mutex_init(&mutex_lock, NULL);

  while ((client_sock = accept(server.server_sock, (struct sockaddr *)&client_addr, &addrlen))) {
    pthread_t thread_id;

    struct conn_args args;
    args.sock = client_sock;
    args.sockaddr = client_addr;

    if (pthread_create(&thread_id, NULL, handle_client, (void *)&args) < 0) {
      printf("could not create thread.\n");
      return 1;
    }
    pthread_detach(thread_id);
  }

  return 0;
}


void
*handle_client(void *arguments)
{
  struct conn_args *args = arguments;
  int sock = args->sock;
  struct sockaddr_in sockaddr = args->sockaddr;
  client_t *client = NULL;
  conn_info_t conn_info = get_conn_info(&sockaddr);
  struct handle_args hargs;
  pthread_t htid;

  hargs.sock = sock;
  hargs.conn_info = conn_info;

  if ((client = server.clients[server.active_client_id]) != NULL && client->ready == 1) {
    pthread_mutex_unlock(&mutex_lock);
    client->ready = 0;
    int id = client->last_id;
    client->connections[id]->tunnel_conn = sock;
    send(client->connections[id]->tunnel_conn, client->buf, strlen(client->buf), 0);

    struct transfer_args args;
    pthread_t tid;

    args.source_sock = client->connections[id]->http_conn;
    args.destination_sock = client->connections[id]->tunnel_conn;
    strcpy(args.id, client->id);

    if (pthread_create(&tid, NULL, handle_transfer, (void *)&args) < 0) {
      printf("error creating thread.\n");
    }
    pthread_join(tid, NULL);

    client->connections[id]->http_conn = -1;
    client->connections[id]->tunnel_conn = -1;
    client->connections[id]->status = 0;
  } else {
    if (pthread_create(&htid, NULL, read_conn, (void *)&hargs) < 0) {
      printf("error creating thread.\n");
    }
    pthread_join(htid, NULL);
  }

  return 0;
}

void
*watch_client(void *arguments)
{
  struct watch_args *args = arguments;
  int main_sock = args->sock;
  client_t *client = args->client;
  ssize_t n;
  char buf[BUF_SIZE];

  while (1) {
    n = recv(main_sock, buf, BUF_SIZE, 0);
    if (n < 1) {
      break;
    }
  }

  shutdown(main_sock, SHUT_RDWR);
  close(main_sock);

  int id = get_client_id(client->id);
  if (id != -1) {
    server.clients[id]->is_disconnected = 1;
  }

  char log[100];
  sprintf(log, "(%s) client disconnected\n", client->id);
  print_info(log);

  return 0;
}

void
*read_conn(void *arguments)
{
  struct handle_args *args = arguments;
  int sock = args->sock;
  conn_info_t conn_info = args->conn_info;
  int client_id;
  client_t *client;

  struct request req;
  int resource = 0;

  init_request(&req);
  req.domain = server.domain;

  if (get_request(sock, &req) < 0) {
    return 0;
  }

  if (req.subdomain != NULL) {
    client_id = get_client_id(req.subdomain);
    if (client_id == -1) {
      req.status = 404;
      http_error(sock, &req, "ID Not Found");
      shutdown(sock, SHUT_RDWR);
      close(sock);
      return 0;
    }

    pthread_mutex_lock(&mutex_lock);
    client = server.clients[client_id];
    server.active_client_id = client_id;
    init_tunnel(sock, client, conn_info);
    strcpy(client->buf, req.buf);

    char *initbuf = "[vex_tunnel]";
    send(client->main_conn, initbuf, strlen(initbuf), 0);
    client->ready = 1;
  } else {
    if (strstr(req.resource, "/vex-api/client-init/") != NULL) {
      char reqid[100];
      client_t client;

      strcpy(reqid, req.resource);
      remove_substring(reqid, "/vex-api/client-init/");

      init_client(sock, &client, reqid);
      char log[100];
      sprintf(log, "client (%s) initialized\n", client.id);
      print_info(log);

      pthread_t wtid;
      struct watch_args wargs;
      wargs.sock = client.main_conn;
      wargs.client = &client;

      if (pthread_create(&wtid, NULL, watch_client, (void *)&wargs) < 0) {
        printf("error creating thread.\n");
      }
      pthread_join(wtid, NULL);
    } else {
      if (req.status == 200) {
        if ((resource = check_resource(&req)) < 0) {
          if (errno == EACCES) {
            req.status = 401;
          } else {
            req.status = 404;
          }
        }

        if ((resource > 0) && req.status == 200) {
          http_send(sock, &req, resource);
        } else {
          http_error(sock, &req, "Not found.");
        }

        if (resource > 0) {
          if (close(resource) < 0) {
            printf("error!\n");
          }
        }

        shutdown(sock, SHUT_RDWR);
        close(sock);
      }
    }
  }

  free_request(&req);
  return 0;
}

void
init_client(int client_sock, client_t *client, char *reqid)
{
  char id[50];
  if (!strncmp(reqid, "random", strlen(reqid))) {
    strcpy(id, rand_string(8));
  } else {
    strcpy(id, reqid);
  }

  strcpy(client->id, id);
  client->main_conn = client_sock;
  client->ready = 0;
  client->next_tunnel_id = -1;
  client->is_disconnected = 0;

  int num = server.num_clients;

  for (int i = 0; i < num; i++) {
    if (!strncmp(server.clients[i]->id, id, strlen(id)) || server.clients[i]->is_disconnected == 1) {
      num = i;
      break;
    }
  }

  printf("%s %d\n", client->id, num);

  server.clients[num] = client;
  if (num == server.num_clients) {
    server.num_clients++;
  }

  char resp[100] = "[vex_data]: ";
  strncat(resp, id, strlen(id));
  strncat(resp, " ", 1);
  strncat(resp, server.domain, strlen(server.domain));
  strncat(resp, " ", 1);
  send(client_sock, resp, strlen(resp), 0);
}

void
init_tunnel(int remote_conn, client_t *client, conn_info_t conn_info)
{
  if (client->is_disconnected == 1) {
    return;
  }

  int fid = -1, id;
  for (int i = 0; i < client->next_tunnel_id; i++) {
    tunnel_t *t = client->connections[i];
    if (t->http_conn == -1 && t->tunnel_conn == -1) {
      fid = i;
      break;
    }
  }

  if (fid != -1) {
    id = fid;
    client->connections[id]->http_conn = remote_conn;
    client->connections[id]->tunnel_conn = -1;
  } else {
    client->next_tunnel_id++;
    id = client->next_tunnel_id;
    tunnel_t *tunnel = malloc(sizeof(tunnel_t));
    tunnel->http_conn = remote_conn;
    tunnel->tunnel_conn = -1;
    client->connections[id] = tunnel;
  }

  client->last_id = id;

  char log[BUF_SIZE];
  sprintf(log, "(%s) new tunnel established - %s:%d <> %s:%d\n",
    client->id, server.conn_info.hostname, server.conn_info.port, conn_info.hostname, conn_info.port);
  print_info(log);
}

int
create_socket(int port)
{
  int server_sock, optval = 1;
  struct sockaddr_in server_addr;

  if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    exit(1);
  }

  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    exit(1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    exit(1);
  }

  if (listen(server_sock, 128) < 0) {
    exit(1);
  }

  conn_info_t conn_info = get_conn_info(&server_addr);
  server.conn_info = conn_info;

  char log[BUF_SIZE];
  sprintf(log, "server listening on %s:%d ...\n", conn_info.hostname, conn_info.port);
  print_info(log);
  sprintf(log, "domain configured to generate URLs at *.%s\n", server.domain);
  print_info(log);

  return server_sock;
}

int
get_client_id(char *subdomain)
{
  int num = server.num_clients;
  for (int i = 0; i < num; i++) {
    if (!strcmp(server.clients[i]->id, subdomain)) {
      return i;
    }
  }
  return -1;
}

void
parse_args(int argc, char **argv)
{
  strcpy(server.domain, "");
  server.local_port = 1234;
  int opt;

  while ((opt = getopt(argc, argv, "p:d:h")) != -1) {
    switch (opt) {
      case 'p':
        if (1 != sscanf(optarg, "%hu", &server.local_port)) {
          printf("bad port number: %s", optarg);
          print_help();
        }
      break;
      case 'd':
        strcpy(server.domain, optarg);
      break;
      case 'h':
        print_help();
      break;
      default:
        print_help();
    }
  }

  if (!strcmp(server.domain, "")) {
    print_help();
  }
}

void
print_help()
{
  printf("vex-server\n\n"
         "Usage: vex-server [-p <port> [-h] -d <domain>]\n"
         "\n"
         "Options:\n"
         "\n"
         " -h                Show this help message.\n"
         " -p <port>         Bind to this port number. Default: 1234\n"
         " -d <domain>       Use this domain when generating URLs.\n"
         "\n"
         "Copyright (c) 2017 Bleenco GmbH http://bleenco.com \n");
  exit(1);
}
