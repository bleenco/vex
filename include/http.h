/**
 * @license
 * Copyright Bleenco GmbH. All Rights Reserved.
 *
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file at https://github.com/bleenco/vex
 */

#ifndef _VEX_HTTP_H_
#define _VEX_HTTP_H_

#define MAX_REQ_LINE 1024

enum req_method { GET, POST, HEAD, PUT, DELETE, OPTIONS, CONNECT, UNKNOWN };
enum req_type   { SIMPLE, FULL };

struct request {
  enum req_method method;
  enum req_type type;
  char buf[4096];
  char *referer;
  char *useragent;
  char *resource;
  char *host;
  char *subdomain;
  char *domain;
  char *upgrade;
  char *content_type;
  int  *content_length;
  int  status;
};

typedef struct {
  char *extension;
  char *mime_type;
} mime_map_t;

int parse_http_header(char *buffer, struct request *req, int line_num);
int get_request(int conn, struct request *req);
void init_request(struct request *req);
void free_request(struct request *req);
int http_send(int conn, struct request *req, int resource);
int check_resource(struct request *req);
int http_error(int conn, struct request *req, char *msg);
int send_http_client_request(int conn, char *id);
char *get_mime_type(char *filename);

#endif // _VEX_HTTP_H_
