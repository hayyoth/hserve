#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "socket.h"
#include "file.h"
#include "worker.h"

int server_run(const ServerConfig *cfg) {
  int root_fd = file_open_root(cfg->path);
  if (root_fd == -1) return -1;

  int listen_fd = server_socket(cfg->host, cfg->port);
  if (listen_fd == -1) {
    close(root_fd);
    return -1;
  }

  // Ignore SIGPIPE when client disconnects abruptly
  struct sigaction sa = { .sa_handler = SIG_IGN };
  sigaction(SIGPIPE, &sa, NULL);

  printf("Serving %s at %s:%s\n", cfg->path, cfg->host, cfg->port);
  fflush(stdout);

  int workers = 0;
  struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };

  for (;;) {
    pfd.revents = 0;
    int rc = poll(&pfd, 1, 1000); // 1-second timeout to periodically call worker_reap

    if (rc == -1) {
      if (errno == EINTR) continue;
      perror("hserve: poll");
      break;
    }

    worker_reap(&workers);

    if (rc == 0 || !(pfd.revents & POLLIN)) continue;

    worker_spawn(listen_fd, root_fd, &workers);
  }

  close(listen_fd);
  close(root_fd);
  return -1;
}
