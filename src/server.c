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
  tunnel_t *connections[];
} client_t;

typedef struct {
  int local_port;
  int server_sock;
  int tunnels_num;
  client_t *clients[];
} server_t;

int create_socket(int port);
void *handle_client(void *client_sock);
void init_client(int client_sock, client_t *client);
void init_tunnel(int remote_conn, client_t *client, int i);
int get_free_tunnel_id(client_t *client);
int get_dead_tunnel_id(client_t *client);

server_t server;

int
main(int argc, char *argv[])
{
  server.local_port = 5000;
  server.tunnels_num = 10;

  int client_sock;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);

  server.server_sock = create_socket(server.local_port);

  while ((client_sock = accept(server.server_sock, (struct sockaddr *)&client_addr, &addrlen))) {
    pthread_t thread_id;
    conn_info_t conn_info = get_conn_info(&client_addr);
    char log[100];
    sprintf(log, "client connected from %s:%d\n", conn_info.hostname, conn_info.port);
    print_info(log);

    if (pthread_create(&thread_id, NULL, handle_client, (void *)&client_sock) < 0) {
      printf("could not create thread.\n");
      return 1;
    }
    pthread_detach(thread_id);
  }

  return 0;
}

int inited = 0;

void
*handle_client(void *client_sock)
{
  int sock = *(int *)client_sock;
  char buf[BUF_SIZE], *host, *subdomain;
  client_t *client = NULL;
  ssize_t n;

  if (inited == 1) {
    client = server.clients[0];
    int id = 0;
    client->connections[id]->tunnel_conn = sock;

    struct transfer_args args;
    args.source_sock = client->connections[id]->tunnel_conn;
    args.destination_sock = client->connections[id]->http_conn;

    pthread_t tid;
    if (pthread_create(&tid, NULL, &handle_transfer, (void *)&args) < 0) {
      printf("error creating thread.\n");
    }
    pthread_detach(tid);

    send(client->connections[id]->tunnel_conn, client->buf, strlen(client->buf), 0);
  } else {
    if ((n = recv(sock, buf, BUF_SIZE, 0)) > 0) {
      if (strncmp(buf, "[vex_client_init]", 17) == 0) { // new client connected
        client_t *client = malloc(sizeof(client_t));
        init_client(sock, client);
        char log[100];
        sprintf(log, "client (%s) initialized\n", client->id);
        print_info(log);
      } else if (strstr(buf, "HTTP") != NULL) {
        printf("DA!\n");
        host = extract_text(buf, "Host:", "\r\n");
        subdomain = extract_text(host, " ", ".");

        client = server.clients[0];
        int id = 0;
        strcpy(client->buf, buf);
        client->connections[id]->http_conn = sock;

        char *initbuf = "[vex_tunnel]";
        send(client->main_conn, initbuf, strlen(initbuf), 0);
        inited = 1;
      } else {
        printf("TO TI JE ELSE\n");
      }
    }
  }



  // while ((n = recv(sock, buf, BUF_SIZE, 0)) > 0) {
  //   if (strncmp(buf, "[vex_client_init]", 17) == 0) { // new client connected
  //     client_t *client = malloc(sizeof(client_t));
  //     int remote_conn;
  //     struct sockaddr_in remote_addr;
  //     socklen_t remote_addrlen = sizeof(remote_addr);
  //     init_client(sock, client);
  //     char log[100];
  //     sprintf(log, "client (%s) initialized\n", client->id);
  //     print_info(log);
  //     client_conn = sock;
  //
  //     // while ((remote_conn = accept(server_sock, (struct sockaddr *)&remote_addr, &remote_addrlen))) {
  //     //   if (client != NULL) {
  //     //     int i = get_dead_tunnel_id(client);
  //     //     init_tunnel(remote_conn, client, i);
  //     //   }
  //     // }
  //   } else {
  //     if (client == NULL && tunnel == NULL && strstr(buf, "HTTP") != NULL) {
  //       // host = extract_text(buf, "Host:", "\r\n");
  //       // subdomain = extract_text(host, " ", ".");
  //       //
  //       // if (server.clients[0] == NULL) {
  //       //   return 0;
  //       // }
  //       //
  //       // client = server.clients[0];
  //       // int tunnel_id = get_free_tunnel_id(client);
  //       //
  //       // client->connections[tunnel_id]->local_conn = sock;
  //       // client->connections[tunnel_id]->status = 1;
  //       // tunnel = client->connections[tunnel_id];
  //       //
  //       // struct transfer_args args;
  //       // pthread_t tid;
  //       //
  //       // args.source_sock = tunnel->remote_conn;
  //       // args.destination_sock = tunnel->local_conn;
  //       //
  //       // if (pthread_create(&tid, NULL, &transfer, (void *)&args) < 0) {
  //       //   printf("error creating thread.\n");
  //       // }
  //     }
  //
  //     // write(tunnel->remote_conn, buf, n);
  //   }
  // }

  return 0;
}

void
init_client(int client_sock, client_t *client)
{
  char *id = rand_string(8);
  // int server_sock = create_socket(0), maxconn = 10;
  // char client_server_port[7], max_connections[3];
  //
  // struct sockaddr_in sin;
  // socklen_t len = sizeof(sin);
  //
  // if (getsockname(server_sock, (struct sockaddr *)&sin, &len) == -1) {
  //   exit(1);
  // }
  //
  // sprintf(client_server_port, "%d", ntohs(sin.sin_port));
  // sprintf(max_connections, "%d", maxconn);

  char resp[100] = "[vex_data]: ";
  strncat(resp, id, strlen(id));
  // strncat(resp, " ", 1);
  // strncat(resp, client_server_port, strlen(client_server_port));
  // strncat(resp, " ", 1);
  // strncat(resp, max_connections, strlen(max_connections));

  write(client_sock, resp, strlen(resp));

  client->id = id;
  client->main_conn = client_sock;

  for (int i = 0; i < server.tunnels_num; i++) {
    client->connections[i] = malloc(sizeof(tunnel_t));
    client->connections[i]->http_conn = -1;
    client->connections[i]->tunnel_conn = -1;
    client->connections[i]->status = -1;
  }

  server.clients[0] = client;
}

void
init_tunnel(int remote_conn, client_t *client, int i)
{
  tunnel_t *tunnel = malloc(sizeof(tunnel_t));
  tunnel->tunnel_conn = remote_conn;
  tunnel->status = 0;
  client->connections[i] = tunnel;
  char log[100];
  sprintf(log, "new tunnel established (%d)\n", i);
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

  return server_sock;
}

int
get_free_tunnel_id(client_t *client)
{
  for (int i = 0; i < server.tunnels_num; i++) {
    if (client->connections[i]->status == 0) {
      return i;
    }
  }
  return -1;
}

int
get_dead_tunnel_id(client_t *client)
{
  for (int i = 0; i < server.tunnels_num; i++) {
    if (client->connections[i] == NULL) {
      return i;
    }

    if (client->connections[i]->status == -1) {
      return i;
    }
  }
  return -1;
}
