#ifndef HANDMADEHERO_ASSERT_H
#define HANDMADEHERO_ASSERT_H

#if HANDMADEHERO_DEBUG

#if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
#define __HANDMADEHERO_ASSERT __builtin_debugtrap()
#elif defined(_MSC_VER)
#define __HANDMADEHERO_ASSERT __debugbreak()
#else
#include <signal.h>
#define __HANDMADEHERO_ASSERT raise(SIGTRAP)
#endif

#define assert(expression)                                                                                             \
  if (!(expression)) {                                                                                                 \
    __HANDMADEHERO_ASSERT;                                                                                             \
  }

#define InvalidCodePath __builtin_trap()

#else

#define assert(expression)
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
