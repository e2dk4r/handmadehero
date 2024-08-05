#ifndef HANDMADEHERO_TEXT_H
#define HANDMADEHERO_TEXT_H

#include "types.h"

struct string {
  const u8 *value;
  u64 length;
};

internal inline struct string
StringFrom(const u8 *src, u64 length)
{
  struct string string = {};

  string.value = src;
  string.length = length;

  return string;
}

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

internal inline b32
PathHasExtension(struct string path, struct string extension)
{
  if (path.length == 0 || extension.length == 0)
    return 0;

  if (extension.length > path.length)
    return 0;

  u64 pathIndex = path.length - extension.length;
  if (path.length != extension.length && path.value[pathIndex - 1] != '.')
    return 0;

  for (u64 extensionIndex = 0; extensionIndex < extension.length; extensionIndex++) {
    if (path.value[pathIndex] != extension.value[extensionIndex])
      return 0;
    pathIndex++;
  }

  if (path.length != extension.length) {
  }

  return 1;
}

internal inline u8
HexStringToU8(struct string hexString)
{
  u8 value = 0;

  assert(hexString.length == 2);
  for (u64 index = 0; index < 2; index++) {
    u64 multiplier = 16;
    if (index != 0)
      multiplier /= 16;

    u8 character = hexString.value[index];
    u8 number = 0; // 0-15
    if (character >= '0' && character <= '9')
      number = character - '0';
    else if (character >= 'A' && character <= 'F')
      number = character - 'A' + 10;
    else if (character >= 'a' && character <= 'f')
      number = character - 'a' + 10;
    else
      assert(0 && "hex invalid");

    value += (u8)(number * multiplier);
  }

  return value;
}

#endif /* HANDMADEHERO_TEXT_H */
