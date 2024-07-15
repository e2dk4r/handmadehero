#ifndef HANDMADEHERO_TEXT_H
#define HANDMADEHERO_TEXT_H

#include "types.h"

struct string {
  const u8 *value;
  u64 length;
};

internal inline struct string
StringFromZeroTerminated(const u8 *src, u64 max)
{
  struct string string = {};

  string.value = src;
  while (*src) {
    string.length++;
    if (string.length == max)
      break;
    src++;
  }

  return string;
}

internal inline b32
StringEndsWith(struct string source, struct string search)
{
  if (source.length == 0 || search.length == 0)
    return 0;

  if (search.length > source.length)
    return 0;

  u64 sourceIndex = source.length - search.length;
  for (u64 searchIndex = 0; searchIndex < search.length; searchIndex++) {
    if (source.value[sourceIndex] != search.value[searchIndex])
      return 0;
    sourceIndex++;
  }

  return 1;
}

#endif /* HANDMADEHERO_TEXT_H */
