#include "common.h"

void memacopy(void *dest, void *src, int size) {
  char *csrc = (char *)src;
  char *cdest = (char *)dest;
  for (int i = 0; i < size; i++) {
    cdest[i] = csrc[i];
  }
}
