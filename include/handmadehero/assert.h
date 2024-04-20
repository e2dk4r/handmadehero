#ifndef HANDMADEHERO_ASSERT_H
#define HANDMADEHERO_ASSERT_H

#if HANDMADEHERO_DEBUG

#define assert(expression)                                                                                             \
  if (!(expression)) {                                                                                                 \
    __builtin_trap();                                                                                                  \
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
