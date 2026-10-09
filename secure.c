#include <errno.h>
#include <string.h>

#include "secure.h"

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int secure_path(const char *url, char *out, size_t size) {
  if (!url || !out || size < 2 || url[0] != '/') {
    errno = EINVAL;
    return -1;
  }

  size_t n = 0;

  for (size_t i = 0; url[i] && url[i] != '?'; i++) {
    unsigned char c = (unsigned char)url[i];

    if (c == '%') {
      if (!url[i + 1] || !url[i + 2]) {
        errno = EINVAL;
        return -1;
      }

      int hi = hex_value(url[i + 1]);
      int lo = hex_value(url[i + 2]);

      if (hi < 0 || lo < 0) {
        errno = EINVAL;
        return -1;
      }

      c = (unsigned char)(hi * 16 + lo);
      i += 2;
    }

    if (c == 0 || c == '\\' || c < 0x20 || c == 0x7f) {
      errno = EINVAL;
      return -1;
    }

    if (n + 1 >= size) {
      errno = ENAMETOOLONG;
      return -1;
    }

    out[n++] = (char)c;
  }

  out[n] = '\0';

  const char *p = out + 1;
  while (*p) {
    const char *slash = strchr(p, '/');
    size_t len = slash ? (size_t)(slash - p) : strlen(p);

    if (len == 0 ||
        (len == 1 && p[0] == '.') ||
        (len == 2 && p[0] == '.' && p[1] == '.')) {
      errno = EACCES;
      return -1;
    }

    if (!slash)
      break;

    p = slash + 1;
  }

  return 0;
}
