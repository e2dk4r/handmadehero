#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/types.h>
#include <pthread.h>

#if HANDMADEHERO_DEBUG

#include <handmadehero/text.h>
#include <stdio.h>
#include <unistd.h>

void
debug(const char *zeroTerminatedString)
{
  struct string string = StringFromZeroTerminated((u8 *)zeroTerminatedString, 1024);
  assert(string.length > 0);
  write(STDOUT_FILENO, string.value, string.length);
}

void
debugf(const char *format, ...)
{
  global_variable char buffer[1024] = {};
  global_variable pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  pthread_mutex_lock(&lock);

  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, ap);
  assert(len > 0);
  write(STDOUT_FILENO, buffer, (size_t)len);
  va_end(ap);

  pthread_mutex_unlock(&lock);
}

#endif /* HANDMADEHERO_DEBUG */
