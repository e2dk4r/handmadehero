#include <handmadehero/text.h>
#include <handmadehero/types.h>

enum text_test_error {
  TEXT_TEST_ERROR_NONE = 0,
  TEXT_TEST_ERROR_STRING_FROM_ZERO_TERMINATED,
  TEXT_TEST_ERROR_STRING_FROM_ZERO_TERMINATED_TRUNCATED,
  TEXT_TEST_ERROR_STRING_ENDS_WITH,
  TEXT_TEST_ERROR_STRING_ENDS_WITH_EXPECTED_FALSE,
  TEXT_TEST_ERROR_STRING_ENDS_WITH_WHEN_SEARCH_IS_BIGGER,
  TEXT_TEST_ERROR_PATH_HAS_EXTENSION,
  TEXT_TEST_ERROR_PATH_HAS_EXTENSION_EXPECTED_FALSE,
  TEXT_TEST_ERROR_PATH_HAS_EXTENSION_WHEN_EXTENSION_IS_BIGGER,
  TEXT_TEST_ERROR_HEX_U8,
  TEXT_TEST_ERROR_HEX_U16,
  TEXT_TEST_ERROR_HEX_U32,
  TEXT_TEST_ERROR_IS_HEX_MUST_BE_FALSE,
  TEXT_TEST_ERROR_IS_HEX_MUST_BE_TRUE,
  TEXT_TEST_ERROR_GET_HEX,
};

int
main(void)
{
  enum text_test_error errorCode = TEXT_TEST_ERROR_NONE;

  // StringFromZeroTerminated
  {
    char *input = "abc";
    struct string result = StringFromZeroTerminated((u8 *)input, 1024);

    char *expectedValue = input;
    u64 expectedLength = 3;
    if ((char *)result.value != input || result.length != expectedLength) {
      errorCode = TEXT_TEST_ERROR_STRING_FROM_ZERO_TERMINATED;
      goto end;
    }
  }

  {
    char *input = "abcdefghijklm";
    struct string result = StringFromZeroTerminated((u8 *)input, 3);

    char *expectedValue = input;
    u64 expectedLength = 3;
    if (result.length != expectedLength) {
      errorCode = TEXT_TEST_ERROR_STRING_FROM_ZERO_TERMINATED_TRUNCATED;
      goto end;
    }
  }

  // StringEndsWith
  {
    struct string string = StringFromZeroTerminated((u8 *)"abcdefghijkl", 1024);
    struct string search = StringFromZeroTerminated((u8 *)"jkl", 1024);
    b32 result = StringEndsWith(string, search);
    b32 expected = 1;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_STRING_ENDS_WITH;
      goto end;
    }
  }

  {
    struct string string = StringFromZeroTerminated((u8 *)"abcdefghijkl", 1024);
    struct string search = StringFromZeroTerminated((u8 *)"abc", 1024);
    b32 result = StringEndsWith(string, search);
    b32 expected = 0;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_STRING_ENDS_WITH_EXPECTED_FALSE;
      goto end;
    }
  }

  {
    struct string string = StringFromZeroTerminated((u8 *)"abcdefghijkl", 1024);
    struct string search = StringFromZeroTerminated((u8 *)"abcdefghijklmnop", 1024);
    b32 result = StringEndsWith(string, search);
    b32 expected = 0;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_STRING_ENDS_WITH_WHEN_SEARCH_IS_BIGGER;
      goto end;
    }
  }

  // PathHasExtension
  {
    struct string path = StringFromZeroTerminated((u8 *)"file.jpg", 1024);
    struct string extension = StringFromZeroTerminated((u8 *)"jpg", 1024);
    b32 result = PathHasExtension(path, extension);
    b32 expected = 1;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_PATH_HAS_EXTENSION;
      goto end;
    }
  }

  {
    struct string path = StringFromZeroTerminated((u8 *)"file.jpg", 1024);
    struct string extension = StringFromZeroTerminated((u8 *)"png", 1024);
    b32 result = PathHasExtension(path, extension);
    b32 expected = 0;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_PATH_HAS_EXTENSION_EXPECTED_FALSE;
      goto end;
    }
  }

  {
    struct string path = StringFromZeroTerminated((u8 *)"file.jpg", 1024);
    struct string extension = StringFromZeroTerminated((u8 *)"very_long_extension", 1024);
    b32 result = PathHasExtension(path, extension);
    b32 expected = 0;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_PATH_HAS_EXTENSION_WHEN_EXTENSION_IS_BIGGER;
      goto end;
    }
  }

  // HexStringToX
  {
    // u8
    struct string hex = StringFromZeroTerminated((u8 *)"fa", 1024);
    u8 result = HexStringToU8(hex);
    u8 expected = 0xfa;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_HEX_U8;
      goto end;
    }
  }

  {
    struct string hex = StringFromZeroTerminated((u8 *)"5c0f", 1024);
    u16 result = HexStringToU16(hex);
    u16 expected = 0x5c0f;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_HEX_U16;
      goto end;
    }
  }

  // IsHex
  {
    char character = 'a';
    u8 result = IsHex(character);
    u8 expected = 1;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_IS_HEX_MUST_BE_TRUE;
      goto end;
    }
  }

  {
    char character = 'g';
    u8 result = IsHex(character);
    u8 expected = 0;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_IS_HEX_MUST_BE_FALSE;
      goto end;
    }
  }

  // GetHex
  {
    char character = 'a';
    u8 result = GetHex(character);
    u8 expected = 0xa;
    if (result != expected) {
      errorCode = TEXT_TEST_ERROR_GET_HEX;
      goto end;
    }
  }

end:
  return (s32)errorCode;
}
