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

#define SERVER_IO_TIMEOUT_MS  5000
#define REQUEST_BUFFER_SIZE   8192
#define PATH_BUFFER_SIZE      4096
#define HEADER_BUFFER_SIZE    512
#define FILE_CHUNK_SIZE       8192

#define METHOD_SIZE           16
#define METHOD_WIDTH_STR      "15"
#define VERSION_SIZE          16
#define VERSION_WIDTH_STR     "15"
#define URL_WIDTH_STR         "4095"

typedef struct {
  char method[METHOD_SIZE];
  char url[PATH_BUFFER_SIZE];
  char version[VERSION_SIZE];
} HttpRequest;

static const char *http_reason_string(HttpStatus status) {
  switch (status) {
    case HTTP_OK:               return "OK";
    case HTTP_BAD_REQUEST:      return "Bad Request";
    case HTTP_FORBIDDEN:        return "Forbidden";
    case HTTP_NOT_FOUND:        return "Not Found";
    case HTTP_METHOD_NOT_ALLOW: return "Method Not Allowed";
    case HTTP_TIMEOUT:          return "Request Timeout";
    case HTTP_TOO_LARGE:        return "Request Header Fields Too Large";
    case HTTP_INTERNAL_ERR:     return "Internal Server Error";
    case HTTP_VERSION_NOT_SUP:  return "HTTP Version Not Supported";
    default:                    return "Internal Server Error";
  }
}

static const char *http_body_string(HttpStatus status) {
  switch (status) {
    case HTTP_BAD_REQUEST:      return "Bad request\n";
    case HTTP_FORBIDDEN:        return "Forbidden\n";
    case HTTP_NOT_FOUND:        return "Not found\n";
    case HTTP_METHOD_NOT_ALLOW: return "Method not allowed\n";
    case HTTP_TIMEOUT:          return "Request timeout\n";
    case HTTP_TOO_LARGE:        return "Request too large\n";
    case HTTP_INTERNAL_ERR:     return "Internal server error\n";
    case HTTP_VERSION_NOT_SUP:  return "HTTP version not supported\n";
    default:                    return "Internal server error\n";
  }
}

static int wait_fd(int fd, short events) {
  struct pollfd pfd = {
    .fd = fd,
    .events = events,
    .revents = 0
  };

  for (;;) {
    int rc = poll(&pfd, 1, SERVER_IO_TIMEOUT_MS);

    if (rc > 0) {
      if (pfd.revents & events)
        return 0;

      errno = (pfd.revents & POLLNVAL) ? EBADF : EIO;
      return -1;
    }

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

static int response_headers(int fd, HttpStatus status) {
  char header[HEADER_BUFFER_SIZE];

  const char *reason = http_reason_string(status);
  const char *body = http_body_string(status);
  size_t body_len = strlen(body);

  int n = snprintf(header, sizeof(header),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: %zu\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "Connection: close\r\n"
      "\r\n",
      status, reason, body_len);

  if (n < 0 || (size_t)n >= sizeof(header))
    return -1;

  return send_all(fd, header, (size_t)n);
}

static int response(int fd, HttpStatus status, const char *method) {
  if (response_headers(fd, status) == -1)
    return -1;

  if (method && strcmp(method, "HEAD") == 0)
    return 0;

  const char *body = http_body_string(status);

  return send_all(fd, body, strlen(body));
}

static int read_request(int fd, char *buffer, size_t capacity) {
  size_t used = 0;

  if (capacity < 2) {
    errno = EINVAL;
    return -1;
  }

  buffer[0] = '\0';

  while (used < capacity - 1) {
    if (wait_fd(fd, POLLIN) == -1)
      return -1;

    ssize_t n = recv(fd, buffer + used, capacity - 1 - used, 0);

    if (n == 0) {
      errno = ECONNRESET;
      return -1;
    }

    if (n == -1) {
      if (errno == EINTR)
        continue;

      return -1;
    }

    used += (size_t)n;
    buffer[used] = '\0';

    if (strstr(buffer, "\r\n\r\n"))
      return 0;
  }

  errno = EMSGSIZE;
  return -1;
}

static int parse_request(char *buffer, HttpRequest *request) {
  char *header_end = strstr(buffer, "\r\n\r\n");

  if (!header_end)
    return -1;

  char *line_end = strstr(buffer, "\r\n");

  if (!line_end || line_end > header_end)
    return -1;

  *line_end = '\0';

  char extra[2];

  int fields = sscanf(buffer,
      "%" METHOD_WIDTH_STR "s %"
      URL_WIDTH_STR "s %"
      VERSION_WIDTH_STR "s %1s",
      request->method,
      request->url,
      request->version,
      extra);

  if (fields != 3)
    return -1;

  return 0;
}

static int serve_file(int client_fd, int root_fd, const char *url, const char *method) {
  char path[PATH_BUFFER_SIZE];

  if (secure_path(url, path, sizeof(path)) == -1)
    return response(client_fd, HTTP_FORBIDDEN, method);

  if (!strcmp(path, "/"))
    strcpy(path, "/index.html");

  struct stat st;
  int file_fd = file_open(root_fd, path, &st);

  if (file_fd == -1) {
    if (errno == ENOENT || errno == ENOTDIR)
      return response(client_fd, HTTP_NOT_FOUND, method);

    if (errno == EACCES || errno == ELOOP || errno == EISDIR)
      return response(client_fd, HTTP_FORBIDDEN, method);

    return response(client_fd, HTTP_INTERNAL_ERR, method);
  }

  char header[HEADER_BUFFER_SIZE];

  int n = snprintf(header, sizeof(header),
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %lld\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "Connection: close\r\n"
      "\r\n",
      file_content_type(path), (long long)st.st_size);

  if (n < 0 || (size_t)n >= sizeof(header)) {
    close(file_fd);
    return -1;
  }

  int rc = send_all(client_fd, header, (size_t)n);

  if (rc == 0 && strcmp(method, "HEAD") != 0) {
    char buf[FILE_CHUNK_SIZE];

    for (;;) {
      ssize_t nr = read(file_fd, buf, sizeof(buf));

      if (nr > 0) {
        if (send_all(client_fd, buf, (size_t)nr) == -1) {
          rc = -1;
          break;
        }
      } else if (nr == 0) {
        break;
      } else if (errno == EINTR) {
        continue;
      } else {
        rc = -1;
        break;
      }
    }
  }

  close(file_fd);
  return rc;
}

int http_handle(int client_fd, int root_fd) {
  char buffer[REQUEST_BUFFER_SIZE + 1];
  HttpRequest request;

  if (read_request(client_fd, buffer, sizeof(buffer)) == -1) {
    if (errno == ETIMEDOUT)
      return response(client_fd, HTTP_TIMEOUT, "GET");

    if (errno == EMSGSIZE)
      return response(client_fd, HTTP_TOO_LARGE, "GET");

    return -1;
  }

  if (parse_request(buffer, &request) == -1)
    return response(client_fd, HTTP_BAD_REQUEST, "GET");

  if (strcmp(request.method, "GET") != 0 &&
      strcmp(request.method, "HEAD") != 0) {
    return response(
        client_fd, HTTP_METHOD_NOT_ALLOW, request.method);
  }

  if (strcmp(request.version, "HTTP/1.0") != 0 &&
      strcmp(request.version, "HTTP/1.1") != 0) {
    return response(
        client_fd, HTTP_VERSION_NOT_SUP, request.method);
  }

  return serve_file(
      client_fd, root_fd, request.url, request.method);
}
