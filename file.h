#ifndef HSERVE_FILE_H
#define HSERVE_FILE_H

#include <sys/stat.h>

int file_open(int root_fd, const char *path, struct stat *st);
const char *file_content_type(const char *path);

#endif
