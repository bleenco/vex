#include "utils.h"
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
  int status; // -1 - uninitialized, 0 - ready, 1 - in use, 2 - done
} tunnel_t;

typedef struct {
  char *id;
  int main_conn;
  char buf[BUF_SIZE];
  int ready;
  int next_tunnel_id;
  tunnel_t *connections[];
} client_t;

typedef struct {
  int local_port;
  int server_sock;
  int tunnels_num;
  conn_info_t conn_info;
  client_t *clients[];
} server_t;

int create_socket(int port);
void *handle_client(void *client_sock);
void init_client(int client_sock, client_t *client);
void init_tunnel(int remote_conn, client_t *client, conn_info_t conn_info);

server_t server;
pthread_mutex_t mutex_lock;

int
main(int argc, char *argv[])
{
  server.local_port = 5000;
  server.tunnels_num = 10;

  int client_sock;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);

  server.server_sock = create_socket(server.local_port);
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
  }

  return 0;
}


void
*handle_client(void *arguments)
{
  struct conn_args *args = arguments;
  int sock = args->sock;
  struct sockaddr_in sockaddr = args->sockaddr;
  char buf[BUF_SIZE], *host, *subdomain;
  client_t *client = NULL;
  ssize_t n;
  conn_info_t conn_info = get_conn_info(&sockaddr);

  if ((client = server.clients[0]) != NULL && client->ready == 1) {
    client->ready = 0;
    int id = client->next_tunnel_id;
    client->connections[id]->tunnel_conn = sock;

    send(client->connections[id]->tunnel_conn, client->buf, strlen(client->buf), 0);

    struct transfer_args args;
    pthread_t tid;

    args.source_sock = client->connections[id]->http_conn;
    args.destination_sock = client->connections[id]->tunnel_conn;

    if (pthread_create(&tid, NULL, handle_transfer, (void *)&args) < 0) {
      printf("error creating thread.\n");
    }
    pthread_join(tid, NULL);

    return 0;
  }

  if ((n = recv(sock, buf, BUF_SIZE, 0)) > 0) {
    if (strncmp(buf, "[vex_client_init]", 17) == 0) { // new client connected
      client_t *client = malloc(sizeof(client_t));
      init_client(sock, client);
      char log[100];
      sprintf(log, "client (%s) initialized\n", client->id);
      print_info(log);
    } else {
      host = extract_text(buf, "Host:", "\r\n");
      subdomain = extract_text(host, " ", ".");

      client = server.clients[0];
      init_tunnel(sock, client, conn_info);
      strcpy(client->buf, buf);
      client->ready = 1;

      char *initbuf = "[vex_tunnel]";
      send(client->main_conn, initbuf, strlen(initbuf), 0);
    }
  }

  return 0;
}

void
init_client(int client_sock, client_t *client)
{
  char *id = rand_string(8);
  char resp[100] = "[vex_data]: ";
  strncat(resp, id, strlen(id));
  send(client_sock, resp, strlen(resp), 0);

  client->id = id;
  client->main_conn = client_sock;
  client->ready = 0;
  client->next_tunnel_id = -1;
  server.clients[0] = client;
}

void
init_tunnel(int remote_conn, client_t *client, conn_info_t conn_info)
{
  client->next_tunnel_id++;
  tunnel_t *tunnel = malloc(sizeof(tunnel_t));
  tunnel->http_conn = remote_conn;
  client->connections[client->next_tunnel_id] = tunnel;
  char log[200];
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

  return server_sock;
}
