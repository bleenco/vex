#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h> /* getopt */
#endif

#define DEFAULT_BIND_HOST "127.0.0.1"
#define DEFAULT_BIND_PORT 5000
#define DEFAULT_IDLE_TIMEOUT (60 * 1000)

static void parse_opts(server_config *cf, int argc, char **argv);
static void usage(void);

static const char *progname = __FILE__; /* Reset in main(). */

int main(int argc, char **argv)
{
  server_config config;
  int err;

  progname = argv[0];
  memset(&config, 0, sizeof(config));
  config.bind_host = DEFAULT_BIND_HOST;
  config.bind_port = DEFAULT_BIND_PORT;
  config.idle_timeout = DEFAULT_IDLE_TIMEOUT;
  parse_opts(&config, argc, argv);

  err = server_run(&config, uv_default_loop());
  if (err)
  {
    exit(1);
  }

  return 0;
}

const char *_getprogname(void)
{
  return progname;
}

static void parse_opts(server_config *cf, int argc, char **argv)
{
  int opt;

  while (-1 != (opt = getopt(argc, argv, "H:hp:")))
  {
    switch (opt)
    {
    case 'H':
      cf->bind_host = optarg;
      break;

    case 'p':
      if (1 != sscanf(optarg, "%hu", &cf->bind_port))
      {
        pr_err("bad port number: %s", optarg);
        usage();
      }
      break;

    default:
      usage();
    }
  }
}

static void usage(void)
{
  printf("Usage:\n"
         "\n"
         "  %s [-b <address> [-h] [-p <port>]\n"
         "\n"
         "Options:\n"
         "\n"
         "  -b <hostname|address>  Bind to this address or hostname.\n"
         "                         Default: \"127.0.0.1\"\n"
         "  -h                     Show this help message.\n"
         "  -p <port>              Bind to this port number.  Default: 1080\n"
         "",
         progname);
  exit(1);
}
