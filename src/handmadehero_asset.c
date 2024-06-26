#include <handmadehero/assert.h>
#include <handmadehero/asset.h>
#include <handmadehero/atomic.h>
#include <handmadehero/handmadehero.h> // BeginTaskWithMemory, EndTaskWithMemory

inline struct bitmap *
AssetBitmapGet(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return 0;

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

internal struct bitmap_id
BitmapInfoAdd(struct game_assets *assets, char *filename, struct v2 alignPercentage)
{
  struct bitmap_id id = {assets->DEBUGUsedBitmapInfoCount};
  assets->DEBUGUsedBitmapInfoCount++;

  struct asset_bitmap_info *info = assets->bitmapInfos + id.value;
  info->filename = filename;
  info->alignPercentage = alignPercentage;

  return id;
}

internal void
BeginAsset(struct game_assets *assets, enum asset_type_id assetTypeId)
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
  assert(assets->DEBUGAssetType && "cannot finish something that is not started");

  struct asset_type *type = assets->DEBUGAssetType;
  struct asset *asset = assets->assets + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  asset->tagIndexFirst = 0;
  asset->tagIndexOnePastLast = 0;
  asset->slotId = BitmapInfoAdd(assets, filename, alignPercentage).value;
}

internal void
EndAsset(struct game_assets *assets)
{
  assert(assets->DEBUGAssetType && "cannot finish something that is not started");
  assets->DEBUGUsedAssetCount = assets->DEBUGAssetType->assetIndexOnePastLast;
  assets->DEBUGAssetType = 0;
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

  assets->audioCount = 1;
  assets->audios = MemoryArenaPush(arena, sizeof(*assets->audios) * assets->audioCount);

  assets->tagCount = 0;
  assets->tags = 0;

  assets->assetCount = assets->bitmapCount;
  assets->assets = MemoryArenaPush(arena, sizeof(*assets->assets) * assets->assetCount);

  assets->DEBUGUsedBitmapInfoCount = 1;
  assets->DEBUGUsedAssetCount = 1;

  BeginAsset(assets, ASSET_TYPE_SHADOW);
  AddBitmapAsset(assets, "test/test_hero_shadow.bmp", v2(0.5f, 0.156682029f));
  EndAsset(assets);

  BeginAsset(assets, ASSET_TYPE_TREE);
  AddBitmapAsset(assets, "test2/tree00.bmp", v2(0.493827164f, 0.295652181f));
  EndAsset(assets);

  BeginAsset(assets, ASSET_TYPE_SWORD);
  AddBitmapAsset(assets, "test2/rock03.bmp", v2(0.5f, 0.65625f));
  EndAsset(assets);

  BeginAsset(assets, ASSET_TYPE_GRASS);
  AddBitmapAsset(assets, "test2/grass00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/grass01.bmp", v2(0.5f, 0.5f));
  EndAsset(assets);

  BeginAsset(assets, ASSET_TYPE_GROUND);
  AddBitmapAsset(assets, "test2/ground00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground01.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground02.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/ground03.bmp", v2(0.5f, 0.5f));
  EndAsset(assets);

  BeginAsset(assets, ASSET_TYPE_TUFT);
  AddBitmapAsset(assets, "test2/tuft00.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/tuft01.bmp", v2(0.5f, 0.5f));
  AddBitmapAsset(assets, "test2/tuft02.bmp", v2(0.5f, 0.5f));
  EndAsset(assets);

  /* load hero bitmaps */
  struct bitmap_hero *bitmapHero = &assets->textureHero[BITMAP_HERO_FRONT];
  bitmapHero->head = LoadBmp(PlatformReadEntireFile, "test/test_hero_front_head.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->torso = LoadBmp(PlatformReadEntireFile, "test/test_hero_front_torso.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->cape = LoadBmp(PlatformReadEntireFile, "test/test_hero_front_cape.bmp", v2(0.5f, 0.156682029f));

  bitmapHero = &assets->textureHero[BITMAP_HERO_BACK];
  bitmapHero->head = LoadBmp(PlatformReadEntireFile, "test/test_hero_back_head.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->torso = LoadBmp(PlatformReadEntireFile, "test/test_hero_back_torso.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->cape = LoadBmp(PlatformReadEntireFile, "test/test_hero_back_cape.bmp", v2(0.5f, 0.156682029f));

  bitmapHero = &assets->textureHero[BITMAP_HERO_LEFT];
  bitmapHero->head = LoadBmp(PlatformReadEntireFile, "test/test_hero_left_head.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->torso = LoadBmp(PlatformReadEntireFile, "test/test_hero_left_torso.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->cape = LoadBmp(PlatformReadEntireFile, "test/test_hero_left_cape.bmp", v2(0.5f, 0.156682029f));

  bitmapHero = &assets->textureHero[BITMAP_HERO_RIGHT];
  bitmapHero->head = LoadBmp(PlatformReadEntireFile, "test/test_hero_right_head.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->torso = LoadBmp(PlatformReadEntireFile, "test/test_hero_right_torso.bmp", v2(0.5f, 0.156682029f));
  bitmapHero->cape = LoadBmp(PlatformReadEntireFile, "test/test_hero_right_cape.bmp", v2(0.5f, 0.156682029f));

  return assets;
}

struct bitmap_id
AssetBitmapGetFirstId(struct game_assets *assets, enum asset_type_id typeId)
{
  struct bitmap_id result = {};
  if (typeId == ASSET_TYPE_NONE)
    return result;

  struct asset_type *type = assets->assetTypes + typeId;
  if (type->assetIndexFirst == type->assetIndexOnePastLast)
    return result;

  struct asset *asset = assets->assets + type->assetIndexFirst;
  result.value = asset->slotId;

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

  struct asset_bitmap_info *info = work->assets->bitmapInfos + work->bitmapId.value;
  *work->bitmap = LoadBmp(work->assets->PlatformReadEntireFile, info->filename, info->alignPercentage);

  // TODO(e2dk4r): fence!
  struct asset_slot *slot = work->assets->bitmaps + work->bitmapId.value;
  slot->bitmap = work->bitmap;
  AtomicStore(&slot->state, work->finalState);

  EndTaskWithMemory(work->task);
}

inline void
AssetBitmapLoad(struct game_assets *assets, struct bitmap_id id)
{
  if (id.value == 0)
    return;

  struct asset_slot *slot = assets->bitmaps + id.value;
  struct asset_bitmap_info *info = assets->bitmapInfos + id.value;
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

void
AssetAudioLoad(struct game_assets *assets, struct audio_id id)
{
  // TODO(e2dk4r): implement loading audio
}
