#include <handmadehero/assert.h>
#include <handmadehero/asset.h>
#include <handmadehero/atomic.h>
#include <handmadehero/handmadehero.h> // BeginTaskWithMemory, EndTaskWithMemory
#include <handmadehero/platform.h>

internal b32
IsAssetTypeIdBitmap(enum asset_type_id typeId)
{
  return typeId >= ASSET_TYPE_SHADOW && typeId <= ASSET_TYPE_FONT;
}

internal b32
IsAssetTypeIdAudio(enum asset_type_id typeId)
{
  return typeId >= ASSET_TYPE_BLOOP && typeId <= ASSET_TYPE_PUHP;
}

struct asset_memory_size {
  u32 section;
  u32 data;
  u32 total;
};

internal void
InsertAssetHeaderToFront(struct game_assets *assets, struct asset_memory_header *header)
{
  struct asset_memory_header *sentinel = &assets->loadedAssetSentiel;

  // insert header to front
  header->prev = sentinel;
  header->next = sentinel->next;

  header->next->prev = header;
  header->prev->next = header;
}

internal void
RemoveAssetHeaderFromList(struct asset_memory_header *header)
{
  header->next->prev = header->prev;
  header->prev->next = header->next;

  header->next = header->prev = 0;
}

internal void
BeginAssetLock(struct game_assets *assets)
{
  u32 expected = 0;
  u32 desired = 1;
  while (1) {
    if (AtomicCompareExchange(&assets->operationLock, &expected, desired))
      break;
  }
}

internal void
EndAssetLock(struct game_assets *assets)
{
  u32 desired = 0;
  AtomicStore(&assets->operationLock, desired);
}

internal struct asset_memory_header *
AssetGet(struct game_assets *assets, u32 assetIndex, u32 generationId)
{
  assert(assetIndex <= assets->assetCount);
  struct asset *asset = assets->assets + assetIndex;

  struct asset_memory_header *header = 0;

  BeginAssetLock(assets);

  if (asset->state == ASSET_STATE_LOADED) {
    header = asset->header;
    RemoveAssetHeaderFromList(header);
    InsertAssetHeaderToFront(assets, header);

    if (header->generationId < generationId) {
      header->generationId = generationId;
    }
  }

  EndAssetLock(assets);

  return header;
}

inline struct bitmap *
BitmapGet(struct game_assets *assets, struct bitmap_id id, u32 generationId)
{
  struct asset_memory_header *header = AssetGet(assets, id.value, generationId);
  struct bitmap *bitmap = header ? &header->bitmap : 0;
  return bitmap;
}

inline struct audio *
AudioGet(struct game_assets *assets, struct audio_id id, u32 generationId)
{
  struct asset_memory_header *header = AssetGet(assets, id.value, generationId);
  struct audio *audio = header ? &header->audio : 0;
  return audio;
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
    for (u32 tagIndex = asset->hhaAsset.tagIndexFirst; tagIndex < asset->hhaAsset.tagIndexOnePastLast; tagIndex++) {
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

internal struct platform_file_handle *
AssetFileHandleGet(struct game_assets *assets, u32 fileIndex)
{
  assert(fileIndex < assets->fileCount);
  struct asset_file *file = (assets->files + fileIndex);
  struct platform_file_handle *handle = &file->handle;
  return handle;
}

internal struct asset_memory_block *
InsertMemoryBlock(struct asset_memory_block *prev, void *memory, u64 size)
{
  struct asset_memory_block *block = memory;
  assert(size > sizeof(*block));
  block->size = size - sizeof(*block);
  block->flags = 0;

  block->prev = prev;
  block->next = prev->next;

  block->prev->next = block;
  block->next->prev = block;

  return block;
}

internal struct asset_memory_block *
FindMemoryBlockForSize(struct game_assets *assets, memory_arena_size_t size)
{
  struct asset_memory_block *found = 0;

  for (struct asset_memory_block *block = assets->memorySentiel.next; block != &assets->memorySentiel;
       block = block->next) {
    if (block->flags & ASSET_MEMORY_BLOCK_USED)
      continue;

    if (block->size < size)
      continue;

    found = block;
    break;
  }

  return found;
}

internal b32
MergeMemoryBlock(struct game_assets *assets, struct asset_memory_block *first, struct asset_memory_block *second)
{
  b32 isMerged = 0;
  if (first == &assets->memorySentiel || second == &assets->memorySentiel)
    return isMerged;

  if (first->flags & ASSET_MEMORY_BLOCK_USED || second->flags & ASSET_MEMORY_BLOCK_USED)
    return isMerged;

  // expected to be continuous space
  u8 *expectedSecond = (u8 *)first + sizeof(*first) + first->size;
  if ((u8 *)second != expectedSecond)
    return isMerged;

  // detach memory block from list
  second->next->prev = second->prev;
  second->prev->next = second->next;

  // notify that first is bigger now
  first->size += sizeof(*second) + second->size;

  isMerged = 1;

  return isMerged;
}

internal b32
HasGenerationCompleted(struct game_assets *assets, u32 generationId)
{
  b32 isCompleted = 1;

  for (u32 index = 0; index < assets->inFlightGenerationCount; index++) {
    if (assets->inFlightGenerations[index] == generationId) {
      isCompleted = 0;
      break;
    }
  }

  return isCompleted;
}

internal struct asset_memory_header *
AcquireAssetMemory(struct game_assets *assets, memory_arena_size_t size, u32 assetIndex)
{
  struct asset_memory_header *result = 0;

  BeginAssetLock(assets);

  for (;;) {
    struct asset_memory_block *block = FindMemoryBlockForSize(assets, size);
    if (block && size <= block->size) {
      block->flags |= ASSET_MEMORY_BLOCK_USED;

      result = (struct asset_memory_header *)(block + 1);

      u64 remainingSize = block->size - size;
      u64 memoryBlockSplitThreshold = 4 * KILOBYTES;
      if (remainingSize > memoryBlockSplitThreshold) {
        block->size -= remainingSize;
        InsertMemoryBlock(block, (u8 *)result + size, remainingSize);
      }

      break;
    } else { // if memory block for size NOT found
      for (struct asset_memory_header *header = assets->loadedAssetSentiel.prev; header != &assets->loadedAssetSentiel;
           header = header->prev) {
        struct asset *asset = assets->assets + header->assetIndex;
        if (asset->state < ASSET_STATE_LOADED && !HasGenerationCompleted(assets, asset->header->generationId)) {
          continue;
        }

        // evict asset
        assert(asset->state == ASSET_STATE_LOADED);

        RemoveAssetHeaderFromList(header);

        // release asset memory
        struct asset_memory_block *block = (struct asset_memory_block *)((u8 *)header - sizeof(*block));
        block->flags &= (u64)(~ASSET_MEMORY_BLOCK_USED);

        if (MergeMemoryBlock(assets, block->prev, block)) {
          block = block->prev;
        }

        MergeMemoryBlock(assets, block, block->next);

        asset->state = ASSET_STATE_UNLOADED;
        asset->header = 0;
        break;
      }
    }
  }

  if (result) {
    // AddAssetHeaderToList
    result->assetIndex = assetIndex;
    InsertAssetHeaderToFront(assets, result);
  }

  EndAssetLock(assets);

  return result;
}

inline struct game_assets *
GameAssetsAllocate(struct memory_arena *arena, memory_arena_size_t size, struct transient_state *transientState)
{
  struct game_assets *assets = MemoryArenaPush(arena, sizeof(*assets));

  assets->nextGenerationId = 0;
  assets->inFlightGenerationCount = 0;
  ZeroMemory(assets->inFlightGenerations, sizeof(assets->inFlightGenerations));

  assets->memorySentiel.prev = &assets->memorySentiel;
  assets->memorySentiel.next = &assets->memorySentiel;
  assets->memorySentiel.flags = 0;
  assets->memorySentiel.size = 0;

  InsertMemoryBlock(&assets->memorySentiel, MemoryArenaPush(arena, size), (u64)size);

  assets->transientState = transientState;

  assets->loadedAssetSentiel.next = assets->loadedAssetSentiel.prev = &assets->loadedAssetSentiel;

  for (u32 tagType = 0; tagType < ASSET_TAG_COUNT; tagType++) {
    assets->tagRanges[tagType] = 1000000.0f;
  }
  assets->tagRanges[ASSET_TAG_FACING_DIRECTION] = TAU32;

  assets->tagCount = 0;
  assets->assetCount = 1;

  {
    struct platform_file_group fileGroup = Platform->GetAllFilesOfTypeBegin(PLATFORM_FILE_TYPE_ASSET_FILE);
    assets->fileCount = fileGroup.fileCount;
    assets->files = MemoryArenaPush(arena, sizeof(*assets->files) * assets->fileCount);
    for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
      struct asset_file *file = assets->files + fileIndex;

      file->handle = Platform->OpenNextFile(&fileGroup);
      Platform->ReadFromFile(&file->header, &file->handle, 0, sizeof(file->header));
      if (Platform->HasFileError(&file->handle)) {
        // TODO: notify user
        assert(0 && "file not opened or read");
        continue;
      }

      struct hha_header *header = &file->header;
      if (header->magic != HHA_MAGIC) {
        // TODO: notify user
        assert(0 && "file is not hha");
        Platform->FileError(&file->handle, HANDMADEHERO_ERROR_FILE_IS_NOT_HHA);
        continue;
      }

      if (header->version > HHA_VERSION) {
        // TODO: notify user
        assert(0 && "not supported version");
        Platform->FileError(&file->handle, HANDMADEHERO_ERROR_HHA_VERSION_IS_NOT_SUPPORTED);
        continue;
      }

      u64 assetTypeArraySize = sizeof(*file->assetTypes) * file->header.assetTypeCount;
      file->assetTypes = MemoryArenaPush(arena, assetTypeArraySize);
      Platform->ReadFromFile(file->assetTypes, &file->handle, file->header.assetTypesOffset, assetTypeArraySize);

      file->tagBase = assets->tagCount;

      assets->tagCount += header->tagCount;
      // NOTE: First asset is always null (reserved) asset.
      assets->assetCount += header->assetCount - 1;
    }

    Platform->GetAllFilesOfTypeEnd(&fileGroup);
  }

  // NOTE: allocate memory for all metadatas from asset pack files
  assets->tags = MemoryArenaPush(arena, sizeof(*assets->tags) * assets->tagCount);
  assets->assets = MemoryArenaPush(arena, sizeof(*assets->assets) * assets->assetCount);

  // NOTE: load tags
  for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
    struct asset_file *file = assets->files + fileIndex;

    if (Platform->HasFileError(&file->handle)) {
      continue;
    }

    struct hha_header *header = &file->header;
    struct hha_tag *dest = assets->tags + file->tagBase;
    u64 tagArraySize = header->tagCount * sizeof(*dest);
    Platform->ReadFromFile(dest, &file->handle, header->tagsOffset, tagArraySize);
  }

  // TODO: Fast loading 100+ asset pack files

  // NOTE: merge asset pack files
  u32 assetCount = 0;

  // first asset is always null
  struct asset *firstAsset = assets->assets + 0;
  ZeroMemory(firstAsset, sizeof(*firstAsset));
  assetCount++;

  for (u32 destAssetTypeId = 0; destAssetTypeId < ASSET_TYPE_COUNT; destAssetTypeId++) {
    struct asset_type *destType = assets->assetTypes + destAssetTypeId;
    destType->assetIndexFirst = assetCount;

    for (u32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++) {
      struct asset_file *file = assets->files + fileIndex;

      if (Platform->HasFileError(&file->handle)) {
        continue;
      }

      struct hha_header *header = &file->header;

      for (u32 srcIndex = 0; srcIndex < file->header.assetTypeCount; srcIndex++) {
        struct hha_asset_type *srcType = file->assetTypes + srcIndex;
        if (srcType->typeId == destAssetTypeId) {
          u32 assetCountForType = srcType->assetIndexOnePastLast - srcType->assetIndexFirst;

          // read assets metadata from file into temporary structure
          struct hha_asset *hhaAssets;
          u64 firstOffset = header->assetsOffset + (srcType->assetIndexFirst * sizeof(*hhaAssets));
          u64 assetArraySize = assetCountForType * sizeof(*hhaAssets);
          struct memory_temp temp = BeginTemporaryMemory(&transientState->transientArena);
          hhaAssets = MemoryArenaPush(temp.arena, assetArraySize);
          Platform->ReadFromFile(hhaAssets, &file->handle, firstOffset, assetArraySize);

          for (u32 assetIndex = 0; assetIndex < assetCountForType; assetIndex++) {
            assert(assetCount < assets->assetCount);
            struct hha_asset *hhaAsset = hhaAssets + assetIndex;
            struct asset *asset = assets->assets + assetCount;

            // TODO: validate hhaAsset

            asset->fileIndex = fileIndex;
            asset->hhaAsset = *hhaAsset;
            asset->hhaAsset.tagIndexFirst += file->tagBase;
            asset->hhaAsset.tagIndexOnePastLast += file->tagBase;

            assetCount++;
          }

          EndTemporaryMemory(&temp);
        }
      }
    }

    destType->assetIndexOnePastLast = assetCount;
  }

  assert(assetCount == assets->assetCount && "missing assets");

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

  struct asset *asset;
  enum asset_state finalState;
  struct task_with_memory *task;
};

internal void
LoadAssetWork(struct load_asset_work *work)
{
  struct asset *asset = work->asset;

  Platform->ReadFromFile(work->dest, work->handle, work->offset, work->size);

  if (Platform->HasFileError(work->handle)) {
    ZeroMemory(work->dest, work->size);
  }

  AtomicStore(&asset->state, work->finalState);
}

internal void
DoLoadAssetWork(struct platform_work_queue *queue, void *data)
{
  struct load_asset_work *work = data;
  LoadAssetWork(work);
  EndTaskWithMemory(work->task);
}

internal inline void
_BitmapLoad(struct game_assets *assets, struct bitmap_id id, b32 immediate)
{
  if (id.value == 0)
    return;

  struct asset *asset = assets->assets + id.value;

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&asset->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = 0;

    if (!immediate) {
      task = BeginTaskWithMemory(assets->transientState);
      if (!task) {
        // memory cannot obtained, revert back
        AtomicStore(&asset->state, ASSET_STATE_UNLOADED);
        return;
      }
    }

    // setup header
    struct hha_asset *info = &asset->hhaAsset;
    assert(info->dataOffset && "asset not setup properly");
    struct hha_bitmap *bitmapInfo = &info->bitmap;

    u32 width = bitmapInfo->width;
    u32 height = bitmapInfo->height;
    s32 stride = (s32)(width * BITMAP_BYTES_PER_PIXEL);

    struct asset_memory_size size = {};
    size.section = (u16)stride;
    size.data = height * size.section;
    size.total = size.data + sizeof(*asset->header);

    asset->header = AcquireAssetMemory(assets, size.total, id.value);
    void *memory = (asset->header + 1);

    // setup bitmap
    struct bitmap *bitmap = &asset->header->bitmap;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->stride = stride;
    bitmap->memory = memory;

    bitmap->widthOverHeight = (f32)bitmap->width / (f32)bitmap->height;
    bitmap->alignPercentage = v2(bitmapInfo->alignPercentage[0], bitmapInfo->alignPercentage[1]);

    // setup work
    struct load_asset_work work = {};
    work.handle = AssetFileHandleGet(assets, asset->fileIndex);
    work.dest = bitmap->memory;
    work.offset = info->dataOffset;
    work.size = size.data;

    work.task = task;
    work.asset = asset;
    work.finalState = ASSET_STATE_LOADED;

    if (immediate) {
      LoadAssetWork(&work);
    } else {
      struct load_asset_work *taskWork = MemoryArenaPush(&task->arena, sizeof(*taskWork));
      *taskWork = work;

      // queue the work
      struct platform_work_queue *queue = assets->transientState->lowPriorityQueue;
      Platform->WorkQueueAddEntry(queue, DoLoadAssetWork, taskWork);
    }
  }

  // else some other thread beat us to it
  else if (immediate) {

    // Wait for it to be queued than return.
    // Without this requesting immediate breaks when two thread call at the same time.
    volatile enum asset_state *state = &asset->state;
    while (*state == ASSET_STATE_QUEUED)
      ;
  }
}

inline void
BitmapLoad(struct game_assets *assets, struct bitmap_id id)
{
  _BitmapLoad(assets, id, 0);
}

inline void
BitmapLoadImmediate(struct game_assets *assets, struct bitmap_id id)
{
  _BitmapLoad(assets, id, 1);
}

inline void
BitmapPrefetch(struct game_assets *assets, struct bitmap_id id)
{
  BitmapLoad(assets, id);
}

inline struct hha_bitmap *
BitmapInfoGet(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->assetCount);
  struct hha_asset *info = &(assets->assets + id.value)->hhaAsset;
  return &info->bitmap;
}

inline void
AudioLoad(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return;

  struct asset *asset = assets->assets + id.value;

  enum asset_state expectedAssetState = ASSET_STATE_UNLOADED;
  if (AtomicCompareExchange(&asset->state, &expectedAssetState, ASSET_STATE_QUEUED)) {
    // asset now queued
    struct task_with_memory *task = BeginTaskWithMemory(assets->transientState);
    if (!task) {
      // memory cannot obtained, revert back
      AtomicStore(&asset->state, ASSET_STATE_UNLOADED);
      return;
    }

    // setup header
    struct hha_asset *info = &asset->hhaAsset;
    assert(info->dataOffset && "asset not setup properly");
    struct hha_audio *audioInfo = &info->audio;

    struct asset_memory_size size = {};
    size.section = audioInfo->channelCount * sizeof(s16);
    size.data = audioInfo->sampleCount * size.section;
    size.total = size.data + sizeof(*asset->header);

    asset->header = AcquireAssetMemory(assets, size.total, id.value);
    void *memory = (asset->header + 1);

    // setup audio
    struct audio *audio = &asset->header->audio;
    audio->channelCount = audioInfo->channelCount;
    audio->sampleCount = audioInfo->sampleCount;

    s16 *samples = memory;
    audio->samples[0] = samples;
    audio->samples[1] = audio->samples[0] + audio->sampleCount;

    // setup work
    struct load_asset_work *work = MemoryArenaPush(&task->arena, sizeof(*work));
    work->handle = AssetFileHandleGet(assets, asset->fileIndex);
    work->dest = audio->samples[0];
    work->offset = info->dataOffset;
    work->size = size.data;

    work->task = task;
    work->asset = asset;
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

inline struct audio_id
AudioGetNextInChain(struct game_assets *assets, struct audio_id id)
{
  struct audio_id result = {};

  struct hha_audio *audioInfo = AudioInfoGet(assets, id);
  switch (audioInfo->chain) {
  case HHA_AUDIO_CHAIN_NONE: {
    // nothing to do
  } break;

  case HHA_AUDIO_CHAIN_LOOP: {
    result = id;
  } break;

  case HHA_AUDIO_CHAIN_ADVANCE: {
    result.value = id.value + 1;
  } break;
  }

  return result;
}

inline struct hha_audio *
AudioInfoGet(struct game_assets *assets, struct audio_id id)
{
  if (id.value == 0)
    return 0;

  assert(id.value <= assets->assetCount);
  struct hha_asset *info = &(assets->assets + id.value)->hhaAsset;
  return &info->audio;
}

inline b32
IsAudioIdValid(struct audio_id id)
{
  return id.value != 0;
}

inline u32
BeginGeneration(struct game_assets *assets)
{
  BeginAssetLock(assets);

  assert(assets->inFlightGenerationCount < ARRAY_COUNT(assets->inFlightGenerations));
  u32 generationId = assets->nextGenerationId++;
  assets->inFlightGenerations[assets->inFlightGenerationCount++] = generationId;

  EndAssetLock(assets);

  return generationId;
}

inline void
EndGeneration(struct game_assets *assets, u32 generationId)
{
  BeginAssetLock(assets);

  for (u32 index = 0; index < assets->inFlightGenerationCount; index++) {
    if (assets->inFlightGenerations[index] == generationId) {
      assets->inFlightGenerations[index] = assets->inFlightGenerations[assets->inFlightGenerationCount - 1];
      assets->inFlightGenerationCount--;
      goto found;
    }
  }

  assert(0 && "no generation id found");

found:
  EndAssetLock(assets);
}
