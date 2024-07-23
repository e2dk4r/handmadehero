#ifndef HANDMADEHERO_FILEFORMATS_H
#define HANDMADEHERO_FILEFORMATS_H

#include "types.h"

#define HHA_ENCODE(a, b, c, d) (a << 0x00 | b << 0x08 | c << 0x10 | d << 0x18)

#pragma pack(push, 1)

enum asset_type_id {
  ASSET_TYPE_NONE,

  /* ================ BITMAPS ================ */
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

  ASSET_TYPE_FONT,

  /* ================ AUDIOS ================ */
  ASSET_TYPE_BLOOP,
  ASSET_TYPE_CRACK,
  ASSET_TYPE_DROP,
  ASSET_TYPE_GLIDE,
  ASSET_TYPE_MUSIC,
  ASSET_TYPE_PUHP,

  ASSET_TYPE_COUNT
};

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,
  ASSET_TAG_FACING_DIRECTION, // angle in radians clockwise
  ASSET_TAG_UNICODE_CODEPOINT,

  ASSET_TAG_COUNT
};

// Handmadehero Asset Header
struct hha_header {
#define HHA_MAGIC HHA_ENCODE('h', 'h', 'a', 'f')
  u32 magic;

#define HHA_VERSION 0
  u32 version;

  u32 tagCount;
  u32 assetCount;
  u32 assetTypeCount;

  // hha_tag[tagCount]
  u64 tagsOffset;

  // hha_asset_type[assetTypeCount]
  u64 assetTypesOffset;

  // hha_asset[assetCount]
  u64 assetsOffset;
};

struct bitmap_id {
  u32 value;
};

struct audio_id {
  u32 value;
};

struct hha_tag {
  u32 id;
  f32 value;
};

struct hha_asset_type {
  u32 typeId;
  u32 assetIndexFirst;
  u32 assetIndexOnePastLast;
};

struct hha_bitmap {
  u32 width;
  u32 height;
  f32 alignPercentage[2];
};

enum hha_audio_chain {
  HHA_AUDIO_CHAIN_NONE,
  HHA_AUDIO_CHAIN_LOOP,
  HHA_AUDIO_CHAIN_ADVANCE,
};

struct hha_audio {
  u32 channelCount;
  u32 sampleCount;
  enum hha_audio_chain chain;
};

struct hha_asset {
  u64 dataOffset;
  u32 tagIndexFirst;
  u32 tagIndexOnePastLast;
  union {
    struct hha_bitmap bitmap;
    struct hha_audio audio;
  };
};

#pragma pack(pop)

#endif /* HANDMADEHERO_FILEFORMATS_H */
