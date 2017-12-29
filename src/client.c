#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
  int remote_conn;
  int local_conn;
  int in_use;
} tunnel_t;

typedef struct {
  char *id;
  char *remote_host;
  int remote_port;
  char *local_host;
  int local_port;
  tunnel_t connections[10];
} client_t;

int create_connection(char *remote_host, int remote_port);
void create_tunnel(int index, char *remote_host, int remote_port, char *local_host, int local_port);

client_t client;

int
main(int argc, char *argv[])
{
  int remote_sock;
  ssize_t n;

  client.remote_host = "33412dea.jan";
  client.remote_port = 5000;
  client.local_host = "localhost";
  client.local_port = 6510;

  remote_sock = create_connection(client.remote_host, client.remote_port);

  char buf[BUF_SIZE] = "[vex_client_init]";
  write(remote_sock, buf, strlen(buf));
  memset(buf, 0, BUF_SIZE);
  int port;

  while ((n = read(remote_sock, buf, BUF_SIZE)) > 0) {
    if (strstr(buf, "[vex_data]")) {
      char delimit[] = " ";
      strtok(buf, delimit);
      char *id = strtok(NULL, delimit);
      port = 5000;
      client.id = id;
      memset(buf, 0, BUF_SIZE);
    } else if (strstr(buf, "[vex_tunnel]")) {
      create_tunnel(0, client.remote_host, port, client.local_host, client.local_port);
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
  printf("creating tunnel...\n");
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

  if (pthread_create(&thread_id, NULL, handle_transfer, (void *)&args) < 0) {
    printf("error creating thread.\n");
  }
}
