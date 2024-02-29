#include <handmadehero/types.h>
#include <libudev.h>

#define JOYSTICK_PATH_MAX 32
struct Joystick {
  char path[JOYSTICK_PATH_MAX];
  u8 path_length;
  int fd;
};

/*
 * returns 0 when no joystick found
 */
u8 enumarateJoysticks(u8 *pCount, struct Joystick *joysticks);
