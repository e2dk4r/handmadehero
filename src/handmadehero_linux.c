/* system */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

/* generated */
#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/* handmadehero */
#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/handmadehero.h>

#include "joystick_linux.h"

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

  struct Joystick joysticks[2];
  u8 joystickCount;

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

static void joystick_event(struct linux_state *state, u16 type, u16 code,
                           i32 value) {
  struct game_controller_input *controller = GetController(state->input, 1);

  debugf("joystick_event time: type: %d code: %d value: %d\n", type, code,
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
    f32 x = (f32)value / max;
    controller->stickAverageX = x;
  }

  // normal of Y values
  else if (code == ABS_Y) {
    static const f32 MAX_LEFT = 32768.0f;
    static const f32 MAX_RIGHT = 32767.0f;
    f32 max = value < 0 ? MAX_LEFT : MAX_RIGHT;
    f32 y = (f32)value / max;
    controller->stickAverageY = y;
  }

  else if (code == BTN_NORTH) {
    controller->actionUp.pressed = (u8)value;
  }

  else if (code == BTN_SOUTH) {
    controller->actionDown.pressed = (u8)value;
  }

  else if (code == BTN_WEST) {
    controller->actionLeft.pressed = (u8)value;
  }

  else if (code == BTN_EAST) {
    controller->actionRight.pressed = (u8)value;
  }
}

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                               u32 format, i32 fd, u32 size) {
  struct linux_state *state = data;
  debugf("[wl_keyboard::keymap] format: %d fd: %d size: %d\n", format, fd,
         size);

  char *keymap_str = mem_push(&state->memory, size);
  keymap_str =
      mmap(keymap_str, size, PROT_READ, MAP_FIXED | MAP_PRIVATE, fd, 0);
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

  case 'L':
  case 'l': {
    /* if key is not pressed, exit */
    if (keystate == WL_KEYBOARD_KEY_STATE_RELEASED)
      break;

    /* if key is pressed, toggle between record and playback */
    if (!RecordInputStarted(state)) {
      PlaybackInputEnd(state);
      RecordInputBegin(state, 1);
    } else {
      RecordInputEnd(state);
      PlaybackInputBegin(state, 1);
    }
  } break;

  case 'A':
  case 'a': {
    assert(controller->moveLeft.pressed != keystate);
    controller->moveLeft.pressed = (u8)keystate;
  } break;

  case 'D':
  case 'd': {
    assert(controller->moveRight.pressed != keystate);
    controller->moveRight.pressed = (u8)keystate;
  } break;

  case 'W':
  case 'w': {
    assert(controller->moveUp.pressed != keystate);
    controller->moveUp.pressed = (u8)keystate;
  } break;

  case 'S':
  case 's': {
    assert(controller->moveDown.pressed != keystate);
    controller->moveDown.pressed = (u8)keystate;
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

  f32 frame_unit = (f32)elapsed * (second_in_milliseconds / frames_per_second);
  if (frame_unit > 1.0f) {
    state->input = newInput;

    if (RecordInputStarted(state)) {
      RecordInput(state, state->input);
    }

    if (PlaybackInputStarted(state)) {
      PlaybackInput(state, state->input);
    }

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
int main(int argc, char *argv[]) {
  struct linux_state state = {
      .running = 1,
  };

  /* game code */
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
  {
    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    /* ~3.53M single, ~7.32M with double buffering */
    static struct game_backbuffer backbuffer = {
        .width = 1280,
        .height = 720,
        .bytes_per_pixel = 4,
    };
    backbuffer.stride = backbuffer.width * backbuffer.bytes_per_pixel;
    state.backbuffer = &backbuffer;
  }

  int error_code = 0;

  struct wl_display *wl_display = wl_display_connect(0);
  if (!wl_display) {
    fprintf(stderr, "error: cannot connect wayland display!\n");
    error_code = 1;
    goto exit;
  }

  struct wl_registry *registry = wl_display_get_registry(wl_display);
  if (!registry) {
    fprintf(stderr, "error: cannot get registry!\n");
    error_code = 2;
    goto wl_exit;
  }

  /* get globals */
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
    error_code = 3;
    goto wl_exit;
  }

  /* create surface */
  state.wl_surface = wl_compositor_create_surface(state.wl_compositor);

  /* application window */
  xdg_wm_base_add_listener(state.xdg_wm_base, &xdg_wm_base_listener, &state);

  state.xdg_surface =
      xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);

  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  xdg_toplevel_set_title(state.xdg_toplevel, "handmadehero");
  xdg_toplevel_set_maximized(state.xdg_toplevel);
  xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
  xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
  wl_surface_commit(state.wl_surface);

  /* register frame callback */
  struct wl_callback *frame_callback = wl_surface_frame(state.wl_surface);
  wl_callback_add_listener(frame_callback, &wl_surface_frame_listener, &state);

  wl_seat_add_listener(state.wl_seat, &wl_seat_listener, &state);

  /* mem allocation */
  static struct game_memory game_memory;
  if (game_memory_allocation(&game_memory, 8 * MEGABYTES, 2 * MEGABYTES)) {
    fprintf(stderr, "error: cannot allocate memory!\n");
    error_code = 4;
    goto wl_exit;
  }
  state.game_memory = &game_memory;

  if (mem_alloc(&state.memory, 8 * MEGABYTES)) {
    fprintf(stderr, "error: cannot allocate memory!\n");
    error_code = 4;
    goto wl_exit;
  }

  /* create buffer */
  u32 backbuffer_multiplier = 2;
  u32 backbuffer_size = state.backbuffer->height * state.backbuffer->stride *
                        backbuffer_multiplier;

  i32 shm_fd = create_shared_memory(backbuffer_size);
  if (shm_fd == 0) {
    fprintf(stderr, "error: cannot create shared memory!\n");
    error_code = 5;
    goto wl_exit;
  }

  state.backbuffer->memory = mem_push(&state.memory, (size_t)backbuffer_size);
  state.backbuffer->memory =
      mmap(state.backbuffer->memory, (size_t)backbuffer_size,
           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, shm_fd, 0);
  if (state.backbuffer->memory == MAP_FAILED) {
    fprintf(stderr, "error: cannot create shared memory!\n");
    error_code = 6;
    goto shm_exit;
  }

  struct wl_shm_pool *pool =
      wl_shm_create_pool(state.wl_shm, shm_fd, (i32)backbuffer_size);
  state.wl_buffer = wl_shm_pool_create_buffer(
      pool, 0, (i32)state.backbuffer->width, (i32)state.backbuffer->height,
      (i32)state.backbuffer->stride, WL_SHM_FORMAT_XRGB8888);
  wl_surface_attach(state.wl_surface, state.wl_buffer, 0, 0);
  wl_shm_pool_destroy(pool);

  struct pollfd fds[3];
  u8 fdsCount = 1;
  int fd_wl_display = wl_display_get_fd(wl_display);
  fds[0].fd = fd_wl_display;
  fds[0].events = POLLIN;

  int hasJoystick = enumarateJoysticks(&state.joystickCount, 0);
  debugf("has joystick: %d %d\n", hasJoystick, state.joystickCount);
  if (hasJoystick && state.joystickCount > 0) {
    if (state.joystickCount > 2)
      state.joystickCount = 2;
    enumarateJoysticks(&state.joystickCount,
                       (struct Joystick *)&state.joysticks);

    for (u8 joystickIndex = 0; joystickIndex < state.joystickCount;
         joystickIndex++) {
      struct Joystick *joystick = &state.joysticks[joystickIndex];
      joystick->fd = open(joystick->path, O_RDONLY);
      if (joystick->fd < 0) {
        fprintf(stderr, "error: cannot open joystick %s\n", joystick->path);
        error_code = 7;
        goto joystick_exit;
      }

      fds[1 + joystickIndex].fd = joystick->fd;
      fds[1 + joystickIndex].events = POLLIN;
      fdsCount++;
    }
  }

  /* main loop */
  while (state.running) {
    while (wl_display_prepare_read(wl_display) != 0)
      wl_display_dispatch_pending(wl_display);
    wl_display_flush(wl_display);

    int ret = ppoll(fds, fdsCount, 0, 0);
    if (ret < 0)
      break;

    if (fds[0].revents & POLLIN) {
      wl_display_read_events(wl_display);
    } else {
      wl_display_cancel_read(wl_display);
    }

    for (u8 joystickIndex = 0; joystickIndex < state.joystickCount;
         joystickIndex++) {

      if (!(fds[1 + joystickIndex].revents & POLLIN)) {
        continue;
      }

      struct Joystick *joystick = &state.joysticks[joystickIndex];
      struct input_event event = {0};
      ssize_t bytesRead = read(joystick->fd, &event, sizeof(event));
      assert(bytesRead > 0);

      joystick_event(&state, event.type, event.code, event.value);
    }
  }

  /* finished */
joystick_exit:
  for (u8 joystickIndex = 0; joystickIndex < state.joystickCount;
       joystickIndex++) {
    struct Joystick *joystick = &state.joysticks[joystickIndex];
    if (joystick->fd >= 0) {
      close(joystick->fd);
    }
  }
shm_exit:
  close(shm_fd);

wl_exit:
  wl_display_disconnect(wl_display);

exit:
  return error_code;
}
