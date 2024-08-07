#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

/* system */
#pragma GCC diagnostic push

// caused by: #include <pipewire/pipewire.h>
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <liburing.h>
#include <linux/input.h>
#include <pipewire/pipewire.h>
#include <pthread.h>
#include <semaphore.h>
#include <spa/param/audio/format-utils.h>
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

#if HANDMADEHERO_DEBUG
#include <dlfcn.h>
#endif

#define POLLIN 0x001 /* There is data to read.  */

/* generated */
#include "content-type-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#pragma GCC diagnostic pop

/* handmadehero */
#include <handmadehero/assert.h>
#include <handmadehero/debug.h>
#include <handmadehero/errors.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/platform.h>
#include <handmadehero/text.h>

#define PAUSE_WHEN_SURFACE_OUT_OF_FOCUS 0
#define RESOLUTION 1080
#define DISABLE_SURFACE_SCALING 0

/*****************************************************************
 * platform layer implementation
 *****************************************************************/

struct linux_file_handle {
  s64 lastError;
  s32 fd;
};

struct linux_file_group {
  DIR *dir;
  long direntIndexes[1024];
  u32 fileIndex;
};

struct linux_memory_block {
  u64 size;
};

void *
LinuxAllocateMemory(u64 size)
{
  u64 total = size + sizeof(struct linux_memory_block);
  struct linux_memory_block *memoryBlock = mmap(0, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(memoryBlock != MAP_FAILED);

  memoryBlock->size = total;
  void *memory = memoryBlock + 1;

  return memory;
}

void
LinuxDeallocateMemory(void *memory)
{
  if (!memory)
    return;

  struct linux_memory_block *memoryBlock = memory - sizeof(*memoryBlock);

  // TODO: unmap failed?
  munmap(memoryBlock, memoryBlock->size);
  memory = 0;
}

struct platform_file_handle
LinuxOpenNextFile(struct platform_file_group *platformFileGroup)
{
  struct platform_file_handle platformFileHandle = {};
  struct linux_file_group *fileGroup = platformFileGroup->data;

  struct linux_file_handle *fileHandle =
      mmap(0, sizeof(*fileHandle), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!fileHandle) {
    return platformFileHandle;
  }
  platformFileHandle.data = fileHandle;

  long direntIndex = fileGroup->direntIndexes[fileGroup->fileIndex];
  seekdir(fileGroup->dir, direntIndex);
  struct dirent *dirent = readdir(fileGroup->dir);
  assert(dirent != 0);

  char *path = dirent->d_name;
  fileGroup->fileIndex++;

  s32 fd = open(path, O_RDONLY);
  if (fd < 0) {
    platformFileHandle.error = HANDMADEHERO_ERROR_OPEN_FILE;
    fileHandle->lastError = errno;
    goto onError;
  }

  struct stat stat;
  s32 fstatResult = fstat(fd, &stat);
  if (fstatResult != 0) {
    platformFileHandle.error = HANDMADEHERO_ERROR_FILE_STAT;
    fileHandle->lastError = errno;
    goto onError;
  }

  if (!S_ISREG(stat.st_mode)) {
    platformFileHandle.error = HANDMADEHERO_ERROR_PATH_IS_NOT_FILE;
    goto onError;
  }

  platformFileHandle.error = HANDMADEHERO_ERROR_NONE;
  fileHandle->fd = fd;

  return platformFileHandle;

onError:
  // TODO: unmap failed?
  munmap(fileHandle, sizeof(*fileHandle));

  if (fd >= 0) {
    // TODO: is file closed?
    close(fd);
  }
  return platformFileHandle;
}

void
LinuxReadFromFile(void *dest, struct platform_file_handle *platformFileHandle, u64 offset, u64 size)
{
  // TODO: use io_uring instead of locking
  struct linux_file_handle *fileHandle = platformFileHandle->data;

  global_variable pthread_mutex_t fileReadLock;
  pthread_mutex_lock(&fileReadLock);

  u64 seekTry = 100;
  while (seekTry) {
    off64_t lseekResult = lseek64(fileHandle->fd, (off64_t)offset, SEEK_SET);
    if (lseekResult == -1 && errno != EAGAIN) {
      platformFileHandle->error = HANDMADEHERO_ERROR_FILE_SEEK;
      fileHandle->lastError = errno;
      goto end;
    } else if (lseekResult == (off64_t)offset) {
      break;
    }

    seekTry--;
  }

  if (seekTry == 0) {
    // tried so hard, got so far
    platformFileHandle->error = HANDMADEHERO_ERROR_FILE_SEEK;
    fileHandle->lastError = EAGAIN;
    goto end;
  }

  ssize_t bytesRead = read(fileHandle->fd, dest, size);
  if (bytesRead < 0) {
    platformFileHandle->error = HANDMADEHERO_ERROR_PATH_IS_NOT_FILE;
    fileHandle->lastError = errno;
    goto end;
  }

end:
  pthread_mutex_unlock(&fileReadLock);
}

struct platform_file_group
LinuxGetAllFilesOfTypeBegin(enum platform_file_type type)
{
  struct platform_file_group platformFileGroup = {};
  struct linux_file_group *fileGroup =
      mmap(0, sizeof(*fileGroup), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!fileGroup) {
    return platformFileGroup;
  }
  platformFileGroup.data = fileGroup;

  ZeroMemory(fileGroup, sizeof(*fileGroup));

  fileGroup->dir = opendir("./");
  if (fileGroup->dir == 0) {
    // TODO: directory cannot opened
    goto onError;
  }

  struct dirent *dirent;
  u32 dirent_max = ARRAY_COUNT(fileGroup->direntIndexes);
  while (dirent_max--) {
    long direntIndex = telldir(fileGroup->dir);
    assert(direntIndex != -1);

    dirent = readdir(fileGroup->dir);

    /* error occured */
    if (!dirent && errno != 0) {
      // TODO: error happend
      break;
    }

    /* end of directory stream is reached */
    if (dirent == 0)
      break;

    /* is directory entry regular file. man 2 getdents */
    if (dirent->d_type != DT_REG)
      continue;

    struct string filename = StringFromZeroTerminated((u8 *)dirent->d_name, 255);

    char *extensionString = 0;
    switch (type) {
    case PLATFORM_FILE_TYPE_ASSET_FILE: {
      extensionString = "hha";
    } break;
    default: {
      assert(0 && "type is not supported in this function");
    } break;
    }
    struct string extension = StringFromZeroTerminated((u8 *)extensionString, 255);
    if (!PathHasExtension(filename, extension))
      continue;

    fileGroup->direntIndexes[platformFileGroup.fileCount] = direntIndex;

    platformFileGroup.fileCount++;
  }

  return platformFileGroup;

onError:
  // TODO: unmap failed?
  munmap(fileGroup, sizeof(*fileGroup));
  return platformFileGroup;
}

void
LinuxGetAllFilesOfTypeEnd(struct platform_file_group *platformFileGroup)
{
  struct linux_file_group *fileGroup = platformFileGroup->data;
  assert(fileGroup);

  closedir(fileGroup->dir);

  // TODO: unmap failed?
  munmap(fileGroup, sizeof(*fileGroup));
  fileGroup = 0;
}

void
LinuxFileError(struct platform_file_handle *platformFileHandle, enum handmadehero_error error)
{
  assert(error != HANDMADEHERO_ERROR_NONE);
  struct linux_file_handle *fileHandle = platformFileHandle->data;
  platformFileHandle->error = error;

  char *errorMessages[] = {
      "none",
      "open file failed",
      "path is not file",
      "read failed",
      "cannot get file information. stat",
      "cannot seek on file",

      "file is not hha",
      "hha version is not supported",
  };
  char *errorMessage = errorMessages[error];

  debugf("[LinuxFileError]: %s\n  fd: %d\n", errorMessage, fileHandle->fd);
}

b32
LinuxHasFileError(struct platform_file_handle *platformFileHandle)
{
  return platformFileHandle->error != HANDMADEHERO_ERROR_NONE;
}

#if HANDMADEHERO_INTERNAL

struct read_file_result
PlatformReadEntireFile(char *path)
{
  struct read_file_result result = {};
  int fd = open(path, O_RDONLY);
  assert(fd >= 0);

  struct stat stat;
  if (fstat(fd, &stat))
    return result;

  result.size = (u64)stat.st_size;

  assert(S_ISREG(stat.st_mode));
  result.data = malloc((size_t)stat.st_size);
  assert(result.data != 0);

  ssize_t bytesRead = read(fd, result.data, (size_t)stat.st_size);
  assert(bytesRead > 0);

  close(fd);

  return result;
}

u8
PlatformWriteEntireFile(char *path, u64 size, void *data)
{
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return 0;

  ssize_t bytesWritten = write(fd, data, size);
  assert(bytesWritten > 0);

  close(fd);

  return 1;
}

void
PlatformFreeMemory(void *address)
{
  free(address);
  address = 0;
}

#endif /* HANDMADEHERO_INTERNAL */

internal void
HandleCycleCounters(struct game_memory *memory)
{
#if HANDMADEHERO_INTERNAL
  debugf("CYCLE COUNTS:\n");

  char *counterNameTable[] = {"GameUpdateAndRender", "DrawRenderGroup",      "DrawRectangleSlowly",
                              "ProcessPixel",        "DrawRectangleQuickly", "AudioMixer"};
  static_assert(ARRAY_COUNT(counterNameTable) == CYCLE_COUNTER_COUNT);

  for (u32 counterIndex = 0; counterIndex < ARRAY_COUNT(memory->counters); counterIndex++) {
    struct cycle_counter *counter = memory->counters + counterIndex;

    if (counter->hitCount == 0)
      continue;

    debugf("  %s: %" PRIu64 "cy %" PRIu64 "h %" PRIu64 "cy/h\n", counterNameTable[counterIndex], counter->cycleCount,
           counter->hitCount, counter->cycleCount / counter->hitCount);

    counter->hitCount = 0;
    counter->cycleCount = 0;
  }
#endif
}

/*****************************************************************
 * memory bank
 *****************************************************************/

internal u8
game_memory_allocation(struct game_memory *memory, u64 permanentStorageSize, u64 transientStorageSize)
{
  memory->permanentStorageSize = permanentStorageSize;
  memory->transientStorageSize = transientStorageSize;
  u64 len = memory->permanentStorageSize + memory->transientStorageSize;

  void *data = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  memory->permanentStorage = data;
  memory->transientStorage = data + permanentStorageSize;

  u8 is_allocation_failed = data == (void *)-1;
  return is_allocation_failed;
}

/***************************************************************
 * hot game code reloading
 ***************************************************************/

struct linux_work_queue;
internal inline void
LinuxWorkQueueCompleteAllWork(struct linux_work_queue *queue);

#if HANDMADEHERO_DEBUG

struct game_code {
  char path[255];
  time_t time;
  void *module;
  volatile u32 isReloading;

  struct linux_work_queue *highPriorityQueue;
  struct linux_work_queue *lowPriorityQueue;

  pfnGameUpdateAndRender GameUpdateAndRender;
  pfnGameOutputAudio GameOutputAudio;
};

internal u8
ReloadGameCode(struct game_code *lib)
{
  struct stat sb;

  int fail = stat(lib->path, &sb);
  if (fail) {
    debugf("[ReloadGameCode] failed to stat\n");
    return 0;
  }

  if (sb.st_mtime == lib->time) {
    return 0;
  }

  __atomic_store_n(&lib->isReloading, 1, __ATOMIC_RELEASE);
  LinuxWorkQueueCompleteAllWork(lib->highPriorityQueue);
  LinuxWorkQueueCompleteAllWork(lib->lowPriorityQueue);

  // unload shared lib
  if (lib->module) {
    dlclose(lib->module);
    lib->module = 0;
  }

  // load shared lib
  lib->module = dlopen(lib->path, RTLD_NOW | RTLD_LOCAL);
  if (!lib->module) {
    debugf("[ReloadGameCode] failed to open\n");
    return 0;
  }

  // get function pointer
  lib->GameUpdateAndRender = dlsym(lib->module, "GameUpdateAndRender");
  assert(lib->GameUpdateAndRender != 0 && "wrong module format");

  lib->GameOutputAudio = dlsym(lib->module, "GameOutputAudio");
  assert(lib->GameOutputAudio != 0 && "wrong module format");

  // update module time
  lib->time = sb.st_mtime;
  debugf("[ReloadGameCode] reloaded @%d\n", lib->time);

  __atomic_store_n(&lib->isReloading, 0, __ATOMIC_RELEASE);

  return 1;
}

#endif

/*****************************************************************
 * structures
 *****************************************************************/
struct linux_state {
  /* WAYLAND */
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
  struct wp_presentation *wp_presentation;
  struct wp_content_type_manager_v1 *wp_content_type_manager_v1;

  struct wl_keyboard *wl_keyboard;
  struct wl_pointer *wl_pointer;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  /* PIPEWIRE */
  struct pw_thread_loop *pw_thread_loop;
  struct pw_stream *pw_stream;
  struct memory_arena pw_arena;

  struct game_input gameInputs[2];
  struct game_input *input;
  struct game_backbuffer backbuffer;
  struct game_memory game_memory;
  struct memory_arena wayland_arena;
  struct memory_arena xkb_arena;

  u32 surfaceWidth;
  u32 surfaceHeight;

#if HANDMADEHERO_DEBUG
  struct game_code lib;
  u8 recordInputIndex;
  int recordInputFd;
  u8 playbackInputIndex;
  int playbackInputFd;
#endif

  /* The Unadjusted System Time (or UST)
   * is a 64-bit monotonically increasing counter that is available
   * throughout the system.
   */
  u64 last_ust;

  u8 running : 1;
#if PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
  u8 paused : 1;
  u8 resumed : 1;
#endif
  u8 fullscreen : 1;
};

/***************************************************************
 * recording & playback inputs
 ***************************************************************/
#if HANDMADEHERO_DEBUG

comptime char recordPath[] = "input.rec";

internal inline u8
RecordInputStarted(struct linux_state *state)
{
  return state->recordInputIndex;
}

internal void
RecordInputBegin(struct linux_state *state, u8 index)
{
  struct game_memory *game_memory = &state->game_memory;

  debug("[RecordInput] begin\n");
  state->recordInputIndex = index;
  state->recordInputFd = open(recordPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  assert(state->recordInputFd >= 0);

  ssize_t bytesWritten = write(state->recordInputFd, game_memory->permanentStorage,
                               game_memory->permanentStorageSize + game_memory->transientStorageSize);
  assert(bytesWritten > 0);
}

internal void
RecordInputEnd(struct linux_state *state)
{
  debug("[RecordInput] end\n");
  state->recordInputIndex = 0;
  if (state->recordInputFd > 0)
    close(state->recordInputFd);
  state->recordInputFd = -1;
}

internal void
RecordInput(struct linux_state *state, struct game_input *input)
{
  write(state->recordInputFd, input, sizeof(*input));
}

internal inline u8
PlaybackInputStarted(struct linux_state *state)
{
  return state->playbackInputIndex;
}

internal void
PlaybackInputBegin(struct linux_state *state, u8 index)
{
  struct game_memory *game_memory = &state->game_memory;

  debug("[PlaybackInput] begin\n");

  state->playbackInputIndex = index;
  state->playbackInputFd = open(recordPath, O_RDONLY);
  assert(state->playbackInputFd >= 0);

  ssize_t bytesRead = read(state->playbackInputFd, game_memory->permanentStorage,
                           game_memory->permanentStorageSize + game_memory->transientStorageSize);
  assert(bytesRead > 0);
}

internal void
PlaybackInputEnd(struct linux_state *state)
{
  debug("[PlaybackInput] end\n");
  state->playbackInputIndex = 0;
  if (state->playbackInputFd > 0)
    close(state->playbackInputFd);
  state->playbackInputFd = -1;
}

internal void
PlaybackInput(struct linux_state *state, struct game_input *input)
{
  struct game_memory *game_memory = &state->game_memory;
  ssize_t bytesRead;

begin:
  bytesRead = read(state->playbackInputFd, input, sizeof(*input));
  // error happened
  assert(bytesRead >= 0);

  // end of file
  if (bytesRead == 0) {
    off_t result = lseek(state->playbackInputFd, 0, SEEK_SET);
    assert(result >= 0);
    ssize_t bytesRead = read(state->playbackInputFd, game_memory->permanentStorage,
                             game_memory->permanentStorageSize + game_memory->transientStorageSize);
    assert(bytesRead > 0);
    goto begin;
  }
}

#endif

/*****************************************************************
 * pipewire events
 *****************************************************************/
comptime u32 SAMPLE_RATE = 48000;
comptime u32 SAMPLE_CHANNELS = 2;

internal void
pw_stream_process(void *data)
{
  struct linux_state *state = data;
#if HANDMADEHERO_DEBUG
  while (state->lib.isReloading)
    ; // spin lock

  // from game layer
  pfnGameOutputAudio GameOutputAudio = state->lib.GameOutputAudio;
#endif

  // Obtain a buffer to write into.
  struct pw_buffer *pwBuffer = pw_stream_dequeue_buffer(state->pw_stream);
  if (!pwBuffer) {
    debug("[pw_stream_events::process] out of buffers\n");
    return;
  }

  // Get pointers in buffer memory to write to.
  struct spa_buffer *spaBuffer = pwBuffer->buffer;
  struct spa_data *datas = spaBuffer->datas;
  s16 *samples = datas[0].data;
  if (!samples) {
    debug("[pw_stream_events::process] empty sample\n");
    return;
  }

  u32 stride = sizeof(u16) * SAMPLE_CHANNELS;
  u32 sampleCount = datas[0].maxsize / stride;
#if PW_CHECK_VERSION(0, 3, 49)
  if (pwBuffer->requested) {
    sampleCount = Minimum((u32)pwBuffer->requested, sampleCount);
  }
#endif
  sampleCount = ALIGN4(sampleCount);

  // Write data into buffer.
  struct game_audio_buffer gameAudioBuffer = {
      .sampleRate = SAMPLE_RATE,
      .sampleCount = sampleCount,
      .samples = samples,
  };
  b32 isWritten = GameOutputAudio(&state->game_memory, &gameAudioBuffer);

  // Adjust buffer with number of written bytes, offset, stride.
  if (isWritten) {
    datas[0].chunk->offset = 0;
    datas[0].chunk->stride = (s32)stride;
    datas[0].chunk->size = sampleCount * stride;
  } else {
    datas[0].chunk->size = 0;
  }

  // Queue the buffer for playback.
  pw_stream_queue_buffer(state->pw_stream, pwBuffer);
}

struct pw_stream_buffer_state {
  b32 isInitialized : 1;
  struct memory_arena arena;
  struct memory_temp temp;
};

internal void
pw_stream_add_buffer(void *data, struct pw_buffer *pwBuffer)
{
  struct linux_state *state = data;
  struct spa_buffer *spaBuffer = pwBuffer->buffer;
  struct spa_data *datas = spaBuffer->datas;

  struct pw_stream_buffer_state *bufferState = state->pw_arena.data;
  if (!bufferState->isInitialized) {
    bufferState->isInitialized = 1;
    state->pw_arena.size += sizeof(*bufferState);
    MemorySubArenaInitAlignment(&bufferState->arena, &state->pw_arena, MemoryArenaGetRemainingSize(&state->pw_arena),
                                4);

    datas[0].type = SPA_DATA_MemFd;
    datas[0].flags = SPA_DATA_FLAG_READWRITE;
    datas[0].mapoffset = 0;

    datas[0].fd = memfd_create("handmadehero-pipewire", 0);
    assert(datas[0].fd >= 0);
    int ftruncateResult = ftruncate((int)datas[0].fd, (off_t)bufferState->arena.size);
    assert(ftruncateResult >= 0);
    void *mmapResult = mmap(bufferState->arena.data, (size_t)bufferState->arena.size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, (int)datas[0].fd, 0);
    assert(mmapResult != MAP_FAILED);
  }

  bufferState->temp = BeginTemporaryMemory(&bufferState->arena);
  u32 bufferSize = SAMPLE_RATE * SAMPLE_CHANNELS * sizeof(s16);
  datas[0].maxsize = bufferSize;
  datas[0].data = MemoryArenaPushAlignment(&bufferState->arena, bufferSize, 4);
}

internal void
pw_stream_remove_buffer(void *data, struct pw_buffer *buffer)
{
  struct linux_state *state = data;
  struct pw_stream_buffer_state *bufferState = state->pw_arena.data;
  assert(bufferState->isInitialized);
  EndTemporaryMemory(&bufferState->temp);
}

comptime struct pw_stream_events pw_stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = pw_stream_process,
    .add_buffer = pw_stream_add_buffer,
    .remove_buffer = pw_stream_remove_buffer,
};

/*****************************************************************
 * input handling
 *****************************************************************/

internal void
joystick_key(struct linux_state *state, u16 type, u16 code, s32 value)
{
  struct game_controller_input *controller = GetController(state->input, 1);

  debugf("[joystick_key] time: type: %d code: %d value: %d\n", type, code, value);
  controller->isAnalog = 1;

  if (type == 0) {
    return;
  }

  switch (code) {
  case ABS_HAT0X:
  case ABS_HAT1X:
  case ABS_HAT2X:
  case ABS_HAT3X: {
    controller->stickAverageX = (f32)value;
  } break;

  case ABS_HAT0Y:
  case ABS_HAT1Y:
  case ABS_HAT2Y:
  case ABS_HAT3Y: {
    controller->stickAverageY = -(f32)value;
  } break;

  case BTN_START: {
    controller->start.pressed = (u8)(value & 1);
  } break;

  case BTN_SELECT: {
    controller->back.pressed = (u8)(value & 1);
  } break;

    /* normalize stick x movement */
    comptime f32 MAX_LEFT = 32768.0f;
    comptime f32 MAX_RIGHT = 32767.0f;
    comptime s32 MIN_LEFT = -129;
    comptime s32 MIN_RIGHT = 128;
  case ABS_X: {
    f32 max = value < 0 ? MAX_LEFT : MAX_RIGHT;
    if (value >= MIN_LEFT && value <= MIN_RIGHT)
      value = 0;
    f32 x = (f32)value / max;
    controller->stickAverageX = x;
  } break;

  /* normalize stick y movement */
  case ABS_Y: {
    f32 max = value < 0 ? MAX_LEFT : MAX_RIGHT;
    if (value >= MIN_LEFT && value <= MIN_RIGHT)
      value = 0;
    f32 y = (f32)value / max;
    controller->stickAverageY = -y;
  } break;

  case BTN_Y: {
    controller->actionUp.pressed = (u8)(value & 0x1);
  } break;

  case BTN_A: {
    controller->actionDown.pressed = (u8)(value & 0x1);
  } break;

  case BTN_X: {
    controller->actionLeft.pressed = (u8)(value & 0x1);
  } break;

  case BTN_B: {
    controller->actionRight.pressed = (u8)(value & 0x1);
  } break;
  }
}

internal void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, u32 format, s32 fd, u32 size)
{
  struct linux_state *state = data;
  debugf("[wl_keyboard::keymap] format: %d fd: %d size: %d\n", format, fd, size);

  void *keymap_str = MemoryArenaPush(&state->xkb_arena, size);
  keymap_str = mmap(keymap_str, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  assert(keymap_str != MAP_FAILED);

  struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(state->xkb_context, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                             XKB_KEYMAP_COMPILE_NO_FLAGS);
  struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);

  xkb_keymap_unref(state->xkb_keymap);
  xkb_state_unref(state->xkb_state);

  state->xkb_keymap = xkb_keymap;
  state->xkb_state = xkb_state;
}

internal void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard, u32 serial, u32 time, u32 key,
                enum wl_keyboard_key_state keystate)
{
  struct linux_state *state = data;
  debugf("[wl_keyboard::key] serial: %d time: %d key: %d state: %d\n", serial, time, key, keystate);

  u32 keycode = key + 8;
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, keycode);

  struct game_controller_input *controller = GetController(state->input, 0);
  switch (keysym) {
  case 'q': {
    state->running = 0;
  } break;

#if HANDMADEHERO_DEBUG
  case 'L':
  case 'l': {
    /* if key is not pressed, exit */
    if (keystate == WL_KEYBOARD_KEY_STATE_RELEASED)
      break;

    /* if key is pressed, toggle between record and playback */
    if (!RecordInputStarted(state)) {
      ZeroMemory(state->input->controllers,
                 ARRAY_COUNT(state->input->controllers) * sizeof(*state->input->controllers));
      PlaybackInputEnd(state);
      RecordInputBegin(state, 1);
    } else {
      RecordInputEnd(state);
      PlaybackInputBegin(state, 1);
    }
  } break;
#endif

  case 'f': {
    if (!keystate)
      break;

    if (!state->fullscreen)
      xdg_toplevel_set_fullscreen(state->xdg_toplevel, 0);
    else
      xdg_toplevel_unset_fullscreen(state->xdg_toplevel);
    state->fullscreen = !state->fullscreen;
  } break;

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

  case 'H':
  case 'h': {
    // assert(controller-actionLeft.pressed != keystate);
    controller->actionLeft.pressed = (u8)(keystate & 0x1);
  } break;

  case ';': {
    // assert(controller-actionRight.pressed != keystate);
    controller->actionRight.pressed = (u8)(keystate & 0x1);
  } break;

  case 'J':
  case 'j': {
    // assert(controller-actionDown.pressed != keystate);
    controller->actionDown.pressed = (u8)(keystate & 0x1);
  } break;

  case 'K':
  case 'k': {
    // assert(controller-actionUp.pressed != keystate);
    controller->actionUp.pressed = (u8)(keystate & 0x1);
  } break;

  case XKB_KEY_Escape: {
    // assert(controller-back.pressed != keystate);
    controller->back.pressed = (u8)(keystate & 0x1);
  } break;

  case XKB_KEY_space: {
    // assert(controller-start.pressed != keystate);
    controller->start.pressed = (u8)(keystate & 0x1);
  } break;
  }
}

internal void
wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, u32 serial, u32 mods_depressed, u32 mods_latched,
                      u32 mods_locked, u32 group)
{
  struct linux_state *state = data;
  debugf("[wl_keyboard::modifiers] serial: %d depressed: %d latched: %d "
         "locked: %d group: %d\n",
         serial, mods_depressed, mods_latched, mods_locked, group);

  xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

internal void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, s32 rate, s32 delay)
{
  // struct my_state *state = data;
  debugf("[wl_keyboard::repeat_info] rate: %d delay: %d\n", rate, delay);
}

internal void
wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, u32 serial, struct wl_surface *surface,
                  struct wl_array *keys)
{
  debugf("[wl_keyboard::enter] serial: %d\n", serial);
  struct linux_state *state = data;

#if PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
  state->resumed = 1;
#endif
}

internal void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, u32 serial, struct wl_surface *surface)
{
  debugf("[wl_keyboard::leave] serial: %d\n", serial);
  struct linux_state *state = data;

#if PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
  state->paused = 1;
  state->resumed = 0;
#endif
}

comptime struct wl_keyboard_listener wl_keyboard_listener = {
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .keymap = wl_keyboard_keymap,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};

internal void
wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface,
                 wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  /* hide cursor */
  wl_pointer_set_cursor(wl_pointer, serial, 0, 0, 0);

  struct linux_state *state = data;
  struct game_input *input = state->input;
  struct game_backbuffer *backbuffer = &state->backbuffer;

  if (state->surfaceWidth == 0 || state->surfaceHeight == 0)
    return;

  f32 pointerX = (f32)wl_fixed_to_int(surface_x) / (f32)state->surfaceWidth;
  f32 pointerY = (f32)wl_fixed_to_int(surface_y) / (f32)state->surfaceHeight;

  input->pointerX = (u32)(pointerX * (f32)backbuffer->width);
  input->pointerY = (u32)(pointerY * (f32)backbuffer->height);
}

internal void
wl_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface)
{
}

internal void
wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  struct linux_state *state = data;
  struct game_input *input = state->input;
  struct game_backbuffer *backbuffer = &state->backbuffer;

  if (state->surfaceWidth == 0 || state->surfaceHeight == 0)
    return;

  f32 pointerX = (f32)wl_fixed_to_int(surface_x) / (f32)state->surfaceWidth;
  f32 pointerY = (f32)wl_fixed_to_int(surface_y) / (f32)state->surfaceHeight;

  input->pointerX = (u32)(pointerX * (f32)backbuffer->width);
  input->pointerY = (u32)(pointerY * (f32)backbuffer->height);
}

internal void
wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button,
                  uint32_t state)
{
}

internal void
wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

internal void
wl_pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
}

comptime struct wl_pointer_listener wl_pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .button = wl_pointer_button,
    .axis = wl_pointer_axis,
    .frame = wl_pointer_frame,
};

internal void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, u32 capabilities)
{
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

  u8 have_pointer = WL_SEAT_CAPABILITY_POINTER;
  if (have_pointer && !state->wl_pointer) {
    state->wl_pointer = wl_seat_get_pointer(wl_seat);
    wl_pointer_add_listener(state->wl_pointer, &wl_pointer_listener, state);
  } else if (!have_pointer && state->wl_pointer) {
    wl_pointer_release(state->wl_pointer);
    state->wl_pointer = 0;
  }
}

internal void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
  debugf("[wl_seat::name] name: %s\n", name);
}

comptime struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_capabilities,
    .name = wl_seat_name,
};

/*****************************************************************
 * shared memory
 *****************************************************************/
internal s32
create_shared_memory(off_t size)
{
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

internal void
wp_presentation_feedback_sync_output(void *data, struct wp_presentation_feedback *wp_presentation_feedback,
                                     struct wl_output *output)
{
}

internal void
wp_presentation_feedback_presented(void *data, struct wp_presentation_feedback *wp_presentation_feedback,
                                   uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh,
                                   uint32_t seq_hi, uint32_t seq_lo, uint32_t flags)
{
  wp_presentation_feedback_destroy(wp_presentation_feedback);
  struct linux_state *state = data;

  struct game_input *newInput = state->gameInputs + 0;
  struct game_input *oldInput = state->gameInputs + 1;
  /*
   *        |-----|-----|-----|-----|-----|---...---|>
   *  frame 0     1     2     3     4     5         30
   *  time  0   ~33ms ~66ms ~99ms                   1sec
   *  frame 0     1     2     3     4     5         60
   *  time  0   ~16ms ~33ms                       1sec
   *  frame 0     1     2     3     4     5
   *  time  0    1sec  2sec
   *        ⇐ ∆t  ⇒
   *
   *    ∆t > one frame time, display next frame
   */
  const f32 nanosecondsPerSecond = 1000000000;
  const f32 framesPerSecond = 30;
  const f32 nanosecondsPerFrame = nanosecondsPerSecond / (framesPerSecond + 1);

  u64 ust = (((u64)tv_sec_hi << 32) + tv_sec_lo) * 1000000000 + tv_nsec;

#if PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
  if (state->paused && state->resumed) {
    state->last_ust = ust - (u64)nanosecondsPerFrame;
    state->paused = 0;
    state->resumed = 0;
  } else if (state->paused) {
    return;
  }
#endif

  u64 elapsed = ust - state->last_ust;
#if !PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
  if (elapsed > (u64)(nanosecondsPerFrame * 1.9f)) {
    state->last_ust = ust - (u64)nanosecondsPerFrame;
    elapsed = ust - state->last_ust;
  }
#endif

  if ((f32)elapsed >= nanosecondsPerFrame) {
    state->input = newInput;

    /*
     *        |-----|-----|-----|-----|-----|---...---|>
     *  frame 0     1     2     3     4     5         30
     *  time  0   ~33ms ~66ms ~99ms                   1
     *        ⇐ ∆t  ⇒
     *  dtPerFrame in seconds
     */
    newInput->dtPerFrame = (f32)elapsed / nanosecondsPerSecond;
    // debugf("∆t = %.3f\n", newInput->dtPerFrame);

#if HANDMADEHERO_DEBUG
    // record & playback
    if (RecordInputStarted(state)) {
      RecordInput(state, state->input);
    }

    if (PlaybackInputStarted(state)) {
      PlaybackInput(state, state->input);
    }

    // game layer
    pfnGameUpdateAndRender GameUpdateAndRender = state->lib.GameUpdateAndRender;
#endif

    struct game_backbuffer *backbuffer = &state->backbuffer;
    GameUpdateAndRender(&state->game_memory, newInput, backbuffer);
    HandleCycleCounters(&state->game_memory);
    wl_surface_attach(state->wl_surface, state->wl_buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0, (s32)backbuffer->width, (s32)backbuffer->height);

    state->last_ust = ust;

    // swap inputs
    struct game_input *tempInput = newInput;
    newInput = oldInput;
    oldInput = tempInput;

#if HANDMADEHERO_DEBUG
    // hot reloading
    struct game_input *newInputToBe = oldInput;
    newInputToBe->gameCodeReloaded = (u8)(ReloadGameCode(&state->lib) & 0x1);
#endif
  }
}

internal void
wp_presentation_feedback_discarded(void *data, struct wp_presentation_feedback *wp_presentation_feedback)
{
  wp_presentation_feedback_destroy(wp_presentation_feedback);
}

comptime struct wp_presentation_feedback_listener wp_presentation_feedback_listener = {
    .sync_output = wp_presentation_feedback_sync_output,
    .presented = wp_presentation_feedback_presented,
    .discarded = wp_presentation_feedback_discarded};

comptime struct wl_callback_listener wl_surface_frame_listener;

internal void
wl_surface_frame_done(void *data, struct wl_callback *wl_callback, u32 time)
{
  wl_callback_destroy(wl_callback);
  struct linux_state *state = data;

  struct wp_presentation_feedback *feedback = wp_presentation_feedback(state->wp_presentation, state->wl_surface);
  wp_presentation_feedback_add_listener(feedback, &wp_presentation_feedback_listener, data);

  wl_callback = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(wl_callback, &wl_surface_frame_listener, data);

  wl_surface_commit(state->wl_surface);
}

comptime struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

/*****************************************************************
 * xdg_wm_base events
 *****************************************************************/
internal void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, u32 serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
  debugf("[xdg_wm_base::ping] pong(serial: %d)\n", serial);
}

comptime struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

/*****************************************************************
 * xdg_toplevel events
 *****************************************************************/

internal void
xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, s32 screen_width, s32 screen_height,
                       struct wl_array *states)
{
  debugf("[xdg_toplevel::configure] screen width: %d height: %d\n", screen_width, screen_height);
  if (screen_width <= 0 || screen_height <= 0)
    return;

  struct linux_state *state = data;

  state->fullscreen = 0;
  enum xdg_toplevel_state *toplevel_state;
  wl_array_for_each(toplevel_state, states)
  {
    switch (*toplevel_state) {
    case XDG_TOPLEVEL_STATE_FULLSCREEN:
      state->fullscreen = 1;
      break;
    default:
      break;
    }
  }

#if !DISABLE_SURFACE_SCALING
  if (state->wp_viewporter) {
    if (state->wp_viewport) {
      wp_viewport_destroy(state->wp_viewport);
      state->wp_viewport = 0;
    }

    state->wp_viewport = wp_viewporter_get_viewport(state->wp_viewporter, state->wl_surface);
    wp_viewport_set_destination(state->wp_viewport, screen_width, screen_height);
  }
#endif

  state->surfaceWidth = (u32)screen_width;
  state->surfaceHeight = (u32)screen_height;

  wl_surface_commit(state->wl_surface);
}

internal void
xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
  struct linux_state *state = data;
  state->running = 0;
}

comptime struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

/*****************************************************************
 * xdg_surface events
 *****************************************************************/
internal void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, u32 serial)
{
  struct linux_state *state = data;

  /* ack */
  xdg_surface_ack_configure(xdg_surface, serial);
  debugf("[xdg_surface::configure] ack_configure(serial: %d)\n", serial);

  wl_surface_commit(state->wl_surface);
}

comptime struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

/*****************************************************************
 * wl_registry events
 *****************************************************************/

#define WL_COMPOSITOR_MINIMUM_REQUIRED_VERSION 4
#define WL_SHM_MINIMUM_REQUIRED_VERSION 1
#define WL_SEAT_MINIMUM_REQUIRED_VERSION 6
#define XDG_WM_BASE_MINIMUM_REQUIRED_VERSION 2
#define WP_VIEWPORTER_MINIMUM_REQUIRED_VERSION 1
#define WP_PRESENTATION_MINIMUM_REQUIRED_VERSION 1
#define EXT_IDLE_NOTIFIER_V1_MINIMUM_REQUIRED_VERSION 1

internal void
wl_registry_global(void *data, struct wl_registry *wl_registry, u32 name, const char *interface, u32 version)
{
  struct linux_state *state = data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->wl_compositor =
        wl_registry_bind(wl_registry, name, &wl_compositor_interface, WL_COMPOSITOR_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_compositor_interface\n");
  }

  else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, WL_SHM_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_shm_interface\n");
  }

  else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, WL_SEAT_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wl_seat\n");
  }

  else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->xdg_wm_base =
        wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, XDG_WM_BASE_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to xdg_wm_base_interface\n");
  }

  else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
    state->wp_viewporter =
        wl_registry_bind(wl_registry, name, &wp_viewporter_interface, WP_VIEWPORTER_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wp_viewporter_interface\n");
  }

  else if (strcmp(interface, wp_presentation_interface.name) == 0) {
    state->wp_presentation =
        wl_registry_bind(wl_registry, name, &wp_presentation_interface, WP_PRESENTATION_MINIMUM_REQUIRED_VERSION);
    debug("[wl_registry::global] binded to wp_presentation_interface\n");
  }

  else if (strcmp(interface, wp_content_type_manager_v1_interface.name) == 0) {
    state->wp_content_type_manager_v1 =
        wl_registry_bind(wl_registry, name, &wp_content_type_manager_v1_interface, version);
    debug("[wl_registry::global] binded to wp_content_type_manager_v1_interface\n");
  }
}

comptime struct wl_registry_listener registry_listener = {
    .global = wl_registry_global,
};

/*****************************************************************
 * starting point
 *****************************************************************/

#define OP_WAYLAND (1 << 0)
#define OP_INOTIFY_WATCH (1 << 1)
#define OP_DEVICE_OPEN (1 << 2)
#define OP_JOYSTICK_POLL (1 << 3)
#define OP_JOYSTICK_READ (1 << 4)

struct op {
  u8 type;
  int fd;
};

struct op_device_open {
  u8 type;
  const char path[32];
};

struct op_joystick_read {
  u8 type;
  int fd;
  struct input_event event;
};

internal inline u8
libevdev_is_joystick(struct libevdev *evdev)
{
  return libevdev_has_event_type(evdev, EV_ABS) && libevdev_has_event_code(evdev, EV_ABS, ABS_RX);
}

#define ALIGNED_TO_CACHE_LINE __attribute__((aligned(64)))

struct linux_work_queue {
  sem_t semaphore;
  u32 completeGoal;
  ALIGNED_TO_CACHE_LINE volatile u32 completeCount;
  ALIGNED_TO_CACHE_LINE volatile u32 writeIndex;
  ALIGNED_TO_CACHE_LINE volatile u32 readIndex;
  struct platform_work_queue_entry entries[256];
};

#define COMPILER_PROGRAM_ORDER __asm__ volatile("" ::: "memory")

internal b32
LinuxWorkQueueDoNextQueueEntry(struct linux_work_queue *queue)
{
  b32 done = 0;

  u32 currentEntryToRead = queue->readIndex;
  u32 nextEntryToRead = (currentEntryToRead + 1) % ARRAY_COUNT(queue->entries);
  if (currentEntryToRead != queue->writeIndex) {
    if (__atomic_compare_exchange_n(&queue->readIndex, &currentEntryToRead, nextEntryToRead, 1, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
      __atomic_thread_fence(__ATOMIC_RELEASE);
      struct platform_work_queue_entry *entry = queue->entries + currentEntryToRead;
      entry->callback((struct platform_work_queue *)queue, entry->data);
      __atomic_fetch_add(&queue->completeCount, 1, __ATOMIC_RELEASE);
      done = 1;
    }
    // else some other thread beat us to it
  }

  return done;
}

internal inline void
LinuxWorkQueueAddEntry(struct linux_work_queue *queue, pfnPlatformWorkQueueCallback callback, void *data)
{
  // TODO(e2dk4r): switch to __atomic_compare_exchange_n eventually so that any thread can add
  COMPILER_PROGRAM_ORDER;
  u32 currentEntryToWrite = queue->writeIndex;
  u32 nextEntryToWrite = (currentEntryToWrite + 1) % ARRAY_COUNT(queue->entries);

  // assert(nextEntryToWrite != queue->readIndex); fails when queue is small
  while (nextEntryToWrite == queue->readIndex)
    /* queue overflowed and is in invalid state,
     * so wait for it to catch up (completes remaining works). */
    ;

  struct platform_work_queue_entry *entry = queue->entries + currentEntryToWrite;
  entry->callback = (pfnPlatformWorkQueueCallback)callback;
  entry->data = data;

  queue->completeGoal++;
  __atomic_store_n(&queue->writeIndex, nextEntryToWrite, __ATOMIC_RELEASE);
  sem_post(&queue->semaphore);
}

internal inline void
LinuxWorkQueueCompleteAllWork(struct linux_work_queue *queue)
{
  while (queue->completeGoal != queue->completeCount) {
    LinuxWorkQueueDoNextQueueEntry(queue);
  }

  queue->completeGoal = 0;
  queue->completeCount = 0;
}

internal void *
thread_start(void *arg)
{
  struct linux_work_queue *queue = arg;

  while (1) {
    if (LinuxWorkQueueDoNextQueueEntry(queue) == 0) {
      sem_wait(&queue->semaphore);
    }
  }

  return 0;
}

internal s32
LinuxWorkQueueInit(struct linux_work_queue *queue, u32 threadCount)
{
  queue->completeGoal = 0;
  queue->completeCount = 0;

  queue->writeIndex = 0;
  queue->readIndex = 0;

  if (sem_init(&queue->semaphore, 0, 0))
    return 1;

  for (u32 threadIndex = 0; threadIndex < threadCount; threadIndex++) {
    pthread_t threadId;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_attr_setstacksize(&attr, 1 * MEGABYTES)) {
      debug("failed to set thread stack size to 1m\n");
    }
    if (pthread_create(&threadId, &attr, thread_start, queue))
      return 2;
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  struct linux_work_queue highPriorityQueue;
  if (LinuxWorkQueueInit(&highPriorityQueue, 6))
    return HANDMADEHERO_ERROR_THREAD_INIT;

  struct linux_work_queue lowPriorityQueue;
  if (LinuxWorkQueueInit(&lowPriorityQueue, 2))
    return HANDMADEHERO_ERROR_THREAD_INIT;

  int error_code = 0;
  struct linux_state state = {
    .running = 1,
#if PAUSE_WHEN_SURFACE_OUT_OF_FOCUS
    .paused = 1,
    .resumed = 1,
#endif
  };
  state.input = state.gameInputs;

  /* game: hot reload game code on debug version */
#if HANDMADEHERO_DEBUG
  {
    /* assumes libhandmadehero.so in same directory as executable */
    comptime char libpath[] = "libhandmadehero.so";
    comptime u64 libpath_length = sizeof(libpath) - 1;
    char *exepath = argv[0];
    char *output = state.lib.path;
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
  state.lib.highPriorityQueue = &highPriorityQueue;
  state.lib.lowPriorityQueue = &lowPriorityQueue;

  ReloadGameCode(&state.lib);
#endif

  /* xkb */
  state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!state.xkb_context) {
    error_code = HANDMADEHERO_ERROR_XKB;
    goto exit;
  }

  /* game: backbuffer */
  /*
   *  resolution single double buffering
   *   960x540x4 ~1.10M  ~3.98M
   *  1280x720x4 ~3.53M  ~7.32M
   * 1920x1080x4 ~7.91M ~15.82M
   */

  state.backbuffer = (struct game_backbuffer)
  {
#if RESOLUTION == 540
    .width = 960, .height = 540,
#elif RESOLUTION == 720
    .width = 1280, .height = 720,
#elif RESOLUTION == 1080
    .width = 1920, .height = 1080,
#elif RESOLUTION == 719 // WEIRD
    .width = 1279, .height = 719,
#else
#error "resolution is invalid"
#endif
  };
  state.backbuffer.stride = ALIGN16(state.backbuffer.width * BITMAP_BYTES_PER_PIXEL);

  /* game: mem allocation */
  struct game_memory *game_memory = &state.game_memory;
  if (game_memory_allocation(game_memory, 256 * MEGABYTES, 1 * GIGABYTES)) {
    comptime char msg[] = "error: cannot allocate memory!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto exit;
  }
#if HANDMADEHERO_INTERNAL
  game_memory->platform.ReadEntireFile = PlatformReadEntireFile;
  game_memory->platform.WriteEntireFile = PlatformWriteEntireFile;
  game_memory->platform.FreeMemory = PlatformFreeMemory;
#endif
  game_memory->highPriorityQueue = (struct platform_work_queue *)&highPriorityQueue;
  game_memory->lowPriorityQueue = (struct platform_work_queue *)&lowPriorityQueue;
  game_memory->platform.WorkQueueAddEntry = (pfnPlatformWorkQueueAddEntry)LinuxWorkQueueAddEntry;
  game_memory->platform.WorkQueueCompleteAllWork = (pfnPlatformWorkQueueCompleteAllWork)LinuxWorkQueueCompleteAllWork;

  game_memory->platform.OpenNextFile = (pfnPlatformOpenNextFile)LinuxOpenNextFile;
  game_memory->platform.ReadFromFile = (pfnPlatformReadFromFile)LinuxReadFromFile;
  game_memory->platform.HasFileError = (pfnPlatformHasFileError)LinuxHasFileError;
  game_memory->platform.FileError = (pfnPlatformFileError)LinuxFileError;
  game_memory->platform.GetAllFilesOfTypeBegin = (pfnPlatformGetAllFilesOfTypeBegin)LinuxGetAllFilesOfTypeBegin;
  game_memory->platform.GetAllFilesOfTypeEnd = (pfnPlatformGetAllFilesOfTypeEnd)LinuxGetAllFilesOfTypeEnd;
  game_memory->platform.AllocateMemory = (pfnPlatformAllocateMemory)LinuxAllocateMemory;
  game_memory->platform.DeallocateMemory = (pfnPlatformDeallocateMemory)LinuxDeallocateMemory;

  /* setup arenas */
  struct memory_arena event_arena;
  {
    u64 used = 0;
    u64 size;

    // for wayland allocations
#if RESOLUTION == 540
    size = 2 * MEGABYTES;
#elif RESOLUTION == 720
    size = 4 * MEGABYTES;
#elif RESOLUTION == 1080
    size = 8 * MEGABYTES;
#elif RESOLUTION == 719 // WEIRD
    size = 4 * MEGABYTES;
#endif
    MemoryArenaInit(&state.wayland_arena, game_memory->permanentStorage + used, size);
    used += size;

    // for xkb keyboard allocations
    size = 1 * MEGABYTES;
    MemoryArenaInit(&state.xkb_arena, game_memory->permanentStorage + used, size);
    used += size;

    // for pipewire allocations
    size = 256 * KILOBYTES;
    MemoryArenaInit(&state.pw_arena, game_memory->permanentStorage + used, size);
    used += size;

    // for event allocations
    size = 256 * KILOBYTES;
    MemoryArenaInit(&event_arena, game_memory->permanentStorage + used, size);
    used += size;

    // decrease permanent storage
    game_memory->permanentStorage += used;
    game_memory->permanentStorageSize -= used;
  }

  /* pipewire */
  pw_init(0, 0);
  state.pw_thread_loop = pw_thread_loop_new("audio", 0);
  if (!state.pw_thread_loop) {
    error_code = HANDMADEHERO_ERROR_PIPEWIRE;
    goto xkb_context_exit;
  }

  state.pw_stream = pw_stream_new_simple(
      pw_thread_loop_get_loop(state.pw_thread_loop), "handmadehero",
      pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Game", NULL),
      &pw_stream_events, &state);
  if (!state.pw_stream) {
    error_code = HANDMADEHERO_ERROR_PIPEWIRE;
    goto xkb_context_exit;
  }

  {
    // setup the format of the stream
    const struct spa_pod *params[1];
    u8 buffer[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    params[0] = spa_format_audio_raw_build(
        &builder, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16, .channels = SAMPLE_CHANNELS, .rate = SAMPLE_RATE, ));

    // Now we're ready to connect the stream
    int failed = pw_stream_connect(
        state.pw_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_ALLOC_BUFFERS, params, 1);
    if (failed) {
      error_code = HANDMADEHERO_ERROR_PIPEWIRE;
      goto xkb_context_exit;
    }
  }

  // start audio thread
  pw_thread_loop_start(state.pw_thread_loop);

  /* wayland */
  struct wl_display *wl_display = wl_display_connect(0);
  if (!wl_display) {
    comptime char msg[] = "error: cannot connect wayland display!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_WAYLAND_CONNECT;
    goto xkb_context_exit;
  }

  struct wl_registry *registry = wl_display_get_registry(wl_display);
  if (!registry) {
    comptime char msg[] = "error: cannot get registry!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_WAYLAND_REGISTRY;
    goto wl_exit;
  }

  /* wayland: get globals */
  wl_registry_add_listener(registry, &registry_listener, &state);
  wl_display_roundtrip(wl_display);

  if (!state.wl_compositor || !state.wl_shm || !state.wl_seat || !state.xdg_wm_base || !state.wp_presentation) {
    comptime char msg[] = "error: cannot get wayland globals!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_WAYLAND_EXTENSIONS;
    goto wl_exit;
  }

  /* wayland: create surface */
  state.wl_surface = wl_compositor_create_surface(state.wl_compositor);

  if (state.wp_content_type_manager_v1) {
    struct wp_content_type_v1 *wp_content_type_v1 =
        wp_content_type_manager_v1_get_surface_content_type(state.wp_content_type_manager_v1, state.wl_surface);
    wp_content_type_v1_set_content_type(wp_content_type_v1, WP_CONTENT_TYPE_V1_TYPE_GAME);

    // cleanup
    wp_content_type_manager_v1_destroy(state.wp_content_type_manager_v1);
    state.wp_content_type_manager_v1 = 0;
  }

  /* wayland: application window */
  xdg_wm_base_add_listener(state.xdg_wm_base, &xdg_wm_base_listener, &state);

  state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);

  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  xdg_toplevel_set_title(state.xdg_toplevel, "handmadehero");
  xdg_toplevel_set_maximized(state.xdg_toplevel);
  xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
  xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
  wl_surface_commit(state.wl_surface);

  /* wayland: register frame callback */
  struct wl_callback *frame_callback = wl_surface_frame(state.wl_surface);
  wl_callback_add_listener(frame_callback, &wl_surface_frame_listener, &state);

  /* wayland: listen for inputs */
  wl_seat_add_listener(state.wl_seat, &wl_seat_listener, &state);

  /* wayland: create buffer */
  u32 backbuffer_multiplier = 1;
  u32 backbuffer_size = state.backbuffer.height * state.backbuffer.stride * backbuffer_multiplier;
  s32 shm_fd = create_shared_memory(backbuffer_size);
  if (shm_fd == 0) {
    comptime char msg[] = "error: cannot create shared memory!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_SHARED_MEMORY;
    goto wl_exit;
  }

  state.backbuffer.memory = MemoryArenaPushAlignment(&state.wayland_arena, (size_t)backbuffer_size, 16);
  state.backbuffer.memory =
      mmap(state.backbuffer.memory, (size_t)backbuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (state.backbuffer.memory == MAP_FAILED) {
    comptime char msg[] = "error: cannot create shared memory!\n";
    comptime u64 msgLength = sizeof(msg) - 1;
    write(STDERR_FILENO, msg, msgLength);
    error_code = HANDMADEHERO_ERROR_ALLOCATION;
    goto shm_exit;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(state.wl_shm, shm_fd, (s32)backbuffer_size);
  if (!pool) {
    wl_shm_pool_destroy(pool);
    error_code = HANDMADEHERO_ERROR_WAYLAND_SHM_POOL;
    goto shm_exit;
  }
  state.wl_buffer = wl_shm_pool_create_buffer(pool, 0, (s32)state.backbuffer.width, (s32)state.backbuffer.height,
                                              (s32)state.backbuffer.stride, WL_SHM_FORMAT_XRGB8888);
  if (!state.wl_buffer) {
    wl_shm_pool_destroy(pool);
    error_code = HANDMADEHERO_ERROR_WAYLAND_SHM_POOL;
    goto shm_exit;
  }
  wl_surface_attach(state.wl_surface, state.wl_buffer, 0, 0);
  wl_shm_pool_destroy(pool);

  // make sure backbuffer is bottom-up
  state.backbuffer.memory = state.backbuffer.memory + (state.backbuffer.height - 1) * state.backbuffer.stride;
  state.backbuffer.stride = -state.backbuffer.stride;

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

  struct memory_chunk *MemoryForDeviceOpenEvents = MemoryArenaPushChunk(&event_arena, sizeof(struct op_device_open), 5);
  struct memory_chunk *MemoryForJoystickReadEvents =
      MemoryArenaPushChunk(&event_arena, sizeof(struct op_joystick_read), 10);

  sqe = io_uring_get_sqe(&ring);
  io_uring_prep_poll_multishot(sqe, fd_wl_display, POLLIN);
  io_uring_sqe_set_data(sqe, &(struct op){.type = OP_WAYLAND, .fd = fd_wl_display});

  /* inotify: notify when a new input added */
  int fd_inotify = inotify_init1(IN_NONBLOCK);
  if (fd_inotify < 0) {
    error_code = HANDMADEHERO_ERROR_INOTIFY_SETUP;
    goto io_uring_exit;
  }

  int fd_watch = inotify_add_watch(fd_inotify, "/dev/input", IN_CREATE | IN_DELETE);
  if (fd_watch < 0) {
    error_code = HANDMADEHERO_ERROR_INOTIFY_WATCH_SETUP;
    goto inotify_exit;
  }

  if (fd_inotify >= 0) {
    sqe = io_uring_get_sqe(&ring);
    struct op *submitOp = &(struct op){.type = OP_INOTIFY_WATCH, .fd = fd_inotify};
    io_uring_prep_poll_multishot(sqe, submitOp->fd, POLLIN);
    io_uring_sqe_set_data(sqe, submitOp);
  }

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

    struct op_joystick_read stagedOp = {};
    stagedOp.type = OP_JOYSTICK_READ, stagedOp.fd = open(path, O_RDONLY | O_NONBLOCK);
    if (stagedOp.fd < 0)
      continue;

    struct libevdev *evdev;
    int rc = libevdev_new_from_fd(stagedOp.fd, &evdev);
    if (rc < 0) {
      debug("libevdev failed\n");
      close(stagedOp.fd);
      if (evdev)
        libevdev_free(evdev);
      continue;
    }

    /* detect joystick */
    if (!libevdev_is_joystick(evdev)) {
      close(stagedOp.fd);
      libevdev_free(evdev);
      continue;
    }

    debugf("Input device name: \"%s\"\n", libevdev_get_name(evdev));
    debugf("Input device ID: bus %#x vendor %#x product %#x\n", libevdev_get_id_bustype(evdev),
           libevdev_get_id_vendor(evdev), libevdev_get_id_product(evdev));

    struct op_joystick_read *submitOp = MemoryChunkPush(MemoryForJoystickReadEvents);
    *submitOp = stagedOp;
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, submitOp->fd, &submitOp->event, sizeof(submitOp->event), 0);
    io_uring_sqe_set_data(sqe, submitOp);

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

    int error;
  wait:
    error = io_uring_wait_cqe(&ring, &cqe);
    if (error) {
      if (errno == EAGAIN)
        goto wait;
      error_code = HANDMADEHERO_ERROR_IO_URING_WAIT;
      break;
    }

    struct op *op = io_uring_cqe_get_data(cqe);
    if (op == 0) {
      wl_display_cancel_read(wl_display);
      goto cqe_seen;
    }

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
    else if (op->type & OP_INOTIFY_WATCH) {
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

      /*
       * get the number of bytes available to read from an
       * inotify file descriptor.
       * see: inotify(7)
       */
      u32 bufsz;
      ioctl(op->fd, FIONREAD, &bufsz);

      u8 buf[bufsz];
      ssize_t readBytes = read(op->fd, buf, bufsz);
      if (readBytes < 0) {
        goto cqe_seen;
      }

      struct inotify_event *event = (struct inotify_event *)buf;
      if (event->len <= 0)
        goto cqe_seen;

      if (event->mask & IN_ISDIR)
        goto cqe_seen;

      /* get full path */
      char path[32] = "/dev/input/";
      for (char *dest = path + 11, *src = event->name; *src; src++, dest++) {
        *dest = *src;
      }

      if (event->mask & IN_DELETE)
        goto cqe_seen;

      struct op_device_open *submitOp = MemoryChunkPush(MemoryForDeviceOpenEvents);
      submitOp->type = OP_DEVICE_OPEN;
      for (char *dest = (char *)submitOp->path, *src = path; *src; src++, dest++)
        *dest = *src;

      /* wait for device initialization */
      sqe = io_uring_get_sqe(&ring);
      struct __kernel_timespec *ts = &(struct __kernel_timespec){
          .tv_nsec = 75000000, /* 750ms */
      };
      io_uring_prep_timeout(sqe, ts, 0, 0);
      io_uring_sqe_set_data(sqe, submitOp);
      io_uring_submit(&ring);
    }

    /* on device open events */
    else if (op->type & OP_DEVICE_OPEN) {
      wl_display_cancel_read(wl_display);

      if (cqe->res < 0 && cqe->res != -ETIME) {
        debug("waiting for device initialiation failed\n");
        MemoryChunkPop(MemoryForDeviceOpenEvents, op);
        goto cqe_seen;
      }
      struct op_device_open *op = io_uring_cqe_get_data(cqe);
      struct op_joystick_read stagedOp = {};
      stagedOp.type = OP_JOYSTICK_READ;
      stagedOp.fd = open(op->path, O_RDONLY | O_NONBLOCK);
      MemoryChunkPop(MemoryForDeviceOpenEvents, op);
      if (stagedOp.fd < 0) {
        debug("opening device failed\n");
        goto cqe_seen;
      }

      struct libevdev *evdev;
      int rc = libevdev_new_from_fd(stagedOp.fd, &evdev);
      if (rc < 0) {
        debug("libevdev failed\n");
        goto error;
      }

      /* detect joystick */
      if (!libevdev_is_joystick(evdev)) {
        debug("This device does not look like a joystick\n");
        goto error;
      }

      debugf("Input device name: \"%s\"\n", libevdev_get_name(evdev));
      debugf("Input device ID: bus %#x vendor %#x product %#x\n", libevdev_get_id_bustype(evdev),
             libevdev_get_id_vendor(evdev), libevdev_get_id_product(evdev));

      struct op_joystick_read *submitOp = MemoryChunkPush(MemoryForJoystickReadEvents);
      *submitOp = stagedOp;
      struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
      io_uring_prep_read(sqe, submitOp->fd, &submitOp->event, sizeof(submitOp->event), 0);
      io_uring_sqe_set_data(sqe, submitOp);
      io_uring_submit(&ring);

      libevdev_free(evdev);
      goto cqe_seen;

    error:
      if (evdev)
        libevdev_free(evdev);
      sqe = io_uring_get_sqe(&ring);
      io_uring_prep_close(sqe, stagedOp.fd);
      io_uring_sqe_set_data(sqe, 0);
      io_uring_submit(&ring);
    }

    /* on joystick events */
    else if (op->type & OP_JOYSTICK_READ) {
      wl_display_cancel_read(wl_display);
      /* on joystick read error (eg. joystick removed), close the fd */
      if (cqe->res < 0) {
        /* TODO: when joystick disconnected reset controller */
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_close(sqe, op->fd);
        io_uring_sqe_set_data(sqe, 0);
        io_uring_submit(&ring);
        MemoryChunkPop(MemoryForJoystickReadEvents, op);
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
