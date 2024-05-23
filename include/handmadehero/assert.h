#ifndef HANDMADEHERO_ASSERT_H
#define HANDMADEHERO_ASSERT_H

#if HANDMADEHERO_DEBUG

#if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
#define __assert __builtin_debugtrap()
#elif defined(_MSC_VER)
#define __assert __debugbreak()
#else
#include <signal.h>
#define __assert raise(SIGTRAP)
#endif

#define assert(expression)                                                                                             \
  if (!(expression)) {                                                                                                 \
    __assert;                                                                                                          \
  }

#define InvalidCodePath __builtin_trap()

#else

#define assert()
#define InvalidCodePath

#endif

/*
 * Generates duplicate case on failure.
 * Only happens at compile time.
 */
#define static_assert(predicate)                                                                                       \
  switch (0) {                                                                                                         \
  case 0:                                                                                                              \
  case predicate:;                                                                                                     \
  }

#endif /* HANDMADEHERO_ASSERT_H */
