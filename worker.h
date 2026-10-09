#ifndef HSERVE_WORKER_H
#define HSERVE_WORKER_H

void worker_reap(int *workers);
void worker_spawn(int listen_fd, int root_fd, int *workers);

#endif
