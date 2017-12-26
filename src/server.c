#include <utils.h>
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
  int remote_conn;
  int local_conn;
  int in_use;
} tunnel_t;

typedef struct {
  char *id;
  tunnel_t connections[10];
} client_t;

int create_socket(int port);
void *handle_client(void *client_sock);
int init_client(int client_sock, client_t *client);
int get_unused_conn_id(tunnel_t connections[]);

int server_sock, client_sock, remote_sock, remote_port = 0;
char *bind_addr, *remote_host;

client_t client;

int
main(int argc, char *argv[])
{
  int local_port = 5000, client_sock;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);

  remote_host = "0.0.0.0";
  remote_port = 6500;

  server_sock = create_socket(local_port);
  pthread_t thread_id;

  while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addrlen))) {
    if (pthread_create(&thread_id, NULL, handle_client, (void *)&client_sock) < 0) {
      printf("could not create thread.\n");
      return 1;
    }
  }

  return 0;
}

void
*handle_client(void *client_sock)
{
  int sock = *(int *)client_sock;
  printf("client connected, id: %s\n", client.id);

  if (client.id) {
    int id;
    pthread_t thread_id;
    struct transfer_args args;

    if ((id = get_unused_conn_id(client.connections)) >= 0) {
      client.connections[id].in_use = 1;
      client.connections[id].local_conn = sock;
      args.source_sock = client.connections[id].local_conn;
      args.destination_sock = client.connections[id].remote_conn;
      if (pthread_create(&thread_id, NULL, &handle_transfer, (void *)&args) < 0) {
        printf("error creating thread.\n");
      }
    }

    printf("ID: %d\n", id);

    // if (pthread_join(thread_id, NULL)) {
    //   printf("error joining thread.\n");
    //   return 0;
    // }
  } else {
    ssize_t n;
    char buf[BUF_SIZE];

    while ((n = recv(sock, buf, BUF_SIZE, 0)) > 0) {
      if (strstr(buf, "[vex_client_init]")) {
        int remote_conn, i = 0;
        struct sockaddr_in remote_addr;
        socklen_t remote_addrlen = sizeof(remote_addr);
        int server_sock = init_client(sock, &client);

        while ((remote_conn = accept(server_sock, (struct sockaddr *)&remote_addr, &remote_addrlen))) {
          tunnel_t tunnel;
          tunnel.remote_conn = remote_conn;
          tunnel.in_use = 0;
          client.connections[i] = tunnel;
          printf("new tunnel established %d.\n", i);
          i++;
        }
      }
    }
  }

  printf("handle client done.\n");
  return 0;
}

int
init_client(int client_sock, client_t *client)
{
  char *id = rand_string(8);
  int server_sock = create_socket(0), maxconn = 10;
  char client_server_port[7], max_connections[3];

  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);

  if (getsockname(server_sock, (struct sockaddr *)&sin, &len) == -1) {
    exit(1);
  }

  sprintf(client_server_port, "%d", ntohs(sin.sin_port));
  sprintf(max_connections, "%d", maxconn);

  char resp[100] = "[vex_data]: ";
  strncat(resp, id, strlen(id));
  strncat(resp, " ", 1);
  strncat(resp, client_server_port, strlen(client_server_port));
  strncat(resp, " ", 1);
  strncat(resp, max_connections, strlen(max_connections));

  writen(client_sock, resp, strlen(resp));

  client->id = id;

  return server_sock;
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

  return server_sock;
}

int
get_unused_conn_id(tunnel_t connections[])
{
  int len = 10;
  for (int i = 0; i < len; i++) {
    if (connections[i].in_use == 0) {
      return i;
    }
  }
  return -1;
}
