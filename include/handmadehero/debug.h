#ifndef HANDMADEHERO_DEBUG_H
#define HANDMADEHERO_DEBUG_H

#if HANDMADEHERO_DEBUG

#include <stdarg.h>
void debug(const char *string);
void debugf(const char *format, ...);

#else

#define debug(str)
#define debugf(fmt, ...)

#endif /* HANDMADEHERO_DEBUG */

#endif /* HANDMADEHERO_DEBUG_H */
