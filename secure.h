#ifndef HSERVE_SECURE_H
#define HSERVE_SECURE_H

#include <stddef.h>

int secure_path(const char *url, char *out, size_t size);

#endif
