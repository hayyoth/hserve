#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

#include "file.h"

int file_open_root(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    perror("hserve: open root");
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) == -1 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "hserve: path is not a directory\n");
    close(fd);
    return -1;
  }
  return fd;
}

static int fail(int dir_fd, int next_fd, int err) {
  if (dir_fd != -1) close(dir_fd);
  if (next_fd != -1) close(next_fd);
  errno = err;
  return -1;
}

int file_open(int root_fd, const char *path, struct stat *st) {
  if (!path || path[0] != '/' || !st)
    return fail(-1, -1, EINVAL);

  int dir_fd = dup(root_fd);
  if (dir_fd == -1)
    return fail(-1, -1, errno);

  const char *p = path + 1;
  if (!*p)
    p = "index.html";

  for (;;) {
    const char *slash = strchr(p, '/');
    size_t len = slash ? (size_t)(slash - p) : strlen(p);

    if (!len || len >= 256 ||
        (len == 1 && p[0] == '.') ||
        (len == 2 && p[0] == '.' && p[1] == '.'))
      return fail(dir_fd, -1, EACCES);

    char name[256];
    memcpy(name, p, len);
    name[len] = '\0';

    int flags = O_RDONLY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int fd = openat(dir_fd, name, flags);
    int err = errno;
    close(dir_fd);

    if (fd == -1)
      return fail(-1, -1, err);

    struct stat sb;
    if (fstat(fd, &sb) == -1)
      return fail(-1, fd, errno);

    if (slash ? !S_ISDIR(sb.st_mode) : !S_ISREG(sb.st_mode))
      return fail(-1, fd, slash ? ENOTDIR : EACCES);

    if (!slash) {
      *st = sb;
      return fd;
    }

    dir_fd = fd;
    p = slash + 1;

    if (!*p)
      return fail(dir_fd, -1, EISDIR);
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
