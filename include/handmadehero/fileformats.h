#ifndef HANDMADEHERO_FILEFORMATS_H
#define HANDMADEHERO_FILEFORMATS_H

#include "types.h"

#define HHA_ENCODE(a, b, c, d) (a << 0x00 | b << 0x08 | c << 0x10 | d << 0x18)

#pragma pack(push, 1)

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

struct hha_audio {
  u32 channelCount;
  u32 sampleCount;
  struct audio_id nextIdToPlay;
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
