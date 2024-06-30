#ifndef HANDMADEHERO_ASSET_H
#define HANDMADEHERO_ASSET_H

#include "math.h"
#include "memory_arena.h"
#include "platform.h"
#include "render_group.h"
#include "types.h"

struct audio {
  u32 channelCount;
  u32 sampleCount;
  u16 *samples[2];
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

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,
  ASSET_TAG_FACING_DIRECTION, // angle in radians clockwise

  ASSET_TAG_COUNT
};

enum asset_type_id {
  ASSET_TYPE_NONE,

  ASSET_TYPE_SHADOW,
  ASSET_TYPE_TREE,
  ASSET_TYPE_SWORD,
  ASSET_TYPE_ROCK,
  ASSET_TYPE_GRASS,
  ASSET_TYPE_GROUND,
  ASSET_TYPE_TUFT,

  ASSET_TYPE_HEAD,
  ASSET_TYPE_TORSO,
  ASSET_TYPE_CAPE,

  ASSET_TYPE_COUNT
};

struct asset_vector {
  f32 e[ASSET_TAG_COUNT];
};

struct asset_tag {
  enum asset_tag_id id;
  f32 value;
};

struct asset {
  u32 tagIndexFirst;
  u32 tagIndexOnePastLast;
  u32 slotId;
};

struct asset_type {
  u32 assetIndexFirst;
  u32 assetIndexOnePastLast;
};

struct asset_bitmap_info {
  char *filename;
  struct v2 alignPercentage;
};

struct audio_info {
  char *filename;
};

struct asset_group {
  u32 tagFirstIndex;
  u32 tagLastIndex;
};

struct game_assets {
  // TODO(e2dk4r): copy of known, not ideal because
  // we want AssetLoad to called from anywhere
  struct transient_state *transientState;
  struct memory_arena arena;
  pfnPlatformReadEntireFile PlatformReadEntireFile;

  u32 bitmapCount;
  struct asset_slot *bitmaps;
  struct asset_bitmap_info *bitmapInfos;

  u32 audioCount;
  struct asset_slot *audios;
  struct audio_info *audioInfos;

  u32 tagCount;
  struct asset_tag *tags;
  f32 tagRanges[ASSET_TYPE_COUNT];

  u32 assetCount;
  struct asset *assets;

  struct asset_type assetTypes[ASSET_TYPE_COUNT];

#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0

  // TODO(e2dk4r): remove this once we actually load a asset pack file
  u32 DEBUGUsedBitmapInfoCount;
  u32 DEBUGUsedAssetCount;
  u32 DEBUGUsedTagCount;
  struct asset_type *DEBUGAssetType;
  struct asset *DEBUGAsset;
};

struct bitmap_id {
  u32 value;
};

struct audio_id {
  u32 value;
};

struct bitmap *
AssetBitmapGet(struct game_assets *assets, struct bitmap_id id);

struct game_assets *
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState,
                   pfnPlatformReadEntireFile PlatformReadEntireFile);

void
AssetBitmapLoad(struct game_assets *assets, struct bitmap_id id);

struct bitmap_id
AssetBitmapGetFirstId(struct game_assets *assets, enum asset_type_id typeId);

void
AudioLoad(struct game_assets *assets, struct audio_id id);

struct bitmap_id
BestMatchAsset(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
               struct asset_vector *weightVector);

struct audio
LoadWav(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename);

#endif /* HANDMADEHERO_ASSET_H */
