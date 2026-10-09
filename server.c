#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"
#include "socket.h"
#include "http.h"

#define MAX_WORKERS 32

static void usage(const char *name) {
  fprintf(stderr, "Usage: %s [-p port] [-h host] [path]\n", name);
}

static int valid_port(const char *s) {
  if (!s || !*s)
    return 0;

  char *end;
  errno = 0;
  long n = strtol(s, &end, 10);

  return errno == 0 && *end == '\0' && n >= 1 && n <= 65535;
}

int server_parse_args(int argc, char **argv, ServerConfig *cfg) {
  cfg->host = "localhost";
  cfg->port = "8080";
  cfg->path = ".";

  int path_set = 0;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      usage(argv[0]);
      exit(0);
    } else if (!strcmp(argv[i], "-p")) {
      if (++i >= argc || !valid_port(argv[i])) {
        usage(argv[0]);
        return -1;
      }
      cfg->port = argv[i];
    } else if (!strcmp(argv[i], "-h")) {
      if (++i >= argc) {
        usage(argv[0]);
        return -1;
      }
      cfg->host = argv[i];
    } else if (argv[i][0] == '-') {
      usage(argv[0]);
      return -1;
    } else if (!path_set) {
      cfg->path = argv[i];
      path_set = 1;
    } else {
      usage(argv[0]);
      return -1;
    }
  }

  return 0;
}

static void reap_children(int *workers) {
  for (;;) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);

    if (pid > 0) {
      if (*workers > 0)
        (*workers)--;
    } else {
      break;
    }
  }
}

int server_run(const ServerConfig *cfg) {
  int root_fd = open(cfg->path, O_RDONLY);
  if (root_fd == -1) {
    perror("hserve: open root");
    return -1;
  }

  struct stat st;
  if (fstat(root_fd, &st) == -1 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "hserve: path is not a directory\n");
    close(root_fd);
    return -1;
  }

  int listen_fd = server_socket(cfg->host, cfg->port);
  if (listen_fd == -1) {
    close(root_fd);
    return -1;
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPIPE, &sa, NULL);

  printf("Serving %s at %s:%s\n", cfg->path, cfg->host, cfg->port);
  fflush(stdout);

  int workers = 0;
  struct pollfd pfd = { listen_fd, POLLIN, 0 };

  for (;;) {
    pfd.revents = 0;
    int rc = poll(&pfd, 1, 1000);

    if (rc == -1 && errno != EINTR) {
      perror("hserve: poll");
      break;
    }

    reap_children(&workers);

    if (rc <= 0 || !(pfd.revents & POLLIN))
      continue;

    int client_fd = accept(listen_fd, NULL, NULL);

    if (client_fd == -1) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        continue;

      perror("hserve: accept");
      continue;
    }

    if (workers >= MAX_WORKERS) {
      close(client_fd);
      continue;
    }

    pid_t pid = fork();

    if (pid == -1) {
      perror("hserve: fork");
      close(client_fd);
      continue;
    }

    if (pid == 0) {
      close(listen_fd);
      http_handle(client_fd, root_fd);
      close(client_fd);
      close(root_fd);
      _exit(0);
    }

    workers++;
    close(client_fd);
  }

  close(listen_fd);
  close(root_fd);
  return -1;
}
