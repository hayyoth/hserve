#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http.h"
#include "file.h"
#include "secure.h"

#define REQUEST_MAX 8192
#define IO_TIMEOUT 5000

static int wait_fd(int fd, short events) {
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;

  for (;;) {
    int rc = poll(&pfd, 1, IO_TIMEOUT);

    if (rc > 0)
      return (pfd.revents & events) ? 0 : -1;

    if (rc == 0) {
      errno = ETIMEDOUT;
      return -1;
    }

    if (errno != EINTR)
      return -1;
  }
}

static int send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;

  while (sent < len) {
    if (wait_fd(fd, POLLOUT) == -1)
      return -1;

    ssize_t n = send(fd, buf + sent, len - sent, 0);

    if (n > 0) {
      sent += (size_t)n;
    } else if (n == -1 && errno == EINTR) {
      continue;
    } else {
      return -1;
    }
  }

  return 0;
}

static int response(int fd, int status, const char *reason,
    const char *body, int head_only) {
  char header[512];
  size_t body_len = strlen(body);

  int n = snprintf(header, sizeof(header),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: %zu\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "Connection: close\r\n\r\n",
      status, reason, body_len);

  if (n < 0 || (size_t)n >= sizeof(header))
    return -1;

  if (send_all(fd, header, (size_t)n) == -1)
    return -1;

  if (!head_only && send_all(fd, body, body_len) == -1)
    return -1;

  return 0;
}

static int serve_file(int client_fd, int root_fd,
    const char *url, int head_only) {
  char path[4096];

  if (secure_path(url, path, sizeof(path)) == -1)
    return response(client_fd, 403, "Forbidden", "Forbidden\n", head_only);

  if (!strcmp(path, "/"))
    strcpy(path, "/index.html");

  struct stat st;
  int file_fd = file_open(root_fd, path, &st);

  if (file_fd == -1) {
    if (errno == ENOENT || errno == ENOTDIR)
      return response(client_fd, 404, "Not Found", "Not found\n", head_only);

    if (errno == EACCES || errno == ELOOP || errno == EISDIR)
      return response(client_fd, 403, "Forbidden", "Forbidden\n", head_only);

    return response(client_fd, 500, "Internal Server Error",
        "Internal server error\n", head_only);
  }

  char header[512];
  int n = snprintf(header, sizeof(header),
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %lld\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "Connection: close\r\n\r\n",
      file_content_type(path), (long long)st.st_size);

  if (n < 0 || (size_t)n >= sizeof(header)) {
    close(file_fd);
    return -1;
  }

  int rc = send_all(client_fd, header, (size_t)n);

  if (rc == 0 && !head_only) {
    char buf[8192];

    for (;;) {
      if (wait_fd(file_fd, POLLIN) == -1) {
        rc = -1;
        break;
      }

      ssize_t nr = read(file_fd, buf, sizeof(buf));

      if (nr > 0) {
        if (send_all(client_fd, buf, (size_t)nr) == -1) {
          rc = -1;
          break;
        }
      } else if (nr == 0) {
        break;
      } else if (errno != EINTR) {
        rc = -1;
        break;
      }
    }
  }

  close(file_fd);
  return rc;
}

int http_handle(int client_fd, int root_fd) {
  char request[REQUEST_MAX + 1];
  size_t used = 0;

  while (used < REQUEST_MAX) {
    if (wait_fd(client_fd, POLLIN) == -1)
      return response(client_fd, 408, "Request Timeout",
          "Request timeout\n", 0);

    ssize_t n = recv(client_fd, request + used, REQUEST_MAX - used, 0);

    if (n == 0)
      return -1;

    if (n == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }

    used += (size_t)n;
    request[used] = '\0';

    if (strstr(request, "\r\n\r\n"))
      break;
  }

  if (!strstr(request, "\r\n\r\n"))
    return response(client_fd, 431, "Request Header Fields Too Large",
        "Request too large\n", 0);

  char *line_end = strstr(request, "\r\n");
  if (!line_end)
    return response(client_fd, 400, "Bad Request", "Bad request\n", 0);

  *line_end = '\0';

  char method[16];
  char url[4096];
  char version[16];
  char extra[2];

  if (sscanf(request, "%15s %4095s %15s %1s",
      method, url, version, extra) != 3)
    return response(client_fd, 400, "Bad Request", "Bad request\n", 0);

  int head_only;

  if (!strcmp(method, "GET"))
    head_only = 0;
  else if (!strcmp(method, "HEAD"))
    head_only = 1;
  else
    return response(client_fd, 405, "Method Not Allowed",
        "Method not allowed\n", 0);

  if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1"))
    return response(client_fd, 505, "HTTP Version Not Supported",
        "HTTP version not supported\n", head_only);

  return serve_file(client_fd, root_fd, url, head_only);
}
