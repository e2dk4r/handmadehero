#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#pragma GCC diagnostic push

// caused by: stb_ds.h
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <fcntl.h>
#include <unistd.h>

#include <handmadehero/assert.h>
#include <handmadehero/asset_type_id.h>
#include <handmadehero/fileformats.h>
#include <handmadehero/types.h>

// #define STB_DS_IMPLEMENTATION
// #include "stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#define sprintf stbsp_sprintf
#define snprintf stbsp_snprintf

#pragma GCC diagnostic pop

comptime f32 TAU32 = 6.28318530717958647692f;

/*****************************************************************
 * LOGGING
 *****************************************************************/

internal void
info(char *buf, u64 count)
{
  write(STDOUT_FILENO, buf, count);
}

internal void
warn(char *buf, u64 count)
{
  write(STDOUT_FILENO, "warn: ", 6);
  write(STDOUT_FILENO, buf, count);
}

internal void
fatal(char *buf, u64 count)
{
  write(STDERR_FILENO, buf, count);
}

/*****************************************************************
 * STRUCTURES
 *****************************************************************/

struct bitmap_id {
  u32 value;
};

struct audio_id {
  u32 value;
};

struct bitmap_info {
  char *filename;
  f32 alignPercentageX;
  f32 alignPercentageY;
};

struct audio_info {
  char *filename;
  u32 sampleIndex;
  u32 sampleCount;
  struct audio_id nextIdToPlay;
};

enum asset_tag_id {
  ASSET_TAG_SMOOTHNESS,
  ASSET_TAG_FLATNESS,
  ASSET_TAG_FACING_DIRECTION, // angle in radians clockwise

  ASSET_TAG_COUNT
};

struct asset {
  u32 tagIndexFirst;
  u32 tagIndexOnePastLast;

  union {
    struct bitmap_info bitmapInfo;
    struct audio_info audioInfo;
  };
};

struct asset_type {
  u32 assetIndexFirst;
  u32 assetIndexOnePastLast;
};

#define TAG_COUNT 0x1000
#define ASSET_COUNT 0x1000

struct game_assets {
  struct asset_slot *slots;

  u32 tagCount;
  struct hha_tag tags[TAG_COUNT];
  f32 tagRanges[ASSET_TYPE_COUNT];

  u32 assetCount;
  struct asset assets[ASSET_COUNT];

  u32 assetTypeCount;
  struct hha_asset_type assetTypes[ASSET_TYPE_COUNT];

  struct hha_asset_type *DEBUGAssetType;
  struct asset *DEBUGAsset;
};

/*****************************************************************
 * FUNCTIONS
 *****************************************************************/

internal inline b32
IsAudioIdValid(struct audio_id id)
{
  return id.value != 0;
}

internal void
BeginAssetType(struct game_assets *assets, enum asset_type_id typeId)
{
  assert(assets->DEBUGAssetType == 0 && "another already in progress, one at a time");
  assets->DEBUGAssetType = assets->assetTypes + typeId;

  struct hha_asset_type *type = assets->DEBUGAssetType;
  type->typeId = typeId;
  type->assetIndexFirst = assets->assetCount;
  type->assetIndexOnePastLast = type->assetIndexFirst;
}

internal struct bitmap_id
AddBitmapAsset(struct game_assets *assets, char *filename, f32 alignPercentageX, f32 alignPercentageY)
{
  assert(assets->DEBUGAssetType && "you must call BeginAssetType()");
  assert(assets->DEBUGAssetType->assetIndexOnePastLast < ARRAY_COUNT(assets->assets) && "asset count exceeded");

  struct hha_asset_type *type = assets->DEBUGAssetType;
  assets->DEBUGAsset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = assets->DEBUGAsset;
  asset->tagIndexFirst = assets->tagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;

  struct bitmap_id id = {assets->assetCount};
  assets->assetCount++;

  struct bitmap_info *info = &(assets->assets + id.value)->bitmapInfo;
  info->filename = filename;
  info->alignPercentageX = alignPercentageX;
  info->alignPercentageY = alignPercentageY;

  return id;
}

internal struct audio_id
AddAudioAssetTrimmed(struct game_assets *assets, char *filename, u32 sampleIndex, u32 sampleCount)
{
  assert(assets->DEBUGAssetType && "you must call BeginAssetType()");
  assert(assets->DEBUGAssetType->assetIndexOnePastLast < ARRAY_COUNT(assets->assets) && "asset count exceeded");

  struct hha_asset_type *type = assets->DEBUGAssetType;
  assets->DEBUGAsset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = assets->DEBUGAsset;
  asset->tagIndexFirst = assets->tagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;

  struct audio_id id = {assets->assetCount};
  assets->assetCount++;

  struct audio_info *info = &(assets->assets + id.value)->audioInfo;
  info->filename = filename;
  info->sampleIndex = sampleIndex;
  info->sampleCount = sampleCount;
  info->nextIdToPlay.value = 0;

  return id;
}

internal struct audio_id
AddAudioAsset(struct game_assets *assets, char *filename)
{
  return AddAudioAssetTrimmed(assets, filename, 0, 0);
}

internal void
AddAssetTag(struct game_assets *assets, enum asset_tag_id tagId, f32 value)
{
  assert(assets->DEBUGAsset && "you must call one of Add...Asset()");
  assert(assets->DEBUGAsset->tagIndexOnePastLast < ARRAY_COUNT(assets->tags) && "tag count exceeded");

  struct asset *asset = assets->DEBUGAsset;
  struct hha_tag *tag = assets->tags + assets->tagCount;
  asset->tagIndexOnePastLast++;

  tag->id = tagId;
  tag->value = value;

  assets->tagCount++;
}

internal void
EndAssetType(struct game_assets *assets)
{
  assert(assets->DEBUGAssetType && "cannot finish something that is not started");
  assets->assetCount = assets->DEBUGAssetType->assetIndexOnePastLast;
  assets->DEBUGAssetType = 0;
  assets->DEBUGAsset = 0;
}

internal void
usage(void)
{
  // clang-format off
  char usageMessage[] =
      "hh_asset_builder [output]" "\n"
      "\n"

      "  output @type    filename" "\n"
      "         @default test.hha" "\n"
      "\n"
  ;

  // clang-format on
  fatal(usageMessage, ARRAY_COUNT(usageMessage) - 1);
}

/*****************************************************************
 * STARTING POINT
 *****************************************************************/

int
main(int argc, char *argv[])
{
  // argc 0 is program path
  char *exepath = argv[0];
  argc--;
  argv++;

  // parse arguments
  if (argc >= 2) {
    usage();
    return 1;
  }

  char *outFilename = "test.hha";
  if (argc == 1)
    outFilename = argv[1];

  /*----------------------------------------------------------------
   * INGEST
   *----------------------------------------------------------------*/
  struct game_assets *assets = &(struct game_assets){};

  assets->tagCount = 1;
  assets->assetCount = 1;

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

  BeginAssetType(assets, ASSET_TYPE_SHADOW);
  AddBitmapAsset(assets, "test/test_hero_shadow.bmp", 0.5f, 0.156682029f);
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TREE);
  AddBitmapAsset(assets, "test2/tree00.bmp", 0.493827164f, 0.295652181f);
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_SWORD);
  AddBitmapAsset(assets, "test2/rock03.bmp", 0.5f, 0.65625f);
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_GRASS);
  AddBitmapAsset(assets, "test2/grass00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/grass01.bmp", 0.5f, 0.5f);
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_GROUND);
  AddBitmapAsset(assets, "test2/ground00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/ground01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/ground02.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/ground03.bmp", 0.5f, 0.5f);
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TUFT);
  AddBitmapAsset(assets, "test2/tuft00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/tuft01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(assets, "test2/tuft02.bmp", 0.5f, 0.5f);
  EndAssetType(assets);

  f32 angleRight = 0.00f * TAU32;
  f32 angleBack = 0.25f * TAU32;
  f32 angleLeft = 0.50f * TAU32;
  f32 angleFront = 0.75f * TAU32;

  BeginAssetType(assets, ASSET_TYPE_HEAD);

  AddBitmapAsset(assets, "test/test_hero_right_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_TORSO);

  AddBitmapAsset(assets, "test/test_hero_right_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_CAPE);

  AddBitmapAsset(assets, "test/test_hero_right_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(assets, "test/test_hero_back_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(assets, "test/test_hero_left_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(assets, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(assets, "test/test_hero_front_cape.bmp", 0.5f, 0.156682029f);
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
  struct audio_id lastMusic = {};
  for (u32 sampleIndex = 0; sampleIndex < totalSampleCount; sampleIndex += chunkSampleCount) {
    u32 sampleCount = totalSampleCount - sampleIndex;
    if (sampleCount > chunkSampleCount) {
      sampleCount = chunkSampleCount;
    }
    struct audio_id thisMusic = AddAudioAssetTrimmed(assets, "test3/music_test.wav", sampleIndex, sampleCount);
    if (IsAudioIdValid(lastMusic)) {
      assets->assets[lastMusic.value].audioInfo.nextIdToPlay = thisMusic;
    }
    lastMusic = thisMusic;
  }
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_PUHP);
  AddAudioAsset(assets, "test3/puhp_00.wav");
  AddAudioAsset(assets, "test3/puhp_01.wav");
  EndAssetType(assets);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/

  char logBuffer[256];
  s64 logLength;

  int outFd = -1;
  outFd = open(outFilename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (outFd < 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot open file\n  filename: %s\n", outFilename);
    assert(logLength > 0);
    fatal(logBuffer, (u64)logLength);
    return 2;
  }

  struct hha_header header = {
      .magic = HHA_MAGIC,
      .version = HHA_VERSION,
  };

  header.tagCount = assets->tagCount - 1;     // 0 is empty
  header.assetCount = assets->assetCount - 1; // 0 is empty
  // TODO: sparseness?
  header.assetTypeCount = ASSET_TYPE_COUNT;
  header.tagsOffset = sizeof(header);
  header.assetTypesOffset = header.tagsOffset + (header.tagCount * sizeof(*assets->tags));
  header.assetsOffset = header.assetTypesOffset + (header.assetTypeCount * sizeof(*assets->assetTypes));

  s64 writtenBytes = write(outFd, &header, sizeof(header));
  assert(writtenBytes > 0);

  // 1 - tags
  struct hha_tag *tags = assets->tags;
  tags++; // 0 is reserved for null
  u64 tagArraySize = sizeof(*tags) * header.tagCount;
  writtenBytes = write(outFd, tags, tagArraySize);
  assert(writtenBytes > 0);

  logLength = snprintf(logBuffer, sizeof(logBuffer), "%u tags written\n", header.tagCount);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

  // 2 - assetTypes
  struct hha_asset_type *assetTypes = assets->assetTypes;
  u64 assetTypeArraySize = sizeof(*assetTypes) * header.assetTypeCount;
  writtenBytes = write(outFd, assetTypes, assetTypeArraySize);
  assert(writtenBytes > 0);

  logLength = snprintf(logBuffer, sizeof(logBuffer), "%u asset types written\n", header.assetTypeCount);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

  // 3 - assets
  // u64 assetArraySize = sizeof(struct hha_asset) * header.assetCount;
  // writtenBytes = write(outFd, &assets, assetArraySize);
  // assert(writtenBytes > 0);

  // logLength = snprintf(logBuffer, sizeof(logBuffer), "%u assets written\n", header.assetCount);
  // assert(logLength > 0);
  // info(logBuffer, (u64)logLength);

  s32 fsyncResult = fsync(outFd);
  if (fsyncResult != 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot sync data\n");
    assert(logLength > 0);
    warn(logBuffer, (u64)logLength);
  }

  s32 closeResult = close(outFd);
  if (closeResult != 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot close the file\n");
    assert(logLength > 0);
    warn(logBuffer, (u64)logLength);
  }
  outFd = -1;

  logLength = snprintf(logBuffer, sizeof(logBuffer), "Assets are packed into '%s' successfully.\n", outFilename);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

  return 0;
}
