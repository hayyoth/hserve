#ifndef HSERVE_SECURE_H
#define HSERVE_SECURE_H

#include <stddef.h>

int is_invalid_segment(const char *p, size_t len);
int secure_path(const char *url, char *out, size_t size);

#endif
