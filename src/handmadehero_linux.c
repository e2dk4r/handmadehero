#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/handmadehero.h>

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

privatefn void *mem_push(struct mem *mem, size_t size) {
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
privatefn uint8_t mem_alloc(struct mem *mem, size_t len) {
  mem->capacity = len;
  mem->current = 0;
  mem->data =
      mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  uint8_t is_allocation_failed = mem->data == (void *)-1;
  return is_allocation_failed;
}

/*****************************************************************
 * structures
 *****************************************************************/
struct my_state {
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

  struct mem memory;
  struct game_backbuffer *backbuffer;

  u32 frame;
  u8 running : 1;
};

privatefn void draw_frame(struct game_backbuffer *backbuffer, int offsetX,
                          int offsetY) {
  u8 *row = backbuffer->memory;
  for (u32 y = 0; y < backbuffer->height; y++) {
    u8 *pixel = row;
    for (u32 x = 0; x < backbuffer->width; x++) {
      *pixel = (u8)(x + offsetX);
      pixel++;

      *pixel = (u8)(y + offsetY);
      pixel++;

      *pixel = 0x00;
      pixel++;

      *pixel = 0x00;
      pixel++;
    }
    row += backbuffer->stride;
  }
}

/*****************************************************************
 * input handling
 *****************************************************************/

privatefn void global_wl_keyboard_keymap_handle(void *data,
                                                struct wl_keyboard *wl_keyboard,
                                                u32 format, i32 fd, u32 size) {
  struct my_state *state = data;
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

privatefn void global_wl_keyboard_key_handle(void *data,
                                             struct wl_keyboard *wl_keyboard,
                                             u32 serial, u32 time, u32 key,
                                             u32 state) {
  struct my_state *client_state = data;
  debugf("[wl_keyboard::key] serial: %d time: %d key: %d state: %d\n", serial,
         time, key, state);

  u32 keycode = key + 8;
  xkb_keysym_t keysym =
      xkb_state_key_get_one_sym(client_state->xkb_state, keycode);

  char buf[128];
  xkb_keysym_get_name(keysym, buf, sizeof(buf));
  debugf("[wl_keyboard::key] keysym: %s\n", buf);

  if (keysym == XKB_KEY_q) {
    client_state->running = 0;
  }
}

privatefn void global_wl_keyboard_modifiers_handle(
    void *data, struct wl_keyboard *wl_keyboard, u32 serial, u32 mods_depressed,
    u32 mods_latched, u32 mods_locked, u32 group) {
  struct my_state *state = data;
  debugf("[wl_keyboard::modifiers] serial: %d depressed: %d latched: %d "
         "locked: %d group: %d\n",
         serial, mods_depressed, mods_latched, mods_locked, group);

  xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

privatefn void global_wl_keyboard_repeat_info_handle(
    void *data, struct wl_keyboard *wl_keyboard, i32 rate, i32 delay) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::repeat_info] rate: %d delay: %d\n", rate, delay);
}

privatefn void global_wl_keyboard_enter_handle(void *data,
                                               struct wl_keyboard *wl_keyboard,
                                               uint32_t serial,
                                               struct wl_surface *surface,
                                               struct wl_array *keys) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::enter] serial: %d\n", serial);
}

privatefn void global_wl_keyboard_leave_handle(void *data,
                                               struct wl_keyboard *wl_keyboard,
                                               uint32_t serial,
                                               struct wl_surface *surface) {
  // struct my_state *state = data;
  debugf("[wl_keyboard::leave] serial: %d\n", serial);
}

comptime struct wl_keyboard_listener global_wl_keyboard_listener = {
    .enter = global_wl_keyboard_enter_handle,
    .leave = global_wl_keyboard_leave_handle,
    .keymap = global_wl_keyboard_keymap_handle,
    .key = global_wl_keyboard_key_handle,
    .modifiers = global_wl_keyboard_modifiers_handle,
    .repeat_info = global_wl_keyboard_repeat_info_handle,
};

privatefn void global_wl_seat_capabilities_handle(void *data,
                                                  struct wl_seat *wl_seat,
                                                  uint32_t capabilities) {
  struct my_state *state = data;
  debugf("[wl_seat::capabilities] capabilities: %d\n", capabilities);

  uint8_t have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  if (have_keyboard && !state->wl_keyboard) {
    state->wl_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(state->wl_keyboard, &global_wl_keyboard_listener,
                             state);
  } else if (!have_keyboard && state->wl_keyboard) {
    wl_keyboard_release(state->wl_keyboard);
    state->wl_keyboard = 0;
  }
}

privatefn void global_wl_seat_name_handle(void *data, struct wl_seat *wl_seat,
                                          const char *name) {
  debugf("[wl_seat::name] name: %s\n", name);
}

comptime struct wl_seat_listener global_wl_seat_listener = {
    .capabilities = global_wl_seat_capabilities_handle,
    .name = global_wl_seat_name_handle,
};

/*****************************************************************
 * shared memory
 *****************************************************************/
privatefn i32 create_shared_memory(off_t size) {
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
static void global_wl_surface_frame_done_handle(void *data,
                                                struct wl_callback *wl_callback,
                                                u32 time);

static const struct wl_callback_listener global_wl_surface_frame_listener = {
    .done = global_wl_surface_frame_done_handle,
};

static void global_wl_surface_frame_done_handle(void *data,
                                                struct wl_callback *wl_callback,
                                                u32 time) {
  wl_callback_destroy(wl_callback);
  struct my_state *state = data;

  wl_callback = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(wl_callback, &global_wl_surface_frame_listener,
                           data);

  u32 elapsed = time - state->frame;
  f32 second_in_milliseconds = 1000;
  f32 frames_per_second = 24;

  f32 frame_unit = (f32)elapsed * (second_in_milliseconds / frames_per_second);
  static i32 offsetX = 0;
  if (frame_unit > 1.0f) {
    draw_frame(state->backbuffer, offsetX, 0);
    offsetX++;

    wl_surface_attach(state->wl_surface, state->wl_buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0,
                             (i32)state->backbuffer->width,
                             (i32)state->backbuffer->height);

    state->frame = time;
  }

  wl_surface_commit(state->wl_surface);
}

/*****************************************************************
 * xdg_wm_base events
 *****************************************************************/
privatefn void global_xdg_wm_base_ping_handle(void *data,
                                              struct xdg_wm_base *xdg_wm_base,
                                              u32 serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
  debugf("[xdg_wm_base::ping] pong(serial: %d)\n", serial);
}

static const struct xdg_wm_base_listener global_xdg_wm_base_listener = {
    .ping = global_xdg_wm_base_ping_handle,
};

/*****************************************************************
 * xdg_toplevel events
 *****************************************************************/

privatefn void global_xdg_toplevel_configure_handle(
    void *data, struct xdg_toplevel *xdg_toplevel, i32 screen_width,
    i32 screen_height, struct wl_array *states) {
  debugf("[xdg_toplevel::configure] screen width: %d height: %d\n",
         screen_width, screen_height);
  if (screen_width == 0 || screen_height == 0)
    return;

  struct my_state *state = data;

  if (state->wp_viewport) {
    wp_viewport_destroy(state->wp_viewport);
    state->wp_viewport = 0;
  }

  state->wp_viewport =
      wp_viewporter_get_viewport(state->wp_viewporter, state->wl_surface);
  wp_viewport_set_destination(state->wp_viewport, screen_width, screen_height);

  wl_surface_commit(state->wl_surface);
}

privatefn void
global_xdg_toplevel_close_handle(void *data,
                                 struct xdg_toplevel *xdg_toplevel) {
  struct my_state *state = data;
  state->running = 0;
}

comptime struct xdg_toplevel_listener global_xdg_toplevel_listener = {
    .configure = global_xdg_toplevel_configure_handle,
    .close = global_xdg_toplevel_close_handle,
};

/*****************************************************************
 * xdg_surface events
 *****************************************************************/
privatefn void
global_xdg_surface_configure_handle(void *data, struct xdg_surface *xdg_surface,
                                    u32 serial) {
  struct my_state *state = data;

  /* ack */
  xdg_surface_ack_configure(xdg_surface, serial);
  debugf("[xdg_surface::configure] ack_configure(serial: %d)\n", serial);

  wl_surface_attach(state->wl_surface, state->wl_buffer, 0, 0);
  wl_surface_commit(state->wl_surface);
}

comptime struct xdg_surface_listener global_xdg_surface_listener = {
    .configure = global_xdg_surface_configure_handle,
};

/*****************************************************************
 * wl_registry events
 *****************************************************************/

#define WL_COMPOSITOR_MINIMUM_REQUIRED_VERSION 4
#define WL_SHM_MINIMUM_REQUIRED_VERSION 1
#define WL_SEAT_MINIMUM_REQUIRED_VERSION 6
#define XDG_WM_BASE_MINIMUM_REQUIRED_VERSION 2
#define WP_VIEWPORTER_MINIMUM_REQUIRED_VERSION 1

privatefn void global_registry_handle(void *data,
                                      struct wl_registry *wl_registry, u32 name,
                                      const char *interface, u32 version) {
  struct my_state *state = data;

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

privatefn void global_registry_remove_handle(void *data,
                                             struct wl_registry *wl_registry,
                                             u32 name) {}
comptime struct wl_registry_listener global_registry_listener = {
    .global = global_registry_handle,
    .global_remove = global_registry_remove_handle,
};

/*****************************************************************
 * starting point
 *****************************************************************/
int main() {
  struct my_state state = {
      .running = 1,
  };

  {
    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    struct game_backbuffer backbuffer = {
        .width = 1280,
        .height = 720,
        .bytes_per_pixel = 4,
    };
    backbuffer.stride = backbuffer.width * backbuffer.bytes_per_pixel;
    state.backbuffer = &backbuffer;
  }

  int error_code = 0;

  struct wl_display *display = wl_display_connect(0);
  if (!display) {
    fprintf(stderr, "error: cannot connect wayland display!\n");
    error_code = 1;
    goto exit;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  if (!registry) {
    fprintf(stderr, "error: cannot get registry!\n");
    error_code = 2;
    goto wl_exit;
  }

  /* get globals */
  wl_registry_add_listener(registry, &global_registry_listener, &state);
  wl_display_roundtrip(display);

  debugf("backbuffer: @%p\n", state.backbuffer);
  debugf("memory: @%p\n", state.memory);
  debugf("wl_display: @%p\n", display);
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
  xdg_wm_base_add_listener(state.xdg_wm_base, &global_xdg_wm_base_listener,
                           &state);

  state.xdg_surface =
      xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);

  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  xdg_toplevel_set_title(state.xdg_toplevel, "handmadehero");
  xdg_toplevel_set_maximized(state.xdg_toplevel);
  xdg_toplevel_add_listener(state.xdg_toplevel, &global_xdg_toplevel_listener,
                            &state);
  xdg_surface_add_listener(state.xdg_surface, &global_xdg_surface_listener,
                           &state);
  wl_surface_commit(state.wl_surface);

  /* register frame callback */
  struct wl_callback *frame_callback = wl_surface_frame(state.wl_surface);
  wl_callback_add_listener(frame_callback, &global_wl_surface_frame_listener,
                           &state);

  wl_seat_add_listener(state.wl_seat, &global_wl_seat_listener, &state);

  /* mem allocation */
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

  /* evloop */
  while (wl_display_dispatch(display) && state.running) {
  }

  /* finished */
shm_exit:
  close(shm_fd);

wl_exit:
  wl_display_disconnect(display);

exit:
  return error_code;
}
