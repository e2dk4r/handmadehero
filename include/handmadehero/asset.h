#ifndef HANDMADEHERO_ASSET_H
#define HANDMADEHERO_ASSET_H

#include "math.h"
#include "memory_arena.h"
#include "platform.h"
#include "render_group.h"
#include "types.h"

enum asset_state {
  ASSET_STATE_UNLOADED,
  ASSET_STATE_QUEUED,
  ASSET_STATE_LOADED,
  ASSET_STATE_LOCKED,
};

struct asset_slot {
  enum asset_state state;
  struct bitmap *bitmap;
};

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,

  ASSET_TAG_COUNT
};

enum asset_type_id {
  ASSET_TYPE_BACKGROUND,
  ASSET_TYPE_SHADOW,
  ASSET_TYPE_TREE,
  ASSET_TYPE_SWORD,
  ASSET_TYPE_STAIRWELL,
  ASSET_TYPE_ROCK,

  ASSET_TYPE_COUNT
};

struct asset_tag {
  u32 id;
  f32 value;
};

struct asset {
  u32 tagFirstIndex;
  u32 tagLastIndex;
  u32 slotId;
};

struct asset_type {
  u32 count;
  u32 tagFirstIndex;
  u32 tagLastIndex;
};

struct asset_bitmap_info {
  struct v2 alignPercentage;
  f32 widthOverHeight;
  u32 width;
  u32 height;
};

struct asset_group {
  u32 tagFirstIndex;
  u32 tagLastIndex;
};

struct bitmap_hero {
  struct bitmap head;
  struct bitmap torso;
  struct bitmap cape;
};

struct game_assets {
  // TODO(e2dk4r): copy of known, not ideal because
  // we want AssetLoad to called from anywhere
  struct transient_state *transientState;
  struct memory_arena arena;
  pfnPlatformReadEntireFile PlatformReadEntireFile;

  u32 bitmapCount;
  struct asset_slot *bitmaps;

  u32 audioCount;
  struct asset_slot *audios;

  struct asset_type assetTypes[ASSET_TYPE_COUNT];

  // array'd assets
  struct bitmap textureGrass[2];
  struct bitmap textureGround[4];
  struct bitmap textureTuft[3];

  // structures assets
#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0
  struct bitmap_hero textureHero[4];
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
AssetAudioLoad(struct game_assets *assets, struct audio_id id);

#endif /* HANDMADEHERO_ASSET_H */
