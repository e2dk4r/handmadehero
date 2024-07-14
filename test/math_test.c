#include <handmadehero/math.h>
#include <handmadehero/types.h>

enum math_test_error {
  MATH_TEST_ERROR_NONE = 0,
  MATH_TEST_ERROR_MINIMUM,
  MATH_TEST_ERROR_MAXIMUM,
};

#define test(result, expected)
int
main(void)
{
  enum math_test_error errorCode = MATH_TEST_ERROR_NONE;

  // Minimum
  {
    f32 a = 3.0f;
    f32 b = 4.0f;
    f32 result = Minimum(a, b);
    f32 expected = a;
    if (result != expected) {
      errorCode = MATH_TEST_ERROR_MINIMUM;
      goto end;
    }
  }

  // Maximum
  {
    f32 a = 3.0f;
    f32 b = 4.0f;
    f32 result = Maximum(a, b);
    f32 expected = b;
    if (result != expected) {
      errorCode = MATH_TEST_ERROR_MAXIMUM;
      goto end;
    }
  }

end:
  return (s32)errorCode;
}
