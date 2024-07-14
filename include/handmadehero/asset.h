#ifndef HANDMADEHERO_ASSET_H
#define HANDMADEHERO_ASSET_H

#include "asset_type_id.h"
#include "fileformats.h"
#include "math.h"
#include "memory_arena.h"
#include "platform.h"
#include "random.h"
#include "render_group.h"
#include "types.h"

struct audio {
  u32 channelCount;
  u32 sampleCount;
  s16 *samples[2];
};

enum asset_state {
  ASSET_STATE_UNLOADED,
  ASSET_STATE_QUEUED,
  ASSET_STATE_LOADED,
  ASSET_STATE_LOCKED,
};

struct asset_slot {
  enum asset_state state;
  union {
    struct bitmap *bitmap;
    struct audio *audio;
  };
};

struct asset {
  u32 fileIndex;
  struct hha_asset hhaAsset;
};

struct bitmap_info {
  char *filename;
  struct v2 alignPercentage;
};

struct audio_info {
  char *filename;
  u32 sampleIndex;
  u32 sampleCount;
  struct audio_id nextIdToPlay;
};
#define AUDIO_INFO_SAMPLE_COUNT_ALL 0

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,
  ASSET_TAG_FACING_DIRECTION, // angle in radians clockwise

  ASSET_TAG_COUNT
};

struct asset_vector {
  f32 e[ASSET_TAG_COUNT];
};

struct asset_type {
  u32 assetIndexFirst;
  u32 assetIndexOnePastLast;
};

struct asset_group {
  u32 tagFirstIndex;
  u32 tagLastIndex;
};

struct asset_file {
  struct platform_file_handle *handle;
  struct hha_header header;

  // TODO: if we ever do thread stacks, assetTypes does not
  // need to be kept here probably.
  struct hha_asset_type *assetTypes;

  u32 tagBase;
};

struct game_assets {
  // TODO(e2dk4r): copy of known, not ideal because
  // we want AssetLoad to called from anywhere
  struct transient_state *transientState;
  struct memory_arena arena;
  pfnPlatformReadEntireFile PlatformReadEntireFile;

  u32 tagCount;
  struct hha_tag *tags;
  f32 tagRanges[ASSET_TYPE_COUNT];

  u32 assetCount;
  struct asset *assets;
  struct asset_slot *slots;

  struct asset_type assetTypes[ASSET_TYPE_COUNT];

  u8 *hhaData;

  u32 fileCount;
  struct asset_file *files;
};

struct game_assets *
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState);

void
BitmapLoad(struct game_assets *assets, struct bitmap_id id);

void
BitmapPrefetch(struct game_assets *assets, struct bitmap_id id);

struct bitmap *
BitmapGet(struct game_assets *assets, struct bitmap_id id);

struct bitmap_id
BitmapGetFirstId(struct game_assets *assets, enum asset_type_id typeId);

struct bitmap_id
BestMatchBitmap(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
                struct asset_vector *weightVector);

struct bitmap_id
RandomBitmap(struct random_series *series, struct game_assets *assets, enum asset_type_id typeId);

void
AudioLoad(struct game_assets *assets, struct audio_id id);

void
AudioPrefetch(struct game_assets *assets, struct audio_id id);

struct audio *
AudioGet(struct game_assets *assets, struct audio_id id);

struct audio_id
AudioGetFirstId(struct game_assets *assets, enum asset_type_id typeId);

struct audio_id
AudioGetNextInChain(struct game_assets *assets, struct audio_id id);

struct audio_id
RandomAudio(struct random_series *series, struct game_assets *assets, enum asset_type_id typeId);

struct audio_id
BestMatchAudio(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
               struct asset_vector *weightVector);

struct hha_audio *
AudioInfoGet(struct game_assets *assets, struct audio_id id);

b32
IsAudioIdValid(struct audio_id id);

#endif /* HANDMADEHERO_ASSET_H */
