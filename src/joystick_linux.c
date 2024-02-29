#include "joystick_linux.h"

u8 enumarateJoysticks(u8 *pCount, struct Joystick *joysticks) {
  u8 joystickCount = 0;
  struct udev *udev = udev_new();
  struct udev_enumerate *udev_enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(udev_enumerate, "input");
  udev_enumerate_add_match_property(udev_enumerate, "ID_INPUT_JOYSTICK", "1");

  if (udev_enumerate_scan_devices(udev_enumerate) < 0) {
    goto exit;
  }

  struct udev_list_entry *devices =
      udev_enumerate_get_list_entry(udev_enumerate);
  struct udev_list_entry *entry;

  for (entry = devices; entry; entry = udev_list_entry_get_next(entry)) {
    const char *path = udev_list_entry_get_name(entry);
    struct udev_device *udev_device = udev_device_new_from_syspath(udev, path);
    const char *devnode = udev_device_get_devnode(udev_device);
    /* /dev/input/eventXX not found */
    if (!devnode) {
      continue;
    }

    /* device found */
    if (joysticks) {
      struct Joystick *joystick = &joysticks[joystickCount];

      size_t devnode_length = 0;
      while (devnode[devnode_length] && devnode_length < JOYSTICK_PATH_MAX)
        devnode_length++;

      joystick->path_length = devnode_length;
      for (size_t index = 0;
           index < devnode_length && index < JOYSTICK_PATH_MAX; index++) {
        joystick->path[index] = devnode[index];
      }

      joystick->path[joystick->path_length] = 0;
    }

    joystickCount++;
  }

  *pCount = joystickCount;

exit:
  udev_enumerate_unref(udev_enumerate);
  udev_unref(udev);
  return joystickCount != 0;
}
