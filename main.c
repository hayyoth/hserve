#include <stdio.h>
#include <stdlib.h>

#include "server.h"

int main(int argc, char **argv) {
  ServerConfig config;

  if (server_parse_args(argc, argv, &config) == -1)
    return 2;

  int rc = server_run(&config);
  return rc == -1 ? 1 : 0;
}
