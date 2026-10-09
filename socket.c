#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "socket.h"

int server_socket(const char *host, const char *port) {
  struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_NUMERICSERV };
  struct addrinfo *res;
  
  int rv = getaddrinfo(host, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "hserve: %s\n", gai_strerror(rv));
    return -1;
  }

  int fd = -1, yes = 1;
  for (struct addrinfo *p = res; p; p = p->ai_next) {
    if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1 ||
        bind(fd, p->ai_addr, p->ai_addrlen) == -1 ||
        listen(fd, 64) == -1) {
      close(fd);
      fd = -1;
      continue;
    }
    break;
  }

  freeaddrinfo(res);
  if (fd == -1) perror("hserve: socket/bind/listen");
  return fd;
}
