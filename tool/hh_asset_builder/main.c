#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#pragma GCC diagnostic push

// caused by: stb_ds.h
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#define sprintf stbsp_sprintf

#include <handmadehero/assert.h>
#include <handmadehero/asset_type_id.h>
#include <handmadehero/math.h>
#include <handmadehero/types.h>

#pragma GCC diagnostic pop

#define infon(str, len) write(STDOUT_FILENO, str, len)
#define info(str) infon(str, sizeof(str) - 1)
#define warning(str) info("w:" str)
#define fatal(str) write(STDERR_FILENO, "e:" str, sizeof(str) - 1)

struct bitmap_info {
  char *filename;
  struct v2 alignPercentage;
};

struct audio_info {
  char *filename;
  u32 sampleIndex;
  u32 sampleCount;
  u32 nextIdToPlay;
};

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,
  ASSET_TAG_FACING_DIRECTION, // angle in radians clockwise

  ASSET_TAG_COUNT
};

struct asset_tag {
  enum asset_tag_id id;
  f32 value;
};

struct asset {
  u64 dataOffset;
  u32 tagIndexFirst;
  u32 tagIndexOnePastLast;
};

struct asset_type {
  u32 assetIndexFirst;
  u32 assetIndexOnePastLast;
};

#define BITMAP_INFO_COUNT 0x1000
#define AUDIO_INFO_COUNT 0x1000
#define TAG_COUNT 0x1000
#define ASSET_COUNT 0x1000

u32 bitmapCount;
struct bitmap_info bitmapInfos[BITMAP_INFO_COUNT];

u32 audioCount;
struct audio_info audioInfos[AUDIO_INFO_COUNT];

u32 tagCount;
struct asset_tag tags[TAG_COUNT];
f32 tagRanges[ASSET_TYPE_COUNT];

u32 assetCount;
struct asset assets[ASSET_COUNT];

struct asset_type assetTypes[ASSET_TYPE_COUNT];

u32 DEBUGUsedBitmapInfoCount;
u32 DEBUGUsedAudioInfoCount;
u32 DEBUGUsedAssetCount;
u32 DEBUGUsedTagCount;
struct asset_type *DEBUGAssetType;
struct asset *DEBUGAsset;

internal void
BeginAssetType(enum asset_type_id assetTypeId)
{
  assert(DEBUGAssetType == 0 && "another already in progress, one at a time");
  DEBUGAssetType = assetTypes + assetTypeId;

  struct asset_type *type = DEBUGAssetType;
  type->assetIndexFirst = assets->DEBUGUsedAssetCount;
  type->assetIndexOnePastLast = type->assetIndexFirst;
}

internal void
AddBitmapAsset(char *filename, struct v2 alignPercentage)
{
  assert(DEBUGAssetType && "you must call BeginAssetType()");
  assert(DEBUGAssetType->assetIndexOnePastLast < assetCount && "asset count exceeded");

  struct asset_type *type = DEBUGAssetType;
  DEBUGAsset = assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = DEBUGAsset;
  asset->tagIndexFirst = DEBUGUsedTagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;

  struct bitmap_id id = {DEBUGUsedBitmapInfoCount};
  DEBUGUsedBitmapInfoCount++;

  struct bitmap_info *info = bitmapInfos + id.value;
  info->filename = MemoryArenaPushString(&assets->arena, filename);
  info->alignPercentage = alignPercentage;

  asset->slotId = id;
}

internal struct asset *
AddAudioAssetTrimmed(char *filename, u32 sampleIndex, u32 sampleCount)
{
  assert(assets->DEBUGAssetType && "you must call BeginAssetType()");
  assert(assets->DEBUGAssetType->assetIndexOnePastLast < assets->assetCount && "asset count exceeded");

  struct asset_type *type = assets->DEBUGAssetType;
  DEBUGAsset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = DEBUGAsset;
  asset->tagIndexFirst = DEBUGUsedTagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;
  struct audio_id id = {DEBUGUsedAudioInfoCount};
  assets->DEBUGUsedAudioInfoCount++;

  struct audio_info *info = audioInfos + id.value;
  info->filename = MemoryArenaPushString(&assets->arena, filename);
  info->sampleIndex = sampleIndex;
  info->sampleCount = sampleCount;
  info->nextIdToPlay.value = 0;

  asset->slotId = id;

  return asset;
}

internal struct asset *
AddAudioAsset(char *filename)
{
  return AddAudioAssetTrimmed(filename, 0, 0);
}

internal void
AddAssetTag(enum asset_tag_id tagId, f32 value)
{
  assert(DEBUGAsset && "you must call one of Add...Asset()");
  assert(DEBUGAsset->tagIndexOnePastLast < assets->tagCount && "tag count exceeded");

  struct asset *asset = DEBUGAsset;
  struct asset_tag *tag = tags + DEBUGUsedTagCount;
  asset->tagIndexOnePastLast++;

  tag->id = tagId;
  tag->value = value;

  DEBUGUsedTagCount++;
}

internal void
EndAssetType()
{
  assert(assets->DEBUGAssetType && "cannot finish something that is not started");
  assets->DEBUGUsedAssetCount = assets->DEBUGAssetType->assetIndexOnePastLast;
  assets->DEBUGAssetType = 0;
  assets->DEBUGAsset = 0;
}

internal void
usage(void)
{
  fatal("hh_asset_builder [output]\n");
}

int
main(int argc, char *argv[])
{
  argc--;
  argv++;
  if (argc == 2) {
    usage();
    return 1;
  }

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

  BeginAssetType(assets, ASSET_TYPE_SHADOW);
  AddBitmapAsset(assets, "test/test_hero_shadow.bmp", v2(0.5f, 0.156682029f));
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TREE);
  AddBitmapAsset(assets, "test2/tree00.bmp", v2(0.493827164f, 0.295652181f));
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_SWORD);
  AddBitmapAsset(assets, "test2/rock03.bmp", v2(0.5f, 0.65625f));
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_GRASS);
  AddBitmapAsset(assets, "test2/grass00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/grass01.bmp", v2(0.5f, 0.5f));
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_GROUND);
  AddBitmapAsset(assets, "test2/ground00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground01.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground02.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground03.bmp", v2(0.5f, 0.5f));
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TUFT);
  AddBitmapAsset(assets, "test2/tuft00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/tuft01.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/tuft02.bmp", v2(0.5f, 0.5f));
  EndAssetType(assets);

  f32 angleRight = 0.00f * TAU32;
  f32 angleBack = 0.25f * TAU32;
  f32 angleLeft = 0.50f * TAU32;
  f32 angleFront = 0.75f * TAU32;

  BeginAssetType(assets, ASSET_TYPE_HEAD);

  AddBitmapAsset(assets, "test/test_hero_right_head.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_head.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_head.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_head.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TORSO);

  AddBitmapAsset(assets, "test/test_hero_right_torso.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_torso.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_torso.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_torso.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_CAPE);

  AddBitmapAsset(assets, "test/test_hero_right_cape.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_cape.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_cape.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_cape.bmp", v2(0.5f, 0.156682029f));
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(assets);

  // audios
  BeginAssetType(assets, ASSET_TYPE_BLOOP);
  AddAudioAsset(assets, "test3/bloop_00.wav");
  AddAudioAsset(assets, "test3/bloop_01.wav");
  AddAudioAsset(assets, "test3/bloop_02.wav");
  AddAudioAsset(assets, "test3/bloop_03.wav");
  AddAudioAsset(assets, "test3/bloop_04.wav");
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_CRACK);
  AddAudioAsset(assets, "test3/crack_00.wav");
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_DROP);
  AddAudioAsset(assets, "test3/drop_00.wav");
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_GLIDE);
  AddAudioAsset(assets, "test3/glide_00.wav");
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_MUSIC);
  u32 sampleRate = 48000;
  u32 chunkSampleCount = 10 * sampleRate;
  u32 totalSampleCount = 7468095;
  struct asset *lastMusic = 0;
  for (u32 sampleIndex = 0; sampleIndex < totalSampleCount; sampleIndex += chunkSampleCount) {
    u32 sampleCount = totalSampleCount - sampleIndex;
    if (sampleCount > chunkSampleCount) {
      sampleCount = chunkSampleCount;
    }
    struct asset *thisMusic = AddAudioAssetTrimmed(assets, "test3/music_test.wav", sampleIndex, sampleCount);
    if (lastMusic) {
      assets->audioInfos[lastMusic->slotId].nextIdToPlay.value = thisMusic->slotId;
    }
    lastMusic = thisMusic;
  }
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_PUHP);
  AddAudioAsset(assets, "test3/puhp_00.wav");
  AddAudioAsset(assets, "test3/puhp_01.wav");
  EndAssetType(assets);

  char *outFilename = "test.hha";
  if (argc == 1)
    outFilename = argv[1];

  int outFd = -1;
  outFd = open(outFilename, O_CREAT | O_WRONLY | O_TRUNC);
  if (outFd < 0) {
    fatal("cannot open file\n");
    return 2;
  }
  if (fchmod(outFd, 0644) < 0) {
    warning("chmod(644) failed\n");
  }

  int *list = NULL;
  arrput(list, 5);
  arrput(list, 9);
  arrput(list, 12);

  char buf[1024];
  for (u32 index = 0; index < arrlen(list); index++) {
    s64 len = sprintf(buf, "%d\n", list[index]);
    write(outFd, buf, (u64)len);
  }

  return 0;
}
