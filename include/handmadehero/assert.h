#ifndef HANDMADEHERO_ASSERT_H
#define HANDMADEHERO_ASSERT_H

#if HANDMADEHERO_DEBUG

#define assert(expression)                                                     \
  if (!(expression)) {                                                         \
    __builtin_trap();                                                          \
  }

#define InvalidCodePath __builtin_trap()

#else

#define assert()
#define InvalidCodePath

#endif

#endif /* HANDMADEHERO_ASSERT_H */
