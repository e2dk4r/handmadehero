/* system */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <liburing.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#define POLLIN 0x001 /* There is data to read.  */

#ifndef DT_CHR
#define DT_CHR 2
#endif

/* generated */
#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/* handmadehero */
#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/errors.h>
#include <handmadehero/handmadehero.h>

/*****************************************************************
 * platform layer implementation
 *****************************************************************/

#if HANDMADEHERO_INTERNAL

struct read_file_result PlatformReadEntireFile(char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return (struct read_file_result){};

  struct stat stat;
  if (fstat(fd, &stat))
    return (struct read_file_result){};
  ;

  assert(S_ISREG(stat.st_mode));
  void *data = malloc((size_t)stat.st_size);
  assert(data != 0);

  ssize_t bytesRead = read(fd, data, (size_t)stat.st_size);
  assert(bytesRead > 0);

  close(fd);

  return (struct read_file_result){
      .size = (u64)stat.st_size,
      .data = data,
  };
}

u8 PlatformWriteEntireFile(char *path, u64 size, void *data) {
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return 0;

  ssize_t bytesWritten = write(fd, data, size);
  assert(bytesWritten > 0);

  close(fd);

  return 1;
}

void PlatformFreeMemory(void *address) {
  free(address);
  address = 0;
}

#endif /* HANDMADEHERO_INTERNAL */

/*****************************************************************
 * memory bank
 *****************************************************************/
const u64 KILOBYTES = 1024;
const u64 MEGABYTES = 1024 * KILOBYTES;
const u64 GIGABYTES = 1024 * MEGABYTES;

struct mem {
  u64 current;
  u64 capacity;
  void *data;
};

static void *mem_push(struct mem *mem, size_t size) {
  void *ptr;

  assert(mem->current + size <= mem->capacity && "capacity overloaded");
  ptr = mem->data + mem->current;
  mem->current += size;

  return ptr;
}

#define mem_push_array(mem, type, count) mem_push(mem, sizeof(type) * count)

/*
 * allocates bytes on ram.
 *
 * @return int 1 if allocation failed, 0 otherwise
 * @example
 *   int failed = mem_alloc(memory, 1);
 *   if (failed)
 *     fail();
 * @param mem destionation structure
 * @param len length of allocated area
 */
static u8 mem_alloc(struct mem *mem, size_t len) {
  mem->capacity = len;
  mem->current = 0;
  mem->data =
      mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  u8 is_allocation_failed = mem->data == (void *)-1;
  return is_allocation_failed;
}

static u8 game_memory_allocation(struct game_memory *memory,
                                 u64 permanentStorageSize,
                                 u64 transientStorageSize) {
  memory->permanentStorageSize = permanentStorageSize;
  memory->transientStorageSize = transientStorageSize;
  u64 len = memory->permanentStorageSize + memory->transientStorageSize;

  void *data =
      mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  memory->permanentStorage = data;
  memory->transientStorage = data + permanentStorageSize;

  u8 is_allocation_failed = data == (void *)-1;
  return is_allocation_failed;
}

/***************************************************************
 * hot game code reloading
 ***************************************************************/

struct game_code {
#ifdef HANDMADEHERO_DEBUG
  char path[255];
  time_t time;
  void *module;
#endif

  pfnGameUpdateAndRender GameUpdateAndRender;
};

#ifdef HANDMADEHERO_DEBUG
#include <dlfcn.h>

void ReloadGameCode(struct game_code *lib) {
  struct stat sb;

  int fail = stat(lib->path, &sb);
  if (fail) {
    debugf("[ReloadGameCode] failed to stat\n");
    return;
  }

  if (sb.st_mtime == lib->time) {
    return;
  }

  // unload shared lib
  if (lib->module) {
    dlclose(lib->module);
    lib->module = 0;
  }

  // load shared lib
  lib->module = dlopen(lib->path, RTLD_NOW | RTLD_LOCAL);
  if (!lib->module) {
    debugf("[ReloadGameCode] failed to open\n");
    return;
  }

  // get function pointer
  void *fn = dlsym(lib->module, "GameUpdateAndRender");
  assert(fn != 0 && "wrong module format");

  // set function pointer
  lib->GameUpdateAndRender = fn;

  // update module time
  lib->time = sb.st_mtime;
  debugf("[ReloadGameCode] reloaded @%d\n", lib->time);
}

#endif

/*****************************************************************
 * structures
 *****************************************************************/
struct linux_state {
  struct wl_compositor *wl_compositor;
  struct wl_shm *wl_shm;
  struct wl_seat *wl_seat;
  struct xdg_wm_base *xdg_wm_base;

  struct wp_viewporter *wp_viewporter;
  struct wp_viewport *wp_viewport;
  struct wl_surface *wl_surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct wl_buffer *wl_buffer;

  struct wl_keyboard *wl_keyboard;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  struct game_code *lib;
  struct mem memory;
  struct game_memory *game_memory;
  struct game_backbuffer *backbuffer;
  struct game_input *input;

  u8 recordInputIndex;
  int recordInputFd;
  u8 playbackInputIndex;
  int playbackInputFd;

  u32 frame;
  u8 running : 1;
};

/***************************************************************
 * recording & playback inputs
 ***************************************************************/
#ifdef HANDMADEHERO_DEBUG

static const char recordPath[] = "input.rec";

static inline u8 RecordInputStarted(struct linux_state *state) {
  return state->recordInputIndex;
}

static void RecordInputBegin(struct linux_state *state, u8 index) {
  debug("[RecordInput] begin\n");
  state->recordInputIndex = index;
  unlink(recordPath);
  state->recordInputFd = open(recordPath, O_CREAT | O_WRONLY, 0644);
  assert(state->recordInputFd >= 0);
}

static void RecordInputEnd(struct linux_state *state) {
  debug("[RecordInput] end\n");
  state->recordInputIndex = 0;
  if (state->recordInputFd > 0)
    close(state->recordInputFd);
  state->recordInputFd = -1;
}

static void RecordInput(struct linux_state *state, struct game_input *input) {
  write(state->recordInputFd, input, sizeof(*input));
}

static inline u8 PlaybackInputStarted(struct linux_state *state) {
  return state->playbackInputIndex;
}

static void PlaybackInputBegin(struct linux_state *state, u8 index) {
  debug("[PlaybackInput] begin\n");
  state->playbackInputIndex = index;
  state->playbackInputFd = open(recordPath, O_RDONLY);
  assert(state->playbackInputFd >= 0);
}

static void PlaybackInputEnd(struct linux_state *state) {
  debug("[PlaybackInput] end\n");
  state->playbackInputIndex = 0;
  if (state->playbackInputFd > 0)
    close(state->playbackInputFd);
  state->playbackInputFd = -1;
}

static void PlaybackInput(struct linux_state *state, struct game_input *input) {
  ssize_t bytesRead;

begin:
  bytesRead = read(state->playbackInputFd, input, sizeof(*input));
  // error happened
  assert(bytesRead >= 0);

  // end of file
  if (bytesRead == 0) {
    off_t result = lseek(state->playbackInputFd, 0, SEEK_SET);
    assert(result >= 0);
    goto begin;
  }
}

#endif

/*****************************************************************
 * input handling
 *****************************************************************/

static void joystick_key(struct linux_state *state, u16 type, u16 code,
                         i32 value) {
  struct game_controller_input *controller = GetController(state->input, 1);

  debugf("[joystick_key] time: type: %d code: %d value: %d\n", type, code,
         value);
  controller->isAnalog = 1;

  if (type == 0) {
    return;
  }

  else if (code == ABS_HAT0Y || code == ABS_HAT1Y || code == ABS_HAT2Y ||
           code == ABS_HAT3Y) {
    controller->moveUp.pressed = value < 0;
    controller->moveDown.pressed = value > 0;
  }

  else if (code == ABS_HAT0X || code == ABS_HAT1X || code == ABS_HAT2X ||
           code == ABS_HAT3X) {
    controller->moveLeft.pressed = value < 0;
    controller->moveRight.pressed = value > 0;
  }

  else if (code == BTN_START) {
    controller->start.pressed = value < 0;
  }

  else if (code == BTN_SELECT) {
    controller->back.pressed = value < 0;
  }

  // normal of X values
  else if (code == ABS_X) {
    static const f32 MAX_LEFT = 32768.0f;
    static const f32 MAX_RIGHT = 32767.0f;
    f32 max = value < 0 ? MAX_LEFT : MAX_RIGHT;
    if (value >= -129 && value <= 128)
      value = 0;
    f32 x = (f32)value / max;
    controller->stickAverageX = x;
  }

  // normal of Y values
  else if (code == ABS_Y) {
    static const f32 MAX_LEFT = 32768.0f;
    static const f32 MAX_RIGHT = 32767.0f;
    f32 max = value < 0 ? MAX_LEFT : MAX_RIGHT;
    if (value >= -129 && value <= 128)
      value = 0;
    f32 y = (f32)value / max;
    controller->stickAverageY = -y;
  }

  else if (code == BTN_NORTH) {
    controller->actionUp.pressed = (u8)(value & 0x1);
  }

  else if (code == BTN_SOUTH) {
    controller->actionDown.pressed = (u8)(value & 0x1);
  }

  else if (code == BTN_WEST) {
    controller->actionLeft.pressed = (u8)(value & 0x1);
  }

  else if (code == BTN_EAST) {
    controller->actionRight.pressed = (u8)(value & 0x1);
  }
}

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                               u32 format, i32 fd, u32 size) {
  struct linux_state *state = data;
  debugf("[wl_keyboard::keymap] format: %d fd: %d size: %d\n", format, fd,
         size);

  void *keymap_str = mem_push(&state->memory, size);
  keymap_str = mmap(keymap_str, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  assert(keymap_str != MAP_FAILED);

  struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
      state->xkb_context, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
  struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);

  xkb_keymap_unref(state->xkb_keymap);
  xkb_state_unref(state->xkb_state);

  state->xkb_keymap = xkb_keymap;
  state->xkb_state = xkb_state;
}

static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                            u32 serial, u32 time, u32 key,
                            enum wl_keyboard_key_state keystate) {
  struct linux_state *state = data;
  debugf("[wl_keyboard::key] serial: %d time: %d key: %d state: %d\n", serial,
         time, key, keystate);

  u32 keycode = key + 8;
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, keycode);

  struct game_controller_input *controller = GetController(state->input, 0);
  switch (keysym) {
  case 'q': {
    state->running = 0;
  } break;

#ifdef HANDMADEHERO_DEBUG
  case 'L':
  case 'l': {
    /* if key is not pressed, exit */
    if (keystate == WL_KEYBOARD_KEY_STATE_RELEASED)
      break;

    /* if key is pressed, toggle between record and playback */
    if (!RecordInputStarted(state)) {
      memset(state->input->controllers, 0,
             HANDMADEHERO_CONTROLLER_COUNT * sizeof(state->input->controllers));
      PlaybackInputEnd(state);
      RecordInputBegin(state, 1);
    } else {
      RecordInputEnd(state);
      PlaybackInputBegin(state, 1);
    }
  } break;
#endif

  case 'A':
  case 'a': {
    // assert(controller->moveLeft.pressed != keystate);
    controller->moveLeft.pressed = (u8)(keystate & 0x1);
  } break;

  case 'D':
  case 'd': {
    // assert(controller->moveRight.pressed != keystate);
    controller->moveRight.pressed = (u8)(keystate & 0x1);
  } break;

  case 'W':
  case 'w': {
    // assert(controller->moveUp.pressed != keystate);
    controller->moveUp.pressed = (u8)(keystate & 0x1);
  } break;

  case 'S':
  case 's': {
    // assert(controller->moveDown.pressed != keystate);
    controller->moveDown.pressed = (u8)(keystate & 0x1);
  } break;
  }
}

static void wl_keyboard_modifiers_handle(void *data,
                                         struct wl_keyboard *wl_keyboard,
                                         u32 serial, u32 mods_depressed,
                                         u32 mods_latched, u32 mods_locked,
                                         u32 group) {
  struct linux_state *state = data;
  debugf("[wl_keyboard::modifiers] serial: %d depressed: %d latched: %d "
         "locked: %d group: %d\n",
         serial, mods_depressed, mods_latched, mods_locked, group);

  xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                    i32 rate, i32 delay) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::repeat_info] rate: %d delay: %d\n", rate, delay);
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
                              u32 serial, struct wl_surface *surface,
                              struct wl_array *keys) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::enter] serial: %d\n", serial);
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
                              u32 serial, struct wl_surface *surface) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::leave] serial: %d\n", serial);
}

comptime struct wl_keyboard_listener wl_keyboard_listener = {
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .keymap = wl_keyboard_keymap,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers_handle,
    .repeat_info = wl_keyboard_repeat_info,
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat,
                                 u32 capabilities) {
  struct linux_state *state = data;
  debugf("[wl_seat::capabilities] capabilities: %d\n", capabilities);

  u8 have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  if (have_keyboard && !state->wl_keyboard) {
    state->wl_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, state);
  } else if (!have_keyboard && state->wl_keyboard) {
    wl_keyboard_release(state->wl_keyboard);
    state->wl_keyboard = 0;
  }
}

static void wl_seat_name_handle(void *data, struct wl_seat *wl_seat,
                                const char *name) {
  debugf("[wl_seat::name] name: %s\n", name);
}

comptime struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_capabilities,
    .name = wl_seat_name_handle,
};

/*****************************************************************
 * shared memory
 *****************************************************************/
static i32 create_shared_memory(off_t size) {
  int fd;

  fd = memfd_create("buff", 0);
  if (fd < 0)
    return 0;

  int ok;
  do {
    ok = ftruncate(fd, size);
  } while (ok < 0 && errno == EINTR);
  if (ok < 0) {
    close(fd);
    return 0;
  }

  return fd;
}

/*****************************************************************
 * frame_callback events
 *****************************************************************/
static void wl_surface_frame_done(void *data, struct wl_callback *wl_callback,
                                  u32 time);

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

static void wl_surface_frame_done(void *data, struct wl_callback *wl_callback,
                                  u32 time) {
  wl_callback_destroy(wl_callback);
  struct linux_state *state = data;

  static struct game_input inputs[2] = {};
  struct game_input *newInput = &inputs[0];
  struct game_input *oldInput = &inputs[1];

  wl_callback = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(wl_callback, &wl_surface_frame_listener, data);

  u32 elapsed = time - state->frame;
  f32 second_in_milliseconds = 1000;
  f32 frames_per_second = 24;
  newInput->dtPerFrame = frames_per_second / second_in_milliseconds;

  f32 frame_unit = (f32)elapsed * second_in_milliseconds / frames_per_second;
  if (frame_unit > 1.0f) {
    state->input = newInput;

#ifdef HANDMADEHERO_DEBUG
    if (RecordInputStarted(state)) {
      RecordInput(state, state->input);
    }

    if (PlaybackInputStarted(state)) {
      PlaybackInput(state, state->input);
    }
#endif

    state->lib->GameUpdateAndRender(state->game_memory, newInput,
                                    state->backbuffer);
    wl_surface_attach(state->wl_surface, state->wl_buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0,
                             (i32)state->backbuffer->width,
                             (i32)state->backbuffer->height);

    state->frame = time;

    // swap inputs
    struct game_input *tempInput = newInput;
    newInput = oldInput;
    oldInput = tempInput;

    // load
#if HANDMADEHERO_DEBUG
    ReloadGameCode(state->lib);
#endif
  }

  wl_surface_commit(state->wl_surface);
}

/*****************************************************************
 * xdg_wm_base events
 *****************************************************************/
static void xdg_wm_base_ping_handle(void *data, struct xdg_wm_base *xdg_wm_base,
                                    u32 serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
  debugf("[xdg_wm_base::ping] pong(serial: %d)\n", serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping_handle,
};

/*****************************************************************
 * xdg_toplevel events
 *****************************************************************/

static void xdg_toplevel_configure_handle(void *data,
                                          struct xdg_toplevel *xdg_toplevel,
                                          i32 screen_width, i32 screen_height,
                                          struct wl_array *states) {
  debugf("[xdg_toplevel::configure] screen width: %d height: %d\n",
         screen_width, screen_height);
  if (screen_width == 0 || screen_height == 0)
    return;

  struct linux_state *state = data;

  if (state->wp_viewport) {
    wp_viewport_destroy(state->wp_viewport);
    state->wp_viewport = 0;
  }

  state->wp_viewport =
      wp_viewporter_get_viewport(state->wp_viewporter, state->wl_surface);
  wp_viewport_set_destination(state->wp_viewport, screen_width, screen_height);

  wl_surface_commit(state->wl_surface);
}

static void xdg_toplevel_close_handle(void *data,
                                      struct xdg_toplevel *xdg_toplevel) {
  struct linux_state *state = data;
  state->running = 0;
}

comptime struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handle,
    .close = xdg_toplevel_close_handle,
};

/*****************************************************************
 * xdg_surface events
 *****************************************************************/
static void xdg_surface_configure_handle(void *data,
                                         struct xdg_surface *xdg_surface,
                                         u32 serial) {
  struct linux_state *state = data;

  /* ack */
  xdg_surface_ack_configure(xdg_surface, serial);
  debugf("[xdg_surface::configure] ack_configure(serial: %d)\n", serial);

  wl_surface_attach(state->wl_surface, state->wl_buffer, 0, 0);
  wl_surface_commit(state->wl_surface);
}

comptime struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handle,
};

/*****************************************************************
 * wl_registry events
 *****************************************************************/

#define WL_COMPOSITOR_MINIMUM_REQUIRED_VERSION 4
#define WL_SHM_MINIMUM_REQUIRED_VERSION 1
#define WL_SEAT_MINIMUM_REQUIRED_VERSION 6
#define XDG_WM_BASE_MINIMUM_REQUIRED_VERSION 2
#define WP_VIEWPORTER_MINIMUM_REQUIRED_VERSION 1

static void registry_handle(void *data, struct wl_registry *wl_registry,
                            u32 name, const char *interface, u32 version) {
  struct linux_state *state = data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->wl_compositor =
        wl_registry_bind(wl_registry, name, &wl_compositor_interface,
                         WL_COMPOSITOR_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_compositor_interface\n");
  }

  else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface,
                                     WL_SHM_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_shm_interface\n");
  }

  else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface,
                                      WL_SEAT_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_seat\n");
  }

  else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->xdg_wm_base =
        wl_registry_bind(wl_registry, name, &xdg_wm_base_interface,
                         XDG_WM_BASE_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to xdg_wm_base_interface\n");
  }

  else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
    state->wp_viewporter =
        wl_registry_bind(wl_registry, name, &wp_viewporter_interface,
                         WP_VIEWPORTER_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wp_viewporter_interface\n");
  }
}

comptime struct wl_registry_listener registry_listener = {
    .global = registry_handle,
};

/*****************************************************************
 * starting point
 *****************************************************************/

#define OP_WAYLAND (1 << 0)
#define OP_INOTIFY_WATCH (1 << 1)
#define OP_JOYSTICK_POLL (1 << 2)
#define OP_JOYSTICK_READ (1 << 3)

struct op {
  u8 type;
  int fd;
};

struct op_joystick_read {
  u8 type;
  int fd;
  struct input_event event;
};

#define ACTION_ADD (1 << 0)
#define ACTION_REMOVE (1 << 1)

static inline u8 libevdev_is_joystick(struct libevdev *evdev) {
  return libevdev_has_event_type(evdev, EV_ABS) &&
         libevdev_has_event_code(evdev, EV_ABS, ABS_RX);
}

int main(int argc, char *argv[]) {
  int error_code = 0;
  struct linux_state state = {
      .running = 1,
  };

  /* game: hot reload game code on debug version */
  state.lib = &(struct game_code){};
#if HANDMADEHERO_DEBUG
  {
    /* assumes libhandmadehero.so in same directory as executable */
    static const char libpath[] = "libhandmadehero.so";
    static const u64 libpath_length = sizeof(libpath) - 1;
    char *exepath = argv[0];
    char *output = state.lib->path;
    u64 index = 0;
    for (char *c = exepath; *c; c++) {
      if (*c == '/')
        index = (u64)(c - exepath);
    }
    u64 length = index;
    memcpy(output, exepath, length);
    output[length++] = '/';
    memcpy(output + length, libpath, libpath_length);
    length += libpath_length;
    assert(length < 255);
  }

  ReloadGameCode(state.lib);
#else
  state.lib->GameUpdateAndRender = GameUpdateAndRender;
#endif

  /* xkb */
  state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!state.xkb_context) {
    error_code = HANDMADEHERO_ERROR_XKB;
    goto exit;
  }

  /* game: backbuffer */
  {
    /*
     *  960x540x4 ~1.10M single, ~3.98M with double buffering
     * 1280x720x4 ~3.53M single, ~7.32M with double buffering
     */
    static struct game_backbuffer backbuffer = {
        .width = 960,
        .height = 540,
        .bytes_per_pixel = 4,
    };
    backbuffer.stride = backbuffer.width * backbuffer.bytes_per_pixel;
    state.backbuffer = &backbuffer;
  }

  /* wayland */
  struct wl_display *wl_display = wl_display_connect(0);
  if (!wl_display) {
    fprintf(stderr, "error: cannot connect wayland display!\n");
    error_code = HANDMADEHERO_ERROR_WAYLAND_CONNECT;
    goto xkb_context_exit;
  }

  struct wl_registry *registry = wl_display_get_registry(wl_display);
  if (!registry) {
    fprintf(stderr, "error: cannot get registry!\n");
    error_code = HANDMADEHERO_ERROR_WAYLAND_REGISTRY;
    goto wl_exit;
  }

  /* wayland: get globals */
  wl_registry_add_listener(registry, &registry_listener, &state);
  wl_display_roundtrip(wl_display);

  debugf("backbuffer: @%p\n", state.backbuffer);
  debugf("memory: @%p\n", state.memory);
  debugf("wl_display: @%p\n", wl_display);
  debugf("wl_registry: @%p\n", registry);
  debugf("wl_compositor: @%p\n", state.wl_compositor);
  debugf("wl_shm: @%p\n", state.wl_shm);
  debugf("wl_seat: @%p\n", state.wl_seat);
  debugf("xdg_wm_base: @%p\n", state.xdg_wm_base);

  if (!state.wl_compositor || !state.wl_shm || !state.wl_seat ||
      !state.xdg_wm_base || !state.wp_viewporter) {
    fprintf(stderr, "error: cannot get wayland globals!\n");
    error_code = HANDMADEHERO_ERROR_WAYLAND_EXTENSIONS;
    goto wl_exit;
  }

  /* wayland: create surface */
  state.wl_surface = wl_compositor_create_surface(state.wl_compositor);

  /* wayland: application window */
  xdg_wm_base_add_listener(state.xdg_wm_base, &xdg_wm_base_listener, &state);

  state.xdg_surface =
      xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);

  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  xdg_toplevel_set_title(state.xdg_toplevel, "handmadehero");
  xdg_toplevel_set_maximized(state.xdg_toplevel);
  xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
  xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
  wl_surface_commit(state.wl_surface);

  /* wayland: register frame callback */
  struct wl_callback *frame_callback = wl_surface_frame(state.wl_surface);
  wl_callback_add_listener(frame_callback, &wl_surface_frame_listener, &state);

  wl_seat_add_listener(state.wl_seat, &wl_seat_listener, &state);

  /* game: mem allocation */
  static struct game_memory game_memory;
  if (game_memory_allocation(&game_memory, 8 * MEGABYTES, 2 * MEGABYTES)) {
    fprintf(stderr, "error: cannot allocate memory!\n");
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto wl_exit;
  }
  state.game_memory = &game_memory;

  if (mem_alloc(&state.memory, 4 * MEGABYTES)) {
    fprintf(stderr, "error: cannot allocate memory!\n");
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto wl_exit;
  }

  /* wayland: create buffer */
  u32 backbuffer_multiplier = 1;
  u32 backbuffer_size = state.backbuffer->height * state.backbuffer->stride *
                        backbuffer_multiplier;

  i32 shm_fd = create_shared_memory(backbuffer_size);
  if (shm_fd == 0) {
    fprintf(stderr, "error: cannot create shared memory!\n");
    error_code = HANDMADEHERO_ERROR_SHARED_MEMORY;
    goto wl_exit;
  }

  state.backbuffer->memory = mem_push(&state.memory, (size_t)backbuffer_size);
  state.backbuffer->memory =
      mmap(state.backbuffer->memory, (size_t)backbuffer_size,
           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, shm_fd, 0);
  if (state.backbuffer->memory == MAP_FAILED) {
    fprintf(stderr, "error: cannot create shared memory!\n");
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto shm_exit;
  }

  struct wl_shm_pool *pool =
      wl_shm_create_pool(state.wl_shm, shm_fd, (i32)backbuffer_size);
  if (!pool) {
    wl_shm_pool_destroy(pool);
    error_code = HANDMADEHERO_ERROR_WAYLAND_SHM_POOL;
    goto shm_exit;
  }
  state.wl_buffer = wl_shm_pool_create_buffer(
      pool, 0, (i32)state.backbuffer->width, (i32)state.backbuffer->height,
      (i32)state.backbuffer->stride, WL_SHM_FORMAT_XRGB8888);
  if (!state.wl_buffer) {
    wl_shm_pool_destroy(pool);
    error_code = HANDMADEHERO_ERROR_WAYLAND_SHM_POOL;
    goto shm_exit;
  }
  wl_surface_attach(state.wl_surface, state.wl_buffer, 0, 0);
  wl_shm_pool_destroy(pool);

  /* io_uring */
  struct io_uring_sqe *sqe;
  struct io_uring ring;
  if (io_uring_queue_init(4, &ring, 0)) {
    error_code = HANDMADEHERO_ERROR_IO_URING_SETUP;
    goto shm_exit;
  }

  /* io_uring: poll on wl_display */
  int fd_wl_display = wl_display_get_fd(wl_display);
  if (fd_wl_display < 0) {
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto io_uring_exit;
  }

  sqe = io_uring_get_sqe(&ring);
  io_uring_prep_poll_multishot(sqe, fd_wl_display, POLLIN);
  io_uring_sqe_set_data(sqe,
                        &(struct op){.type = OP_WAYLAND, .fd = fd_wl_display});

  /* inotify: notify when a new input added */
  int fd_inotify = inotify_init1(IN_NONBLOCK);
  if (fd_inotify < 0) {
    error_code = HANDMADEHERO_ERROR_INOTIFY_SETUP;
    goto io_uring_exit;
  }

  int fd_watch =
      inotify_add_watch(fd_inotify, "/dev/input", IN_CREATE | IN_DELETE);
  if (fd_watch < 0) {
    error_code = HANDMADEHERO_ERROR_INOTIFY_WATCH_SETUP;
    goto inotify_exit;
  }

  sqe = io_uring_get_sqe(&ring);
  struct op *op = &(struct op){.type = OP_INOTIFY_WATCH, .fd = fd_inotify};
  io_uring_prep_poll_multishot(sqe, op->fd, POLLIN);
  io_uring_sqe_set_data(sqe, op);

  /* add already connected joysticks to queue */
  DIR *dir = opendir("/dev/input");
  if (dir == 0) {
    error_code = HANDMADEHERO_ERROR_DEV_INPUT_DIR_OPEN;
    goto inotify_watch_exit;
  }

  struct dirent *dirent;
  u32 dirent_max = 1024;
  while (dirent_max--) {
    errno = 0;
    dirent = readdir(dir);

    /* error occured */
    if (errno != 0) {
      error_code = HANDMADEHERO_ERROR_DEV_INPUT_DIR_READ;
      closedir(dir);
      goto inotify_watch_exit;
    }

    /* end of directory stream is reached */
    if (dirent == 0)
      break;

    if (dirent->d_type != DT_CHR)
      continue;

    /* get full path */
    char path[32] = "/dev/input/";
    for (char *dest = path + 11, *src = dirent->d_name; *src; src++, dest++) {
      *dest = *src;
    }

    struct op_joystick_read *op = &(struct op_joystick_read){
        .type = OP_JOYSTICK_READ,
    };

    op->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (op->fd < 0)
      continue;

    struct libevdev *evdev;
    int rc = libevdev_new_from_fd(op->fd, &evdev);
    if (rc < 0) {
      debug("libevdev failed\n");
      close(op->fd);
      if (evdev)
        libevdev_free(evdev);
      continue;
    }

    /* detect joystick */
    if (!libevdev_is_joystick(evdev)) {
      close(op->fd);
      libevdev_free(evdev);
      continue;
    }

    debugf("Input device name: \"%s\"\n", libevdev_get_name(evdev));
    debugf("Input device ID: bus %#x vendor %#x product %#x\n",
           libevdev_get_id_bustype(evdev), libevdev_get_id_vendor(evdev),
           libevdev_get_id_product(evdev));

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, op->fd, &op->event, sizeof(op->event), 0);
    io_uring_sqe_set_data(sqe, op);

    libevdev_free(evdev);
  }
  closedir(dir);
  /* submit any work */
  io_uring_submit(&ring);

  /* event loop */
  struct io_uring_cqe *cqe;
  while (state.running) {
    while (wl_display_prepare_read(wl_display) != 0)
      wl_display_dispatch_pending(wl_display);
    wl_display_flush(wl_display);

    int error = io_uring_wait_cqe(&ring, &cqe);
    if (error) {
      error_code = HANDMADEHERO_ERROR_IO_URING_WAIT;
      break;
    }

    struct op *op = io_uring_cqe_get_data(cqe);
    if (op == 0)
      goto cqe_seen;

    /* on wayland events */
    if (op->type & OP_WAYLAND) {
      int revents = cqe->res;

      if (revents & POLLIN) {
        wl_display_read_events(wl_display);
      } else {
        wl_display_cancel_read(wl_display);
      }
    }

    /* on inotify events */
    if (op->type & OP_INOTIFY_WATCH) {
      wl_display_cancel_read(wl_display);

      /* on inotify watch error, finish the program */
      if (cqe->res < 0) {
        debug("inotify watch\n");
        error_code = HANDMADEHERO_ERROR_INOTIFY_WATCH;
        break;
      }

      int revents = cqe->res;
      if (!(revents & POLLIN)) {
        error_code = HANDMADEHERO_ERROR_INOTIFY_WATCH_POLL;
        break;
      }

      /* note: reading one inotify_event (16 bytes) fails */
      u8 buf[32];
      ssize_t readBytes = read(op->fd, buf, sizeof(buf));
      if (readBytes < 0) {
        goto cqe_seen;
      }

      struct inotify_event *event = (struct inotify_event *)buf;
      if (event->len <= 0)
        goto cqe_seen;

      u8 action = 0;
      if (event->mask & IN_CREATE)
        action = ACTION_ADD;
      else if (event->mask & IN_DELETE)
        action = ACTION_REMOVE;

      if (event->mask & IN_ISDIR)
        goto cqe_seen;

      /* get full path */
      char path[32] = "/dev/input/";
      for (char *dest = path + 11, *src = event->name; *src; src++, dest++) {
        *dest = *src;
      }

      if (action & ACTION_REMOVE)
        goto cqe_seen;

      struct op_joystick_read *op = &(struct op_joystick_read){
          .type = OP_JOYSTICK_READ,
      };

      /* try loop for opening file until is ready.
       *
       * if we try to open file that is not ready,
       * we get errno 13 EACCESS.
       */
      u32 try = 1 << 17;
      while (1) {
        if (try == 0) {
          debug("open failed\n");
          goto cqe_seen;
        }

        op->fd = open(path, O_RDONLY | O_NONBLOCK);
        if (op->fd >= 0) {
          break;
        }

        try--;
      }

      struct libevdev *evdev;
      int rc = libevdev_new_from_fd(op->fd, &evdev);
      if (rc < 0) {
        debug("libevdev failed\n");
        close(op->fd);
        if (evdev)
          libevdev_free(evdev);
        goto cqe_seen;
      }

      /* detect joystick */
      if (!libevdev_is_joystick(evdev)) {
        debug("This device does not look like a joystick\n");
        close(op->fd);
        libevdev_free(evdev);
        goto cqe_seen;
      }

      printf("Input device name: \"%s\"\n", libevdev_get_name(evdev));
      printf("Input device ID: bus %#x vendor %#x product %#x\n",
             libevdev_get_id_bustype(evdev), libevdev_get_id_vendor(evdev),
             libevdev_get_id_product(evdev));

      sqe = io_uring_get_sqe(&ring);
      io_uring_prep_read(sqe, op->fd, &op->event, sizeof(op->event), 0);
      io_uring_sqe_set_data(sqe, op);
      io_uring_submit(&ring);

      libevdev_free(evdev);
    }

    /* on joystick events */
    else if (op->type & OP_JOYSTICK_READ) {
      wl_display_cancel_read(wl_display);
      /* on joystick read error (eg. joystick removed), close the fd */
      if (cqe->res < 0) {
        /* TODO: when joystick disconnected reset controller */
        if (op->fd >= 0)
          close(op->fd);
        goto cqe_seen;
      }

      struct op_joystick_read *op = io_uring_cqe_get_data(cqe);
      struct input_event *event = &op->event;

      joystick_key(&state, event->type, event->code, event->value);

      sqe = io_uring_get_sqe(&ring);
      io_uring_prep_read(sqe, op->fd, &op->event, sizeof(op->event), 0);
      io_uring_sqe_set_data(sqe, op);
      io_uring_submit(&ring);
    } /* on joystick events finish */

  cqe_seen:
    io_uring_cqe_seen(&ring, cqe);
  }

  /* finished */
inotify_watch_exit:
  close(fd_watch);

inotify_exit:
  close(fd_inotify);

io_uring_exit:
  io_uring_queue_exit(&ring);

shm_exit:
  close(shm_fd);

wl_exit:
  wl_display_disconnect(wl_display);

xkb_context_exit:
  xkb_context_unref(state.xkb_context);

exit:
  return error_code;
}
