#include <handmadehero/assert.h>
#include <handmadehero/asset.h>
#include <handmadehero/atomic.h>
#include <handmadehero/handmadehero.h> // BeginTaskWithMemory, EndTaskWithMemory

internal b32
IsAssetTypeIdBitmap(enum asset_type_id typeId)
{
  return typeId >= ASSET_TYPE_SHADOW && typeId <= ASSET_TYPE_CAPE;
}

internal b32
IsAssetTypeIdAudio(enum asset_type_id typeId)
{
  return typeId >= ASSET_TYPE_BLOOP && typeId <= ASSET_TYPE_PUHP;
}

inline struct bitmap *
BitmapGet(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->bitmapCount);
  struct asset_slot *slot = assets->bitmaps + id.value;
  return slot->bitmap;
}

#define BITMAP_COMPRESSION_RGB 0
#define BITMAP_COMPRESSION_BITFIELDS 3
struct __attribute__((packed)) bitmap_header {
  u16 fileType;
  u32 fileSize;
  u16 reserved1;
  u16 reserved2;
  u32 bitmapOffset;
  u32 size;
  i32 width;
  i32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 imageSize;
  u32 horzResolution;
  u32 vertResolution;
  u32 colorsPalette;
  u32 colorsImportant;
};

struct __attribute__((packed)) bitmap_header_compressed {
  struct bitmap_header header;
  u32 redMask;
  u32 greenMask;
  u32 blueMask;
};

internal struct bitmap
LoadBmp(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename, struct v2 alignPercentage)
{
  struct bitmap result = {0};

  struct read_file_result readResult = PlatformReadEntireFile(filename);
  if (readResult.size == 0) {
    return result;
  }

  struct bitmap_header *header = readResult.data;
  u8 *pixels = readResult.data + header->bitmapOffset;

  if (header->compression == BITMAP_COMPRESSION_BITFIELDS) {
    struct bitmap_header_compressed *cHeader = (struct bitmap_header_compressed *)header;

    i32 redShift = FindLeastSignificantBitSet((i32)cHeader->redMask);
    i32 greenShift = FindLeastSignificantBitSet((i32)cHeader->greenMask);
    i32 blueShift = FindLeastSignificantBitSet((i32)cHeader->blueMask);
    assert(redShift != greenShift);

    u32 alphaMask = ~(cHeader->redMask | cHeader->greenMask | cHeader->blueMask);
    i32 alphaShift = FindLeastSignificantBitSet((i32)alphaMask);

    u32 *srcDest = (u32 *)pixels;
    for (i32 y = 0; y < header->height; y++) {
      for (i32 x = 0; x < header->width; x++) {

        u32 value = *srcDest;

        // extract pixel from file
        struct v4 texel = v4((f32)((value >> redShift) & 0xff), (f32)((value >> greenShift) & 0xff),
                             (f32)((value >> blueShift) & 0xff), (f32)((value >> alphaShift) & 0xff));
        texel = sRGB255toLinear1(texel);

        /*
         * Store channels values pre-multiplied with alpha.
         */
        v3_mul_ref(&texel.rgb, texel.a);

        texel = Linear1tosRGB255(texel);
        *srcDest = (u32)(texel.a + 0.5f) << 0x18 | (u32)(texel.r + 0.5f) << 0x10 | (u32)(texel.g + 0.5f) << 0x08 |
                   (u32)(texel.b + 0.5f) << 0x00;

        srcDest++;
      }
    }
  }

  result.width = (u32)header->width;
  if (header->width < 0)
    result.width = (u32)-header->width;

  result.height = (u32)header->height;
  if (header->height < 0)
    result.height = (u32)-header->height;

  assert(result.width != 0);
  assert(result.height != 0);
  result.alignPercentage = alignPercentage;
  result.widthOverHeight = (f32)result.width / (f32)result.height;

  result.stride = (i32)result.width * BITMAP_BYTES_PER_PIXEL;
  result.memory = pixels;

  if (header->height < 0) {
    result.memory = pixels + (i32)(result.height - 1) * result.stride;
    result.stride = -result.stride;
  }

  return result;
}

internal u32
BestMatchAsset(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
               struct asset_vector *weightVector)
{
  u32 result = 0;

  f32 bestDiff = F32_MAX;
  struct asset_type *type = assets->assetTypes + typeId;
  for (u32 assetIndex = type->assetIndexFirst; assetIndex < type->assetIndexOnePastLast; assetIndex++) {
    struct asset *asset = assets->assets + assetIndex;

    f32 totalWeightedDiff = 0.0f;
    for (u32 tagIndex = asset->tagIndexFirst; tagIndex < asset->tagIndexOnePastLast; tagIndex++) {
      struct asset_tag *tag = assets->tags + tagIndex;

      f32 a = matchVector->e[tag->id];
      f32 b = tag->value;
      f32 d0 = Absolute(a - b);
      f32 d1 = Absolute(a - assets->tagRanges[tag->id] * SignOf(a) - b);
      f32 diff = Minimum(d0, d1);

      f32 weightedDiff = weightVector->e[tag->id] * diff;

      totalWeightedDiff += weightedDiff;
    }

    if (bestDiff > totalWeightedDiff) {
      bestDiff = totalWeightedDiff;
      result = asset->slotId;
    }
  }

  return result;
}

struct bitmap_id
BestMatchBitmap(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
                struct asset_vector *weightVector)
{
  assert(IsAssetTypeIdBitmap(typeId));
  struct bitmap_id result = {BestMatchAsset(assets, typeId, matchVector, weightVector)};
  return result;
}

struct audio_id
BestMatchAudio(struct game_assets *assets, enum asset_type_id typeId, struct asset_vector *matchVector,
               struct asset_vector *weightVector)
{
  assert(IsAssetTypeIdAudio(typeId));
  struct audio_id result = {BestMatchAsset(assets, typeId, matchVector, weightVector)};
  return result;
}

internal struct bitmap_id
BitmapInfoAdd(struct game_assets *assets, char *filename, struct v2 alignPercentage)
{
  struct bitmap_id id = {assets->DEBUGUsedBitmapInfoCount};
  assets->DEBUGUsedBitmapInfoCount++;

  struct bitmap_info *info = assets->bitmapInfos + id.value;
  info->filename = filename;
  info->alignPercentage = alignPercentage;

  return id;
}

internal struct bitmap_id
AudioInfoAdd(struct game_assets *assets, char *filename)
{
  struct bitmap_id id = {assets->DEBUGUsedAudioInfoCount};
  assets->DEBUGUsedAudioInfoCount++;

  struct audio_info *info = assets->audioInfos + id.value;
  info->filename = filename;
  info->nextIdToPlay.value = 0;

  return id;
}

internal void
BeginAssetType(struct game_assets *assets, enum asset_type_id assetTypeId)
{
  assert(assets->DEBUGAssetType == 0 && "another already in progress, one at a time");
  assets->DEBUGAssetType = assets->assetTypes + assetTypeId;

  struct asset_type *type = assets->DEBUGAssetType;
  type->assetIndexFirst = assets->DEBUGUsedAssetCount;
  type->assetIndexOnePastLast = type->assetIndexFirst;
}

internal void
AddBitmapAsset(struct game_assets *assets, char *filename, struct v2 alignPercentage)
{
  assert(assets->DEBUGAssetType && "you must call BeginAssetType()");
  assert(assets->DEBUGAssetType->assetIndexOnePastLast < assets->assetCount && "asset count exceeded");

  struct asset_type *type = assets->DEBUGAssetType;
  assets->DEBUGAsset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = assets->DEBUGAsset;
  asset->tagIndexFirst = assets->DEBUGUsedTagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;
  asset->slotId = BitmapInfoAdd(assets, filename, alignPercentage).value;
}

internal void
AddAudioAsset(struct game_assets *assets, char *filename)
{
  assert(assets->DEBUGAssetType && "you must call BeginAssetType()");
  assert(assets->DEBUGAssetType->assetIndexOnePastLast < assets->assetCount && "asset count exceeded");

  struct asset_type *type = assets->DEBUGAssetType;
  assets->DEBUGAsset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset *asset = assets->DEBUGAsset;
  asset->tagIndexFirst = assets->DEBUGUsedTagCount;
  asset->tagIndexOnePastLast = asset->tagIndexFirst;
  asset->slotId = AudioInfoAdd(assets, filename).value;
}

internal void
AddAssetTag(struct game_assets *assets, enum asset_tag_id tagId, f32 value)
{
  assert(assets->DEBUGAsset && "you must call one of Add...Asset()");
  assert(assets->DEBUGAsset->tagIndexOnePastLast < assets->tagCount && "tag count exceeded");

  struct asset *asset = assets->DEBUGAsset;
  struct asset_tag *tag = assets->tags + assets->DEBUGUsedTagCount;
  asset->tagIndexOnePastLast++;

  tag->id = tagId;
  tag->value = value;

  assets->DEBUGUsedTagCount++;
}

internal void
EndAssetType(struct game_assets *assets)
{
  assert(assets->DEBUGAssetType && "cannot finish something that is not started");
  assets->DEBUGUsedAssetCount = assets->DEBUGAssetType->assetIndexOnePastLast;
  assets->DEBUGAssetType = 0;
  assets->DEBUGAsset = 0;
}

inline struct game_assets *
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState,
                   pfnPlatformReadEntireFile PlatformReadEntireFile)
{
  struct game_assets *assets = MemoryArenaPush(arena, sizeof(*assets));

  MemorySubArenaInit(&assets->arena, arena, size);
  assets->PlatformReadEntireFile = PlatformReadEntireFile;
  assets->transientState = transientState;

  assets->bitmapCount = 256 * ASSET_TYPE_COUNT;
  assets->bitmaps = MemoryArenaPush(arena, sizeof(*assets->bitmaps) * assets->bitmapCount);
  assets->bitmapInfos = MemoryArenaPush(arena, sizeof(*assets->bitmapInfos) * assets->bitmapCount);

  assets->audioCount = 256 * ASSET_TYPE_COUNT;
  assets->audios = MemoryArenaPush(arena, sizeof(*assets->audios) * assets->audioCount);
  assets->audioInfos = MemoryArenaPush(arena, sizeof(*assets->audioInfos) * assets->audioCount);

  assets->tagCount = 1024 * ASSET_TYPE_COUNT;
  assets->tags = MemoryArenaPush(arena, sizeof(*assets->tags) * assets->tagCount);

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

  assets->assetCount = assets->bitmapCount;
  assets->assets = MemoryArenaPush(arena, sizeof(*assets->assets) * assets->assetCount);

  assets->DEBUGUsedBitmapInfoCount = 1;
  assets->DEBUGUsedAssetCount = 1;
  assets->DEBUGUsedTagCount = 1;

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
  AddAudioAsset(assets, "test3/music_test.wav");
  EndAssetType(assets);

  BeginAssetType(assets, ASSET_TYPE_PUHP);
  AddAudioAsset(assets, "test3/puhp_00.wav");
  AddAudioAsset(assets, "test3/puhp_01.wav");
  EndAssetType(assets);

  return assets;
}

u32
AssetGetFirstId(struct game_assets *assets, enum asset_type_id typeId)
{
  struct asset_type *type = assets->assetTypes + typeId;
  if (type->assetIndexFirst == type->assetIndexOnePastLast)
    return 0;

  struct asset *asset = assets->assets + type->assetIndexFirst;
  return asset->slotId;
}

struct bitmap_id
BitmapGetFirstId(struct game_assets *assets, enum asset_type_id typeId)
{
  assert(IsAssetTypeIdBitmap(typeId));
  struct bitmap_id result = {AssetGetFirstId(assets, typeId)};
  return result;
}

struct audio_id
AudioGetFirstId(struct game_assets *assets, enum asset_type_id typeId)
{
  assert(IsAssetTypeIdAudio(typeId));
  struct audio_id result = {AssetGetFirstId(assets, typeId)};
  return result;
}

internal u32
RandomAsset(struct random_series *series, struct game_assets *assets, enum asset_type_id typeId)
{
  struct asset_type *type = assets->assetTypes + typeId;
  u32 min = type->assetIndexFirst;
  u32 max = type->assetIndexOnePastLast; // exclusive
  u32 count = max - min;
  assert(count != 0 && "no bitmap asset added to this asset type");
  struct asset *asset = assets->assets + min + RandomChoice(series, count);
  return asset->slotId;
}

struct bitmap_id
RandomBitmap(struct random_series *series, struct game_assets *assets, enum asset_type_id typeId)
{
  assert(IsAssetTypeIdBitmap(typeId));
  struct bitmap_id result = {RandomAsset(series, assets, typeId)};
  return result;
}

struct audio_id
RandomAudio(struct random_series *series, struct game_assets *assets, enum asset_type_id typeId)
{
  assert(IsAssetTypeIdAudio(typeId));
  struct audio_id result = {RandomAsset(series, assets, typeId)};
  return result;
}

struct asset_load_bitmap_work {
  struct task_with_memory *task;
  struct game_assets *assets;
  struct bitmap *bitmap;
  struct bitmap_id bitmapId;
  enum asset_state finalState;
};

internal void
DoAssetLoadBitmapWork(struct platform_work_queue *queue, void *data)
{
  struct asset_load_bitmap_work *work = data;

  struct bitmap_info *info = work->assets->bitmapInfos + work->bitmapId.value;
  *work->bitmap = LoadBmp(work->assets->PlatformReadEntireFile, info->filename, info->alignPercentage);

  // TODO(e2dk4r): fence!
  struct asset_slot *slot = work->assets->bitmaps + work->bitmapId.value;
  slot->bitmap = work->bitmap;
  AtomicStore(&slot->state, work->finalState);

  EndTaskWithMemory(work->task);
}

inline void
BitmapLoad(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return;

  struct asset_slot *slot = assets->bitmaps + id.value;
  struct bitmap_info *info = assets->bitmapInfos + id.value;
  assert(info->filename && "asset not setup properly");

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&slot->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = BeginTaskWithMemory(assets->transientState);
    if (!task) {
      // memory cannot obtained, revert back
      AtomicStore(&slot->state, ASSET_STATE_UNLOADED);
      return;
    }

    struct asset_load_bitmap_work *work = MemoryArenaPush(&task->arena, sizeof(*work));
    work->task = task;
    work->assets = assets;
    work->bitmapId = id;
    work->bitmap = MemoryArenaPush(&assets->arena, sizeof(*work->bitmap));
    work->finalState = ASSET_STATE_LOADED;

    PlatformWorkQueueAddEntry(assets->transientState->lowPriorityQueue, DoAssetLoadBitmapWork, work);
  }
  // else some other thread beat us to it
}

inline void
BitmapPrefetch(struct game_assets *assets, struct bitmap_id id)
{
  BitmapLoad(assets, id);
}

struct wave_header {
  u32 riffId;
  u32 fileSize;
  u32 waveId;
} __attribute__((packed));

#define WAVE_CHUNKID(a, b, c, d) (a << 0x00 | b << 0x08 | c << 0x10 | d << 0x18)
enum {
  WAVE_CHUNKID_FMT = WAVE_CHUNKID('f', 'm', 't', ' '),
  WAVE_CHUNKID_RIFF = WAVE_CHUNKID('R', 'I', 'F', 'F'),
  WAVE_CHUNKID_WAVE = WAVE_CHUNKID('W', 'A', 'V', 'E'),
  WAVE_CHUNKID_DATA = WAVE_CHUNKID('d', 'a', 't', 'a'),
};
#undef WAVE_CHUNKID

struct wave_chunk {
  u32 id;
  u32 size;
} __attribute__((packed));

struct wave_fmt {
  u16 audioFormat;
  u16 numChannels;
  u32 sampleRate;
  u32 byteRate;
  u16 blockAlign;
  u16 bitsPerSample;
} __attribute__((packed));

#define WAVE_FORMAT_PCM 0x0001

struct wave_chunk_iterator {
  struct wave_chunk *chunk;
  void *eof;
};

internal struct wave_chunk_iterator
WaveChunkParse(struct wave_header *header)
{
  struct wave_chunk_iterator iterator = {};
  iterator.chunk = (void *)header + sizeof(*header);
  iterator.eof = (void *)header + sizeof(struct wave_chunk) + header->fileSize;
  return iterator;
}

internal void *
WaveChunkData(struct wave_chunk *chunk)
{
  return (void *)chunk + sizeof(*chunk);
}

internal b32
IsWaveChunkValid(struct wave_chunk_iterator iterator)
{
  return (void *)iterator.chunk != (void *)iterator.eof;
}

internal struct wave_chunk_iterator
WaveChunkNext(struct wave_chunk_iterator iterator)
{
  iterator.chunk = (void *)iterator.chunk + sizeof(*iterator.chunk) + ALIGN(iterator.chunk->size, 2);
  return iterator;
}

internal struct audio
LoadWav(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename)
{
  struct audio result = {};

  struct read_file_result readResult = PlatformReadEntireFile(filename);
  if (readResult.size == 0) {
    return result;
  }

  struct wave_header *header = readResult.data;
  assert(header->riffId == WAVE_CHUNKID_RIFF);
  assert(header->waveId == WAVE_CHUNKID_WAVE);

  struct wave_fmt *fmt = 0;
  i16 *sampleData = 0;
  u32 sampleDataSize = 0;
  for (struct wave_chunk_iterator iterator = WaveChunkParse(header); IsWaveChunkValid(iterator);
       iterator = WaveChunkNext(iterator)) {
    if (iterator.chunk->id != WAVE_CHUNKID_FMT)
      continue;

    fmt = WaveChunkData(iterator.chunk);
    assert(fmt->audioFormat == WAVE_FORMAT_PCM);
    assert(fmt->sampleRate == 48000);
    assert(fmt->bitsPerSample == 16);
    assert(fmt->blockAlign == sizeof(u16) * fmt->numChannels);

    iterator = WaveChunkNext(iterator);
    assert(iterator.chunk->id == WAVE_CHUNKID_DATA && "malformed file");
    sampleData = (i16 *)WaveChunkData(iterator.chunk);
    sampleDataSize = iterator.chunk->size;

    break;
  }

  assert(fmt && sampleData);

  result.channelCount = fmt->numChannels;
  result.sampleCount = sampleDataSize / (u32)(fmt->numChannels * sizeof(u16));
  switch (fmt->numChannels) {
  case 1:
    result.samples[0] = sampleData;
    result.samples[1] = 0;
    break;

  case 2:
    result.samples[0] = sampleData;
    result.samples[1] = sampleData + result.sampleCount;

#if 0
    for (u32 sampleIndex = 0; sampleIndex < result.sampleCount; sampleIndex++) {
      sampleData[sampleIndex * 2 + 0] = (i16)sampleIndex;
      sampleData[sampleIndex * 2 + 1] = (i16)sampleIndex;
    }
#endif

    for (u32 sampleIndex = 0; sampleIndex < result.sampleCount; sampleIndex++) {
      i16 source = sampleData[sampleIndex * 2];
      sampleData[sampleIndex * 2] = sampleData[sampleIndex];
      sampleData[sampleIndex] = source;
    }

    // TODO(e2dk4r): load right channels
    break;

  default:
    assert(0 && "Unsupported number of channels");
  }

  return result;
}

struct load_audio_work {
  struct task_with_memory *task;
  struct game_assets *assets;
  struct audio *audio;
  struct audio_id audioId;
  enum asset_state finalState;
};

internal void
DoLoadAudioWork(struct platform_work_queue *queue, void *data)
{
  struct load_audio_work *work = data;

  struct audio_info *info = work->assets->audioInfos + work->audioId.value;
  *work->audio = LoadWav(work->assets->PlatformReadEntireFile, info->filename);

  // TODO(e2dk4r): fence!
  struct asset_slot *slot = work->assets->audios + work->audioId.value;
  slot->audio = work->audio;
  AtomicStore(&slot->state, work->finalState);

  EndTaskWithMemory(work->task);
}

inline void
AudioLoad(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return;

  struct asset_slot *slot = assets->audios + id.value;
  struct audio_info *info = assets->audioInfos + id.value;
  assert(info->filename && "asset not setup properly");

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&slot->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = BeginTaskWithMemory(assets->transientState);
    if (!task) {
      // memory cannot obtained, revert back
      AtomicStore(&slot->state, ASSET_STATE_UNLOADED);
      return;
    }

    struct load_audio_work *work = MemoryArenaPush(&task->arena, sizeof(*work));
    work->task = task;
    work->assets = assets;
    work->audioId = id;
    work->audio = MemoryArenaPush(&assets->arena, sizeof(*work->audio));
    work->finalState = ASSET_STATE_LOADED;

    PlatformWorkQueueAddEntry(assets->transientState->lowPriorityQueue, DoLoadAudioWork, work);
  }
  // else some other thread beat us to it
}

inline void
AudioPrefetch(struct game_assets *assets, struct audio_id id)
{
  AudioLoad(assets, id);
}

inline struct audio *
AudioGet(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->audioCount);
  struct asset_slot *slot = assets->audios + id.value;
  return slot->audio;
}

inline struct audio_info *
AudioInfoGet(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->DEBUGUsedAudioInfoCount);
  struct audio_info *info = assets->audioInfos + id.value;
  return info;
}

inline b32
IsAudioIdValid(struct audio_id id)
{
  return id.value != 0;
}
