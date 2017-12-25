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

#define BUF_SIZE 1024

#define READ 0
#define WRITE 1

typedef enum {TRUE = 1, FALSE = 0} bool;

int create_socket(int port);
void handle_client(int client_sock, struct sockaddr_in client_addr);
void copy_data(int source_sock, int destination_sock);
int create_connection();

int server_sock, client_sock, remote_sock, remote_port = 0;
char *bind_addr, *remote_host, *cmd_in, *cmd_out;

int
main(int argc, char *argv[])
{
  int local_port = 10000, client_sock;
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);

  remote_host = "0.0.0.0";
  remote_port = 6500;

  server_sock = create_socket(local_port);

  while (1) {
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addrlen);
    handle_client(client_sock, client_addr);
    close(client_sock);
  }

  return 0;
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

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    exit(1);
  }

  if (listen(server_sock, 128) < 0)
  {
    exit(1);
  }

  return server_sock;
}

void
handle_client(int client_sock, struct sockaddr_in client_addr)
{
  if ((remote_sock = create_connection()) < 0) {
    goto cleanup;
  }

  if (fork() == 0) {
    copy_data(client_sock, remote_sock);
    exit(0);
  }

  if (fork() == 0) {
    copy_data(remote_sock, client_sock);
    exit(0);
  }

cleanup:
  close(remote_sock);
  close(client_sock);
}

void
copy_data(int source_sock, int destination_sock)
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

int
create_connection()
{
  struct sockaddr_in server_addr;
  struct hostent *server;
  int sock;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return 1;
  }

  if ((server = gethostbyname(remote_host)) == NULL) {
    return 1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  server_addr.sin_port = htons(remote_port);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    return 1;
  }

  return sock;
}
