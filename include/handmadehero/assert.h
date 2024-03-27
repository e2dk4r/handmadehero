#ifndef HANDMADEHERO_ASSERT_H
#define HANDMADEHERO_ASSERT_H

#define assert(expression)                                                     \
  if (!(expression)) {                                                         \
    __builtin_trap();                                                          \
  }

#define InvalidCodePath __builtin_trap()

#endif /* HANDMADEHERO_ASSERT_H */
