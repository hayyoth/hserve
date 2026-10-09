#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "file.h"

int file_open(int root_fd, const char *path, struct stat *st) {
  if (!path || path[0] != '/') {
    errno = EINVAL;
    return -1;
  }

  int dir_fd = dup(root_fd);
  if (dir_fd == -1)
    return -1;

  const char *p = path + 1;
  if (*p == '\0')
    p = "index.html";

  for (;;) {
    const char *slash = strchr(p, '/');
    size_t len = slash ? (size_t)(slash - p) : strlen(p);

    if (len == 0 || len >= 256 ||
        (len == 1 && p[0] == '.') ||
        (len == 2 && p[0] == '.' && p[1] == '.')) {
      close(dir_fd);
      errno = EACCES;
      return -1;
    }

    char name[256];
    memcpy(name, p, len);
    name[len] = '\0';

    int flags = O_RDONLY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int next_fd = openat(dir_fd, name, flags);
    int saved_errno = errno;
    close(dir_fd);

    if (next_fd == -1) {
      errno = saved_errno;
      return -1;
    }

    struct stat current;
    if (fstat(next_fd, &current) == -1) {
      saved_errno = errno;
      close(next_fd);
      errno = saved_errno;
      return -1;
    }

    if (slash && !S_ISDIR(current.st_mode)) {
      close(next_fd);
      errno = ENOTDIR;
      return -1;
    }

    if (!slash) {
      if (!S_ISREG(current.st_mode)) {
        close(next_fd);
        errno = EACCES;
        return -1;
      }

      *st = current;
      return next_fd;
    }

    dir_fd = next_fd;
    p = slash + 1;

    if (*p == '\0') {
      close(dir_fd);
      errno = EISDIR;
      return -1;
    }
  }
}

const char *file_content_type(const char *path) {
  const char *ext = strrchr(path, '.');

  if (!ext) return "application/octet-stream";
  if (!strcmp(ext, ".html") || !strcmp(ext, ".htm"))
    return "text/html; charset=utf-8";
  if (!strcmp(ext, ".css")) return "text/css; charset=utf-8";
  if (!strcmp(ext, ".js")) return "text/javascript; charset=utf-8";
  if (!strcmp(ext, ".json")) return "application/json";
  if (!strcmp(ext, ".txt")) return "text/plain; charset=utf-8";
  if (!strcmp(ext, ".svg")) return "image/svg+xml";
  if (!strcmp(ext, ".png")) return "image/png";
  if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
    return "image/jpeg";
  if (!strcmp(ext, ".gif")) return "image/gif";
  if (!strcmp(ext, ".pdf")) return "application/pdf";
  if (!strcmp(ext, ".wasm")) return "application/wasm";

  return "application/octet-stream";
}
