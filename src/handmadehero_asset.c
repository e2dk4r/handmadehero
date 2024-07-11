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

  assert(id.value <= assets->assetCount);
  struct asset_slot *slot = assets->slots + id.value;
  return slot->bitmap;
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
      result = assetIndex;
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

inline struct game_assets *
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState,
                   pfnPlatformReadEntireFile PlatformReadEntireFile)
{
  struct game_assets *assets = MemoryArenaPush(arena, sizeof(*assets));

  MemorySubArenaInit(&assets->arena, arena, size);
  assets->PlatformReadEntireFile = PlatformReadEntireFile;
  assets->transientState = transientState;

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

  struct read_file_result readResult = PlatformReadEntireFile("test.hha");
  if (readResult.size == 0) {
    return assets;
  }

  struct hha_header *header = readResult.data;
  assert(header->magic == HHA_MAGIC);
  assert(header->version == HHA_VERSION);

  assets->assetCount = 256 * ASSET_TYPE_COUNT;
  assets->assets = MemoryArenaPush(arena, sizeof(*assets->assets) * assets->assetCount);
  assets->slots = MemoryArenaPush(arena, sizeof(*assets->slots) * assets->assetCount);

  assets->tagCount = header->tagCount;
  assets->tags = MemoryArenaPush(arena, sizeof(*assets->tags) * assets->tagCount);
  struct hha_tag *hhaTags = readResult.data + header->tagsOffset;
  for (u32 tagIndex = 0; tagIndex < header->tagCount; tagIndex++) {
    struct hha_tag *src = hhaTags + tagIndex;
    struct asset_tag *dest = assets->tags + tagIndex;
    dest->id = src->value;
    dest->value = src->value;
  }

  struct hha_asset_type *hhaAssetTypes = readResult.data + header->assetTypesOffset;
  for (u32 assetTypeIndex = 0; assetTypeIndex < header->assetTypeCount; assetTypeIndex++) {
    struct hha_asset_type *src = hhaAssetTypes + assetTypeIndex;
    if (src->typeId > ASSET_TYPE_COUNT)
      continue;
    struct asset_type *dest = assets->assetTypes + src->typeId;
    dest->assetIndexFirst = src->assetIndexFirst;
    dest->assetIndexOnePastLast = src->assetIndexOnePastLast;
  }

  // assets->assetCount = ;
  // assets->assets = ;

  return assets;
}

u32
AssetGetFirstId(struct game_assets *assets, enum asset_type_id typeId)
{
  struct asset_type *type = assets->assetTypes + typeId;
  if (type->assetIndexFirst == type->assetIndexOnePastLast)
    return 0;

  return type->assetIndexFirst;
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
  u32 assetIndex = min + RandomChoice(series, count);
  return assetIndex;
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

internal struct bitmap
LoadBmp(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename, struct v2 alignPercentage)
{
  assert(0 && "not implemented");
  struct bitmap result = {};
  return result;
};

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

  struct bitmap_info *info = &(work->assets->assets + work->bitmapId.value)->bitmapInfo;
  *work->bitmap = LoadBmp(work->assets->PlatformReadEntireFile, info->filename, info->alignPercentage);

  // TODO(e2dk4r): fence!
  struct asset_slot *slot = work->assets->slots + work->bitmapId.value;
  slot->bitmap = work->bitmap;
  AtomicStore(&slot->state, work->finalState);

  EndTaskWithMemory(work->task);
}

inline void
BitmapLoad(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return;

  // TODO: implement loading from packed asset file
  return;

  struct asset_slot *slot = assets->slots + id.value;
  struct bitmap_info *info = &(assets->assets + id.value)->bitmapInfo;
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

internal struct audio
LoadWav(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename, u32 sectionSampleIndex,
        u32 sectionSampleCount)
{
  assert(0 && "not implemented");
  struct audio result = {};
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

  struct audio_info *info = &(work->assets->assets + work->audioId.value)->audioInfo;
  *work->audio = LoadWav(work->assets->PlatformReadEntireFile, info->filename, info->sampleIndex, info->sampleCount);

  // TODO(e2dk4r): fence!
  struct asset_slot *slot = work->assets->slots + work->audioId.value;
  slot->audio = work->audio;
  AtomicStore(&slot->state, work->finalState);

  EndTaskWithMemory(work->task);
}

inline void
AudioLoad(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return;

  // TODO: implement loading from packed asset file
  return;

  struct asset_slot *slot = assets->slots + id.value;
  struct audio_info *info = &(assets->assets + id.value)->audioInfo;
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

  assert(id.value <= assets->assetCount);
  struct asset_slot *slot = assets->slots + id.value;
  return slot->audio;
}

inline struct audio_info *
AudioInfoGet(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->assetCount);
  struct audio_info *info = &(assets->assets + id.value)->audioInfo;
  return info;
}

inline b32
IsAudioIdValid(struct audio_id id)
{
  return id.value != 0;
}
