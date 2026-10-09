#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "worker.h"
#include "http.h"

#define MAX_WORKERS 32
static int workers = 0;

// Reap finished child processes (clean up zombies)
void worker_reap(void) {
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    if (workers > 0) 
      workers--;
  }
}

// Accept connection, check capacity, and fork a worker process
void worker_spawn(int listen_fd, int root_fd) {
  int client_fd = accept(listen_fd, NULL, NULL);
  if (client_fd == -1) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return;
    perror("hserve: accept");
    return;
  }

  // Over capacity, silently reject connection
  if (workers >= MAX_WORKERS) {
    close(client_fd);
    return;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("hserve: fork");
    close(client_fd);
    return;
  }

  if (pid == 0) { 
    // --- CHILD PROCESS ---
    close(listen_fd);
    http_handle(client_fd, root_fd);
    close(client_fd);
    close(root_fd);
    _exit(0);
  }

  // --- PARENT PROCESS ---
  workers++;
  close(client_fd);
}
