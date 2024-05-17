#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/types.h>

#if HANDMADEHERO_DEBUG

#include <stdio.h>
#include <unistd.h>

internal inline u64
strnlen(const char *p, u64 max)
{
  u64 length = 0;
  while (*p) {
    length++;
    if (length == max)
      break;
    p++;
  }
  return length;
}

void
debug(const char *string)
{
  u64 len = strnlen(string, 1024);
  assert(len > 0);
  write(STDOUT_FILENO, string, len);
}

void
debugf(const char *format, ...)
{
  static char buffer[1024] = {};

  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, ap);
  assert(len > 0);
  write(STDOUT_FILENO, buffer, (size_t)len);
  va_end(ap);
}

#endif /* HANDMADEHERO_DEBUG */
