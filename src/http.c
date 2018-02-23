/**
 * @license
 * Copyright Bleenco GmbH. All Rights Reserved.
 *
 * Author: Jan Kuri <jan@bleenco.com>
 *
 * Use of this source code is governed by an MIT-style license that can be
 * found in the LICENSE file at https://github.com/bleenco/vex
 */

#include "http.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>

static char server_root[1000] = "/Users/jan/Dev/bleenco/abstruse.bleenco.io/dist";

mime_map_t meme_types [] = {
  {".css", "text/css"},
  {".gif", "image/gif"},
  {".htm", "text/html"},
  {".html", "text/html"},
  {".jpeg", "image/jpeg"},
  {".jpg", "image/jpeg"},
  {".ico", "image/x-icon"},
  {".js", "application/javascript"},
  {".pdf", "application/pdf"},
  {".mp4", "video/mp4"},
  {".png", "image/png"},
  {".svg", "image/svg+xml"},
  {".xml", "text/xml"},
  {".ttf", "application/font-sfnt"},
  {NULL, NULL},
};

char *default_mime_type = "text/plain";

int
parse_http_header(char *buffer, struct request *req, int line_num)
{
  char      *temp;
  char      *endptr;
  int        len;

  if (line_num == 1) {
    char *method = extract_text(buffer, "", " /");

    if (strcmp(method, "GET") == 0) {
      req->method = GET;
      buffer += 3;
    } else if (strcmp(method, "POST") == 0) {
      req->method = POST;
      buffer += 4;
    } else if (strcmp(method, "HEAD") == 0) {
      req->method = HEAD;
      buffer += 4;
    } else if (strcmp(method, "PUT") == 0) {
      req->method = PUT;
      buffer += 3;
    } else if (strcmp(method, "DELETE") == 0) {
      req->method = DELETE;
      buffer += 6;
    } else if (strcmp(method, "OPTIONS") == 0) {
      req->method = OPTIONS;
      buffer += 7;
    } else if (strcmp(method, "CONNECT") == 0) {
      req->method = CONNECT;
      buffer += 7;
    } else {
      req->method = UNKNOWN;
      req->status = 501;
      return -1;
    }

    while (*buffer && isspace(*buffer)) {
      buffer++;
    }

    endptr = strchr(buffer, ' ');
    if (endptr == NULL) {
      len = strlen(buffer);
    } else {
      len = endptr - buffer;
    }
    if (len == 0) {
      req->status = 400;
      return -1;
    }

    if (len == 1) {
      req->resource = calloc(12, sizeof(char));
      strncpy(req->resource, "/index.html", 11);
    } else {
      req->resource = calloc(len + 1, sizeof(char));
      strncpy(req->resource, buffer, len);
    }

    if (strstr(buffer, "HTTP/")) {
      req->type = FULL;
    } else {
      req->type = SIMPLE;
    }

    return 0;
  }

  endptr = strchr(buffer, ':');
  if (endptr == NULL) {
    return -1;
  }

  temp = calloc((endptr - buffer) + 1, sizeof(char));
  strncpy(temp, buffer, (endptr - buffer));
  str_upper(temp);

  buffer = endptr + 1;
  while (*buffer && isspace(*buffer)) {
    ++buffer;
  }
  if (*buffer == '\0') {
    return 0;
  }

  buffer = endptr + 1;
  while (*buffer && isspace(*buffer)) {
	  ++buffer;
    if (*buffer == '\0') {
     	return 0;
    }

    if (!strcmp(temp, "USER-AGENT")) {
      req->useragent = malloc(strlen(buffer) + 1);
      strcpy(req->useragent, buffer);
    } else if (!strcmp(temp, "REFERER")) {
      req->referer = malloc(strlen(buffer) + 1);
      strcpy(req->referer, buffer);
    } else if (!strcmp(temp, "HOST")) {
      req->host = malloc(strlen(buffer) + 1);
      strcpy(req->host, buffer);

      char *subdomain = extract_text(buffer, "", req->domain);
      subdomain[strlen(subdomain) - 1] = '\0';
      req->subdomain = malloc(strlen(subdomain) + 1);
      strcpy(req->subdomain, subdomain);
    } else if (!strcmp(temp, "UPGRADE")) {
      req->upgrade = malloc(strlen(buffer) + 1);
      strcpy(req->upgrade, buffer);
    }
  }

  const char *mime = get_mime_type(req->resource);
  req->content_type = malloc(strlen(mime) + 1);
  strcpy(req->content_type, mime);

  free(temp);
  return 0;
}

int
get_request(int conn, struct request *req) {
  char   buffer[MAX_REQ_LINE] = {0};
  int    rval;
  fd_set fds;
  struct timeval tv;

  tv.tv_sec  = 5;
  tv.tv_usec = 0;

  int line_num = 0;
  do {
    FD_ZERO(&fds);
	  FD_SET(conn, &fds);

	  rval = select(conn + 1, &fds, NULL, NULL, &tv);

  	if (rval < 0) {
	    exit(1);
  	} else if (rval == 0) {
	    return -1;
	  } else {
      readline(conn, buffer, MAX_REQ_LINE - 1);
      strcat(req->buf, buffer);

      line_num++;
	    trim(buffer);

	    if (buffer[0] == '\0') {
		    break;
      }

	    if (parse_http_header(buffer, req, line_num)) {
		    break;
      }
	  }
  } while (req->type != SIMPLE);

  return 0;
}

void
init_request(struct request *req) {
  req->useragent      = NULL;
  req->referer        = NULL;
  req->resource       = NULL;
  req->host           = NULL;
  req->subdomain      = NULL;
  req->domain         = NULL;
  req->upgrade        = NULL;
  req->content_type   = NULL;
  req->content_length = 0;
  req->method         = UNKNOWN;
  req->status         = 200;
}

void
free_request(struct request *req) {
  if (req->useragent) {
	  free(req->useragent);
  }
  if (req->referer) {
	  free(req->referer);
  }
  if (req->resource) {
	  free(req->resource);
  }
  if (req->host) {
    free(req->host);
  }
  if (req->subdomain) {
    free(req->subdomain);
  }
  if (req->upgrade) {
    free(req->upgrade);
  }
  if (req->content_type) {
    free(req->content_type);
  }
}

int
http_send(int conn, struct request *req, int resource)
{
  char buffer[1024];
  int i;

  sprintf(buffer, "HTTP/1.1 %d OK\r\n", req->status);
  writeline(conn, buffer, strlen(buffer));
  writeline(conn, "Server: vex 0.1\r\n", 24);

  sprintf(buffer, "Content-Type: %s\r\n", req->content_type);
  writeline(conn, buffer, strlen(buffer));
  writeline(conn, "\r\n", 2);

  while ((i = readn(resource, buffer, 1024)) > 0) {
    writen(conn, buffer, i);
  }

  return 0;
}

int
check_resource(struct request *req) {
  clean_url(req->resource);
  char resource_path[1000];
  strcpy(resource_path, server_root);
  strcat(resource_path, req->resource);
  return open(resource_path, O_RDONLY);
}

int
http_error(int conn, struct request *req, char *msg) {
  char buffer[1024];

  sprintf(buffer, "HTTP/1.1 %d Not Found\r\n", req->status);
  writeline(conn, buffer, strlen(buffer));
  writeline(conn, "Server: vex 0.1\r\n", 24);
  writeline(conn, "\r\n", 2);

  sprintf(buffer, "<html>\n"
                  "<head>\n"
                  "  <title>Server Error %d</title>\n"
                  "</head>\n\n", req->status);
  writeline(conn, buffer, strlen(buffer));
  sprintf(buffer, "<body>\n<h3>Error %d</h3>\n", req->status);
  writeline(conn, buffer, strlen(buffer));
  sprintf(buffer, "<p>%s.<p>\n</body>\n</html>\n", msg);
  writeline(conn, buffer, strlen(buffer));

  return 0;
}

int
send_http_client_request(int conn, char *id)
{
  char buffer[100];

  sprintf(buffer, "GET /vex-api/client-init/%s\r\n", id);
  writeline(conn, buffer, strlen(buffer));

  return 0;
}

char
*get_mime_type(char *filename)
{
  char *dot = strrchr(filename, '.');
  if (dot) {
    mime_map_t *map = meme_types;
    while(map->extension) {
      if(strcmp(map->extension, dot) == 0) {
        return map->mime_type;
      }
      map++;
    }
  }
  return default_mime_type;
}
