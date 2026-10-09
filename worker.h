#ifndef HSERVE_WORKER_H
#define HSERVE_WORKER_H

void worker_reap(void);
void worker_spawn(int listen_fd, int root_fd);

#endif
