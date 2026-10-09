#ifndef HSERVE_HTTP_H
#define HSERVE_HTTP_H

typedef enum {
  HTTP_OK               = 200,
  HTTP_BAD_REQUEST      = 400,
  HTTP_FORBIDDEN        = 403,
  HTTP_NOT_FOUND        = 404,
  HTTP_METHOD_NOT_ALLOW = 405,
  HTTP_TIMEOUT          = 408,
  HTTP_TOO_LARGE        = 431,
  HTTP_INTERNAL_ERR     = 500,
  HTTP_VERSION_NOT_SUP  = 505
} HttpStatus;

int http_handle(int client_fd, int root_fd);

#endif
