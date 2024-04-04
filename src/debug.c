#include <handmadehero/debug.h>

#if HANDMADEHERO_DEBUG

#include <stdio.h>

void
debug(const char *string)
{
  fputs(string, stdout);
}

void
debugf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
}

#endif /* HANDMADEHERO_DEBUG */