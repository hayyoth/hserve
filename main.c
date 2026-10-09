#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "server.h"

static void usage(const char *name) {
  fprintf(stderr, "Usage: %s [-p port] [-h host] [path]\n", name);
}

static int valid_port(const char *s) {
  if (!s || !*s) return 0;
  char *end;
  errno = 0;
  long n = strtol(s, &end, 10);
  return errno == 0 && *end == '\0' && n >= 1 && n <= 65535;
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      usage(argv[0]);
      return 0;
    }
  }

  ServerConfig config = {
    .host = "localhost",
    .port = "8080",
    .path = "."
  };

  int opt;
  while ((opt = getopt(argc, argv, "p:h:")) != -1) {
    switch (opt) {
      case 'p':
        if (!valid_port(optarg)) {
          usage(argv[0]);
          return 2;
        }
        config.port = optarg;
        break;
      case 'h':
        config.host = optarg;
        break;
      default:
        usage(argv[0]);
        return 2;
    }
  }

  if (optind < argc) {
    config.path = argv[optind++];
    if (optind < argc) {
      usage(argv[0]);
      return 2;
    }
  }

  int rc = server_run(&config);

  return rc == -1 ? 1 : 0;
}
