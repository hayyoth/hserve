#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "socket.h"

int server_socket(const char *host, const char *port) {
  struct addrinfo hints;
  struct addrinfo *list = NULL;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;

  int rc = getaddrinfo(host, port, &hints, &list);
  if (rc != 0) {
    fprintf(stderr, "hserve: %s\n", gai_strerror(rc));
    return -1;
  }

  int fd = -1;

  for (struct addrinfo *ai = list; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1)
      continue;

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
        &yes, sizeof(yes)) == -1 ||
        bind(fd, ai->ai_addr, ai->ai_addrlen) == -1 ||
        listen(fd, 64) == -1) {
      close(fd);
      fd = -1;
      continue;
    }

    break;
  }

  freeaddrinfo(list);

  if (fd == -1)
    perror("hserve: socket/bind/listen");

  return fd;
}
