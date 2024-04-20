#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <handmadehero/handmadehero.h>
#include <handmadehero/types.h>

#define STDOUT 1
#define STDERR 2
#define NEWLINE "\n"

internal void
usage(void)
{
  comptime char msg[] =
      /* short description */
      "hh_record_read <filepath>" NEWLINE;
  comptime u64 msg_len = sizeof(msg) - 1;

  write(STDERR, msg, msg_len);
}

struct record_state {
  int fd;
  char *path;
};

internal u8
PlaybackInputBegin(struct record_state *state)
{
  state->fd = open(state->path, O_RDONLY);
  return state->fd < 0;
}

internal u8
PlaybackInputEnd(struct record_state *state)
{
  return close(state->fd) != 0;
}

#define PLAYBACK_FINISH 0
#define PLAYBACK_ERROR 1
#define PLAYBACK_CONTINUE 2
internal u8
PlaybackInput(struct record_state *state, struct game_input *input)
{
  ssize_t bytesRead;

  bytesRead = read(state->fd, input, sizeof(*input));
  // error happened
  if (bytesRead < 0)
    return PLAYBACK_ERROR;

  // end of file
  if (bytesRead == 0) {
    return PLAYBACK_FINISH;
  }

  return PLAYBACK_CONTINUE;
}

u8
AnyKeyEvent(struct game_controller_input *prev, struct game_controller_input *next)
{
  if (prev->stickAverageX != next->stickAverageX)
    return 1;

  if (prev->stickAverageY != next->stickAverageY)
    return 1;

  for (u8 buttonIndex = 0; buttonIndex < ARRAY_COUNT(prev->buttons); buttonIndex++) {
    if (prev->buttons[buttonIndex].pressed != next->buttons[buttonIndex].pressed)
      return 1;
  }

  return 0;
}

comptime struct ButtonDescription {
  u64 len;
  char *name;
} ButtonDescriptionTable[] = {
#define BUTTON_DESCRIPTION(x)                                                                                          \
  {                                                                                                                    \
    .len = sizeof(x) - 1, .name = x                                                                                    \
  }
    BUTTON_DESCRIPTION("moveDown"),      BUTTON_DESCRIPTION("moveUp"),      BUTTON_DESCRIPTION("moveLeft"),
    BUTTON_DESCRIPTION("moveRight"),     BUTTON_DESCRIPTION("actionUp"),    BUTTON_DESCRIPTION("actionDown"),
    BUTTON_DESCRIPTION("actionLeft"),    BUTTON_DESCRIPTION("actionRight"), BUTTON_DESCRIPTION("leftShoulder"),
    BUTTON_DESCRIPTION("rightShoulder"), BUTTON_DESCRIPTION("start"),       BUTTON_DESCRIPTION("back"),
#undef BUTTON_DESCRIPTION
};

int
main(int argc, char *argv[])
{
  static_assert(ARRAY_COUNT(ButtonDescriptionTable) == ARRAY_COUNT(((struct game_controller_input *)0)->buttons));

  if (argc - 1 == 0) {
    usage();
    return 1;
  }

  struct game_input prev = {};
  struct game_input next = {};
  struct record_state state;
  state.fd = -1;
  state.path = argv[1];

  if (PlaybackInputBegin(&state)) {
    comptime char msg[] = "cannot open path" NEWLINE;
    comptime u64 msg_len = sizeof(msg) - 1;
    write(STDERR, msg, msg_len);
    return 1;
  }

  const off_t MEGABYTES = 1 << 20;
  const off_t GIGABYTES = 1 << 30;
  off_t game_state_size = 256 * MEGABYTES + 1 * GIGABYTES;
  off_t seekBytes = lseek(state.fd, game_state_size, SEEK_SET);
  if (seekBytes < 0) {
    comptime char msg[] = "cannot seek to input" NEWLINE;
    comptime u64 msg_len = sizeof(msg) - 1;
    write(STDERR, msg, msg_len);
    return 1;
  }

  u8 status;
playback:
  status = PlaybackInput(&state, &next);
  if (status == PLAYBACK_FINISH)
    goto finish;
  else if (status == PLAYBACK_ERROR) {
    comptime char msg[] = "playback error occured" NEWLINE;
    comptime u64 msg_len = sizeof(msg) - 1;
    write(STDERR, msg, msg_len);
    goto finish;
  }

  // printing
  for (u8 controller_index = 0; controller_index < ARRAY_COUNT(next.controllers); controller_index++) {
    struct game_controller_input *prev_controller = GetController(&prev, controller_index);
    struct game_controller_input *controller = GetController(&next, controller_index);

    if (!AnyKeyEvent(prev_controller, controller))
      continue;

    comptime char msg_source[] = "source: ";
    comptime u64 msg_source_len = sizeof(msg_source) - 1;
    write(STDOUT, msg_source, msg_source_len);

    if (controller_index == 0) {
      comptime char msg_keyboard[] = "keyboard";
      comptime u64 msg_keyboard_len = sizeof(msg_keyboard) - 1;
      write(STDOUT, msg_keyboard, msg_keyboard_len);
    } else {
      comptime char msg_controller[] = "controller";
      comptime u64 msg_controller_len = sizeof(msg_controller) - 1;
      write(STDOUT, msg_controller, msg_controller_len);
    }

    comptime char msg_type[] = " type: ";
    comptime u64 msg_type_len = sizeof(msg_type) - 1;
    write(STDOUT, msg_type, msg_type_len);
    if (controller->isAnalog) {
      comptime char msg_analog[] = "analog";
      comptime u64 msg_analog_len = sizeof(msg_analog) - 1;
      write(STDOUT, msg_analog, msg_analog_len);

    } else {
      comptime char msg_digital[] = "digital";
      comptime u64 msg_digital_len = sizeof(msg_digital) - 1;
      write(STDOUT, msg_digital, msg_digital_len);
    }

    u8 anyMovementX = prev_controller->stickAverageX != controller->stickAverageX;
    if (anyMovementX) {
      comptime char msg_stickX[] = " stickX: ";
      comptime u64 msg_stickX_len = sizeof(msg_stickX) - 1;
      write(STDOUT, msg_stickX, msg_stickX_len);
      printf("%.2f", controller->stickAverageX);
      fflush(stdout);
    }
    u8 anyMovementY = prev_controller->stickAverageY != controller->stickAverageY;
    if (anyMovementY) {
      comptime char msg_stickY[] = " stickY: ";
      comptime u64 msg_stickY_len = sizeof(msg_stickY) - 1;
      write(STDOUT, msg_stickY, msg_stickY_len);
      printf("%.2f", controller->stickAverageY);
      fflush(stdout);
    }

    for (u8 buttonIndex = 0; buttonIndex < ARRAY_COUNT(prev_controller->buttons); buttonIndex++) {
      /* only print changed button */
      u8 anyButtonPress = prev_controller->buttons[buttonIndex].pressed != controller->buttons[buttonIndex].pressed;
      if (!anyButtonPress)
        continue;

      const struct ButtonDescription *description = &ButtonDescriptionTable[buttonIndex];

      write(STDOUT, " ", 1);
      write(STDOUT, description->name, description->len);
      write(STDOUT, ": ", 2);

      if (controller->buttons[buttonIndex].pressed) {
        comptime char msg_pressed[] = "pressed";
        comptime u64 msg_pressed_len = sizeof(msg_pressed) - 1;
        write(STDOUT, msg_pressed, msg_pressed_len);
      } else {
        comptime char msg_released[] = "released";
        comptime u64 msg_released_len = sizeof(msg_released) - 1;
        write(STDOUT, msg_released, msg_released_len);
      }
    }

    write(STDOUT, NEWLINE, 1);
  }

  prev = next;

  goto playback;

finish:
  if (PlaybackInputEnd(&state)) {
    comptime char msg[] = "cannot close path" NEWLINE;
    comptime u64 msg_len = sizeof(msg) - 1;
    write(STDERR, msg, msg_len);
    return 1;
  }

  return 0;
}
