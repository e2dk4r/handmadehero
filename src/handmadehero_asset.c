#include <handmadehero/assert.h>
#include <handmadehero/asset.h>
#include <handmadehero/atomic.h>
#include <handmadehero/handmadehero.h> // BeginTaskWithMemory, EndTaskWithMemory
#include <handmadehero/platform.h>

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
  assert(id.value <= assets->assetCount);
  struct asset_slot *slot = assets->slots + id.value;

  if (slot->state != ASSET_STATE_LOADED)
    return 0;

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
    struct hha_asset *asset = assets->assets + assetIndex;

    f32 totalWeightedDiff = 0.0f;
    for (u32 tagIndex = asset->tagIndexFirst; tagIndex < asset->tagIndexOnePastLast; tagIndex++) {
      struct hha_tag *tag = assets->tags + tagIndex;

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
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState)
{
  struct game_assets *assets = MemoryArenaPush(arena, sizeof(*assets));

  MemorySubArenaInit(&assets->arena, arena, size);
  assets->transientState = transientState;

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

#if 0
  assets->tagCount = 0;
  assets->assetCount = 0;

  {
    struct platform_file_group fileGroup = Platform->GetAllFilesOfTypeBegin("hha");
    assets->fileCount = fileGroup.fileCount;
    assets->files = MemoryArenaPush(arena, sizeof(*assets->files) * assets->fileCount);
    for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
      struct asset_file *file = assets->files + fileIndex;
      file->handle = Platform->OpenFile(fileGroup, fileIndex);
      Platform->ReadFromFile(&file->header, file->handle, 0, sizeof(file->header));

      u64 assetTypeArraySize = sizeof(*file->assetTypes) * file->header.assetTypeCount;
      file->assetTypes = MemoryArenaPush(arena, assetTypeArraySize);
      Platform->ReadFromFile(file->assetTypes, file->handle, file->header.assetTypesOffset, assetTypeArraySize);

      if (Platform->HasFileError(file->handle)) {
        // TODO: notify user
        assert(0 && "file not read");
        continue;
      }

      struct hha_header *header = &file->header;
      if (header->magic != HHA_MAGIC) {
        // TODO: notify user
        assert(0 && "file is not hha");
        Platform->FileError(file->handle, "File is not hha.");
        continue;
      }

      if (header->version > HHA_VERSION) {
        // TODO: notify user
        assert(0 && "not supported version");
        Platform->FileError(file->handle, "Unsupported hha file version.");
        continue;
      }

      file->tagBase = assets->tagCount;

      assets->tagCount += header->tagCount;
      assets->assetCount += header->assetCount;
    }

    Platform->GetAllFilesOfTypeEnd(&fileGroup);
  }

  // NOTE: allocate memory for all metadatas from asset pack files
  assets->tags = MemoryArenaPush(arena, sizeof(*assets->tags) * assets->tagCount);
  assets->assets = MemoryArenaPush(arena, sizeof(*assets->assets) * assets->assetCount);
  assets->slots = MemoryArenaPush(arena, sizeof(*assets->slots) * assets->assetCount);

  // NOTE: load tags
  for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
    struct asset_file *file = assets->files + fileIndex;

    if (Platform->HasFileError(file->handle)) {
      continue;
    }

    struct hha_header *header = &file->header;
    struct hha_tag *dest = assets->tags + file->tagBase;
    u64 tagArraySize = header->tagCount * sizeof(*dest);
    Platform->ReadFromFile(dest, file->handle, 0, tagArraySize);
  }

  // TODO: Fast loading 100+ asset pack files

  // NOTE: merge asset pack files
  u32 assetCount = 0;
  for (u32 destAssetTypeId = 0; destAssetTypeId < ASSET_TYPE_COUNT; destAssetTypeId++) {
    struct asset_type *destType = assets->assetTypes + destAssetTypeId;
    destType->assetIndexFirst = assetCount;

    for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
      struct asset_file *file = assets->files + fileIndex;

      if (Platform->HasFileError(file->handle)) {
        continue;
      }

      struct hha_header *header = &file->header;

      for (u32 srcIndex = 0; srcIndex < file->header.assetTypeCount; srcIndex++) {
        struct hha_asset_type *srcType = file->assetTypes + srcIndex;
        if (srcType->typeId == destAssetTypeId) {
          u32 assetCountForType = srcType->assetIndexOnePastLast - srcType->assetIndexFirst;

          for (u32 assetIndex = assetCount; assetIndex < assetCount + assetCountForType; assetIndex++) {
            struct hha_asset *asset = assets->assets + assetIndex;
            asset->tagIndexFirst += file->tagBase;
            asset->tagIndexOnePastLast += file->tagBase;
          }

          struct hha_asset *dest = assets->assets + assetCount;
          u64 firstOffset = he > assetsOffset + (srcType->assetIndexFirst * sizeof(struct hha_asset));
          u64 assetArraySize = assetCountForType * sizeof(*dest);
          Platform->ReadFromFile(dest, fil
                                      assetCount += assetCountForType;
	  assert(assetCount <= assets->assetCount);
        }
      }
    }

    destType->assetIndexOnePastLast = assetCount;
  }

  assert(tagCount == assets->tagCount && "missing tags");
  assert(assetCount == assets->assetCount && "missing assets");
#endif

#if 1
  struct read_file_result readResult = Platform->ReadEntireFile("test.hha");
  if (readResult.size == 0) {
    return assets;
  }
  assets->hhaData = readResult.data;

  struct hha_header *header = readResult.data;
  assert(header->magic == HHA_MAGIC);
  assert(header->version == HHA_VERSION);

  assets->tagCount = header->tagCount;
  assets->tags = readResult.data + header->tagsOffset;

  struct hha_asset_type *hhaAssetTypes = readResult.data + header->assetTypesOffset;
  for (u32 assetTypeIndex = 0; assetTypeIndex < header->assetTypeCount; assetTypeIndex++) {
    struct hha_asset_type *src = hhaAssetTypes + assetTypeIndex;
    if (src->typeId >= ASSET_TYPE_COUNT)
      continue;
    struct asset_type *dest = assets->assetTypes + src->typeId;
    dest->assetIndexFirst = src->assetIndexFirst;
    dest->assetIndexOnePastLast = src->assetIndexOnePastLast;
  }

  assets->assetCount = header->assetCount;
  assets->assets = readResult.data + header->assetsOffset;
  assets->slots = MemoryArenaPush(arena, sizeof(*assets->slots) * assets->assetCount);
#endif

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

struct load_asset_work {
  struct platform_file_handle *handle;
  void *dest;
  u64 offset;
  u64 size;

  struct asset_slot *slot;
  enum asset_state finalState;
  struct task_with_memory *task;
};

internal void
DoLoadAssetWork(struct platform_work_queue *queue, void *data)
{
  struct load_asset_work *work = data;
  struct asset_slot *slot = work->slot;

#if 0
  Platform->ReadFromFile(work->dest, work->handle, work->offset, work->size);

  if (!Platform->HasFileError(work->handle)) {
    AtomicStore(&slot->state, work->finalState);
  }
#else
  // temporary bandaid
  AtomicStore(&slot->state, work->finalState);
#endif

  EndTaskWithMemory(work->task);
}

struct load_bitmap_work {
  struct task_with_memory *task;
  struct game_assets *assets;
  struct bitmap *bitmap;
  struct bitmap_id bitmapId;
  enum asset_state finalState;
};

internal void
DoLoadBitmapWork(struct platform_work_queue *queue, void *data)
{
  struct load_bitmap_work *work = data;
  struct game_assets *assets = work->assets;

  struct hha_asset *info = assets->assets + work->bitmapId.value;
  struct hha_bitmap *bitmapInfo = &info->bitmap;
  struct bitmap *bitmap = work->bitmap;

  bitmap->width = bitmapInfo->width;
  bitmap->height = bitmapInfo->height;
  bitmap->stride = (s32)(BITMAP_BYTES_PER_PIXEL * bitmap->width);
  bitmap->memory = assets->hhaData + info->dataOffset;

  bitmap->widthOverHeight = (f32)bitmap->width / (f32)bitmap->height;
  bitmap->alignPercentage = v2(bitmapInfo->alignPercentage[0], bitmapInfo->alignPercentage[1]);

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

  struct asset_slot *slot = assets->slots + id.value;
  struct hha_asset *info = assets->assets + id.value;
  assert(info->dataOffset && "asset not setup properly");

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&slot->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = BeginTaskWithMemory(assets->transientState);
    if (!task) {
      // memory cannot obtained, revert back
      AtomicStore(&slot->state, ASSET_STATE_UNLOADED);
      return;
    }

    // setup bitmap
    struct hha_asset *info = assets->assets + id.value;
    struct hha_bitmap *bitmapInfo = &info->bitmap;

    struct bitmap *bitmap = MemoryArenaPush(&assets->arena, sizeof(*bitmap));
    bitmap->width = bitmapInfo->width;
    bitmap->height = bitmapInfo->height;
    bitmap->stride = (s32)(BITMAP_BYTES_PER_PIXEL * bitmap->width);
#if 1
    // temporary bandaid
    bitmap->memory = assets->hhaData + info->dataOffset;
#else
    bitmap->memory = MemoryArenaPush(&assets->arena, (u32)bitmap->stride * bitmap->height);
#endif

    bitmap->widthOverHeight = (f32)bitmap->width / (f32)bitmap->height;
    bitmap->alignPercentage = v2(bitmapInfo->alignPercentage[0], bitmapInfo->alignPercentage[1]);
    slot->bitmap = bitmap;

    // setup work
    struct load_asset_work *work = MemoryArenaPush(&task->arena, sizeof(*work));
    work->handle = 0;
    work->dest = bitmap->memory;
    work->offset = info->dataOffset;
    work->size = (u32)bitmap->stride * bitmap->height;

    work->task = task;
    work->slot = slot;
    work->finalState = ASSET_STATE_LOADED;

    // queue the work
    struct platform_work_queue *queue = assets->transientState->lowPriorityQueue;
    Platform->WorkQueueAddEntry(queue, DoLoadAssetWork, work);
  }
  // else some other thread beat us to it
}

inline void
BitmapPrefetch(struct game_assets *assets, struct bitmap_id id)
{
  BitmapLoad(assets, id);
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
  struct game_assets *assets = work->assets;

  struct hha_asset *info = assets->assets + work->audioId.value;
  struct hha_audio *audioInfo = &info->audio;
  struct audio *audio = work->audio;
  audio->channelCount = audioInfo->channelCount;
  audio->sampleCount = audioInfo->sampleCount;
  audio->samples[0] = (s16 *)(assets->hhaData + info->dataOffset);
  audio->samples[1] = audio->samples[0] + audio->sampleCount;

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

  struct asset_slot *slot = assets->slots + id.value;
  struct hha_asset *info = assets->assets + id.value;
  assert(info->dataOffset && "asset not setup properly");

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&slot->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = BeginTaskWithMemory(assets->transientState);
    if (!task) {
      // memory cannot obtained, revert back
      AtomicStore(&slot->state, ASSET_STATE_UNLOADED);
      return;
    }

    // setup audio
    struct hha_asset *info = assets->assets + id.value;
    struct hha_audio *audioInfo = &info->audio;
    struct audio *audio = MemoryArenaPush(&assets->arena, sizeof(*audio));
    audio->channelCount = audioInfo->channelCount;
    audio->sampleCount = audioInfo->sampleCount;
    u64 sampleSize = audio->channelCount * audio->sampleCount * sizeof(*audio->samples[0]);
#if 1
    // temporary bandaid
    audio->samples[0] = (s16 *)(assets->hhaData + info->dataOffset);
    audio->samples[1] = audio->samples[0] + audio->sampleCount;
#else
    s16 *samples = MemoryArenaPush(&assets->arena, sampleSize);
    audio->samples[0] = samples;
    audio->samples[1] = audio->samples[0] + audio->sampleCount;
#endif
    slot->audio = audio;

    // setup work
    struct load_asset_work *work = MemoryArenaPush(&task->arena, sizeof(*work));
    // TODO: implement loading from packed asset file
    work->handle = 0;
    work->dest = audio->samples[0];
    work->offset = info->dataOffset;
    work->size = sampleSize;

    work->task = task;
    work->slot = slot;
    work->finalState = ASSET_STATE_LOADED;

    // queue the work
    struct platform_work_queue *queue = assets->transientState->lowPriorityQueue;
    Platform->WorkQueueAddEntry(queue, DoLoadAssetWork, work);
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
  assert(id.value <= assets->assetCount);
  struct asset_slot *slot = assets->slots + id.value;

  if (slot->state != ASSET_STATE_LOADED)
    return 0;

  return slot->audio;
}

inline struct hha_audio *
AudioInfoGet(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->assetCount);
  struct hha_asset *info = assets->assets + id.value;
  return &info->audio;
}

inline b32
IsAudioIdValid(struct audio_id id)
{
  return id.value != 0;
}
