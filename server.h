#ifndef HSERVE_SERVER_H
#define HSERVE_SERVER_H

typedef struct {
  const char *host;
  const char *port;
  const char *path;
} ServerConfig;

int server_run(const ServerConfig *config);

#endif
