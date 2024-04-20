#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <handmadehero/handmadehero.h>
#include <handmadehero/types.h>

#define NEWLINE "\n"

#define infon(str, len) write(STDOUT_FILENO, str, len)
#define info(str) infon(str, sizeof(str) - 1)

#define fatal(str) write(STDERR_FILENO, str, sizeof(str) - 1)

internal void
usage(void)
{
  fatal("hh_record_read <filepath>" NEWLINE);
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
    fatal("cannot open path" NEWLINE);
    return 1;
  }

  comptime off_t KILOBYTES = 1 << 10;
  comptime off_t MEGABYTES = 1 << 20;
  comptime off_t GIGABYTES = 1 << 30;
  off_t game_memory_total = 256 * MEGABYTES + 1 * GIGABYTES;
  game_memory_total -=
      // for wayland allocations
      2 * MEGABYTES
      // for xkb keyboard allocations
      + 1 * MEGABYTES
      // for event allocations
      + 256 * KILOBYTES;
  off_t seekBytes = lseek(state.fd, game_memory_total, SEEK_SET);
  if (seekBytes < 0) {
    fatal("cannot seek to input" NEWLINE);
    return 1;
  }

  u8 status = PLAYBACK_CONTINUE;
  while (status == PLAYBACK_CONTINUE) {
    status = PlaybackInput(&state, &next);
    if (status == PLAYBACK_FINISH)
      goto finish;
    else if (status == PLAYBACK_ERROR) {
      fatal("playback error occured" NEWLINE);
      goto finish;
    }

    // printing
    for (u8 controller_index = 0; controller_index < ARRAY_COUNT(next.controllers); controller_index++) {
      struct game_controller_input *prev_controller = GetController(&prev, controller_index);
      struct game_controller_input *controller = GetController(&next, controller_index);

      if (!AnyKeyEvent(prev_controller, controller))
        continue;

      info("source: ");
      if (controller_index == 0)
        info("keyboard");
      else
        info("controller");

      info(" type: ");
      if (controller->isAnalog)
        info("analog");
      else
        info("digital");

      u8 anyMovementX = prev_controller->stickAverageX != controller->stickAverageX;
      if (anyMovementX) {
        info(" stickX: ");
        printf("%.2f", controller->stickAverageX);
        fflush(stdout);
      }
      u8 anyMovementY = prev_controller->stickAverageY != controller->stickAverageY;
      if (anyMovementY) {
        info(" stickY: ");
        printf("%.2f", controller->stickAverageY);
        fflush(stdout);
      }

      for (u8 buttonIndex = 0; buttonIndex < ARRAY_COUNT(prev_controller->buttons); buttonIndex++) {
        /* only print changed button */
        u8 anyButtonPress = prev_controller->buttons[buttonIndex].pressed != controller->buttons[buttonIndex].pressed;
        if (!anyButtonPress)
          continue;

        const struct ButtonDescription *description = &ButtonDescriptionTable[buttonIndex];

        info(" ");
        infon(description->name, description->len);
        info(": ");

        if (controller->buttons[buttonIndex].pressed)
          info("pressed");
        else
          info("released");
      }

      info(NEWLINE);
    }

    prev = next;
  }

finish:
  if (PlaybackInputEnd(&state)) {
    fatal("cannot close path" NEWLINE);
    return 1;
  }

  return 0;
}
