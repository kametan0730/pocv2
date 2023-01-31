#ifndef CACHE_H
#define CACHE_H

#include "types.h"

void dcache_flush_poc(u64 va_start, u64 va_end);

#define dcache_flush_poc_range(v, s)  __dcache_flush_poc_range((u64)(v), (u64)(s))

static inline void __dcache_flush_poc_range(u64 va, u64 size) {
  u64 end = va + size;
  dcache_flush_poc(va, end);
}

#endif
