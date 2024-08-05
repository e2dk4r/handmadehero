#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#pragma GCC diagnostic push

// caused by: stb_ds.h
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <handmadehero/assert.h>
#include <handmadehero/fileformats.h>
#include <handmadehero/types.h>

// #define STB_DS_IMPLEMENTATION
// #include "stb_ds.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#define PRIu32 "u"
#define PRIs32 "d"

// TRUETYPE_BACKEND_XXX
#if TRUETYPE_BACKEND_FREETYPE
#undef internal
#include <freetype/freetype.h>
#define internal static

#elif TRUETYPE_BACKEND_STBTT
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

#undef snprintf
#define snprintf stbsp_snprintf

#pragma GCC diagnostic pop

/*****************************************************************
 * MEMORY
 *****************************************************************/

struct memory_block {
  u64 size;
};

// allocates and fills memory with zeros
internal void *
AllocateMemory(u64 size)
{
  u64 total = size + sizeof(struct memory_block);
  struct memory_block *memoryBlock = mmap(0, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memoryBlock == MAP_FAILED)
    return 0;

  memoryBlock->size = total;
  void *memory = memoryBlock + 1;

  return memory;
}

internal void
DeallocateMemory(void *memory)
{
  if (!memory)
    return;

  struct memory_block *memoryBlock = memory - sizeof(*memoryBlock);

  // TODO: unmap failed?
  munmap(memoryBlock, memoryBlock->size);
  memory = 0;
}

/*****************************************************************
 * MATH
 *****************************************************************/

comptime f32 TAU32 = 6.28318530717958647692f;

internal inline f32
Square(f32 value)
{
  return value * value;
}

internal inline f32
SquareRoot(f32 value)
{
  return __builtin_sqrtf(value);
}

internal inline s32
FindLeastSignificantBitSet(s32 value)
{
  s32 ffsResult = __builtin_ffs(value);
  assert(ffsResult != 0);
  return ffsResult - 1;
}

/*****************************************************************
 * LOGGING
 *****************************************************************/

enum log_color {
  LOG_COLOR_ERROR,
  LOG_COLOR_FATAL,
  LOG_COLOR_WARN,
};

internal b32
IsStdoutTerminal(void)
{
  s32 isattyResult = isatty(STDOUT_FILENO);
  assert(isattyResult >= 0);
  return isattyResult != 0;
}

internal void
BeginLogColor(enum log_color color)
{
  if (!IsStdoutTerminal())
    return;

#define ANSI_COLOR_START_BACKGROUND "\033[48;2;"
#define ANSI_COLOR_START_TEXT "\033[38;2;"
#define ANSI_COLOR_END "m"
#define ANSI_COLOR_DELIMITER ";"

  // COLOR_RED_500 #ef4444
  char ansiBackgroundColorRed[] =
      ANSI_COLOR_START_BACKGROUND "239" ANSI_COLOR_DELIMITER "68" ANSI_COLOR_DELIMITER "68" ANSI_COLOR_END;

  // COLOR_YELLOW_500 #eab308
  char ansiBackgroundColorYellow[] =
      ANSI_COLOR_START_BACKGROUND "234" ANSI_COLOR_DELIMITER "179" ANSI_COLOR_DELIMITER "8" ANSI_COLOR_END;

  // COLOR_GRAY_900 #111827
  char ansiTextColor[] = ANSI_COLOR_START_TEXT "17" ANSI_COLOR_DELIMITER "24" ANSI_COLOR_DELIMITER "39" ANSI_COLOR_END;

  switch (color) {
  case LOG_COLOR_ERROR:
  case LOG_COLOR_FATAL: {

    write(STDOUT_FILENO, ansiBackgroundColorRed, ARRAY_COUNT(ansiBackgroundColorRed) - 1);
    write(STDOUT_FILENO, ansiTextColor, ARRAY_COUNT(ansiTextColor) - 1);
  } break;

  case LOG_COLOR_WARN: {
    // COLOR_GRAY_900 #111827
    char ansiTextColor[] =
        ANSI_COLOR_START_TEXT "17" ANSI_COLOR_DELIMITER "24" ANSI_COLOR_DELIMITER "39" ANSI_COLOR_END;

    write(STDOUT_FILENO, ansiBackgroundColorYellow, ARRAY_COUNT(ansiBackgroundColorYellow) - 1);
    write(STDOUT_FILENO, ansiTextColor, ARRAY_COUNT(ansiTextColor) - 1);
  } break;
  }

#undef ANSI_BACKGROUND_COLOR_START
#undef ANSI_TEXT_COLOR_START
#undef ANSI_COLOR_END
#undef ANSI_COLOR_DELIMITER
}

internal void
EndLogColor(void)
{
  if (!IsStdoutTerminal())
    return;

  char ansiColorDefault[] = "\033[0m";
  write(STDOUT_FILENO, ansiColorDefault, ARRAY_COUNT(ansiColorDefault) - 1);
}

internal void
info(char *buf, u64 count)
{
  write(STDOUT_FILENO, buf, count);
}

internal void
warn(char *buf, u64 count)
{
  b32 hasNewLineAtEnd = buf[count] == '\n';
  if (hasNewLineAtEnd)
    count--;

  BeginLogColor(LOG_COLOR_WARN);
  write(STDOUT_FILENO, "warning: ", 9);
  write(STDOUT_FILENO, buf, count);
  EndLogColor();

  if (hasNewLineAtEnd)
    write(STDOUT_FILENO, "\n", 1);
}

internal void
error(char *buf, u64 count)
{
  b32 hasNewLineAtEnd = buf[count - 1] == '\n';
  if (hasNewLineAtEnd)
    count--;

  BeginLogColor(LOG_COLOR_ERROR);
  write(STDOUT_FILENO, "error: ", 7);
  write(STDERR_FILENO, buf, count);
  EndLogColor();

  if (hasNewLineAtEnd)
    write(STDOUT_FILENO, "\n", 1);
}

internal void
fatal(char *buf, u64 count)
{
  b32 hasNewLineAtEnd = buf[count - 1] == '\n';
  if (hasNewLineAtEnd)
    count--;

  BeginLogColor(LOG_COLOR_FATAL);
  write(STDERR_FILENO, buf, count);
  EndLogColor();

  if (hasNewLineAtEnd)
    write(STDOUT_FILENO, "\n", 1);
}

/*****************************************************************
 * STRUCTURES
 *****************************************************************/

enum hh_asset_builder_error {
  HH_ASSET_BUILDER_ERROR_NONE = 0,
  HH_ASSET_BUILDER_ERROR_ARGUMENTS,
  HH_ASSET_BUILDER_ERROR_IO_OPEN,
  HH_ASSET_BUILDER_ERROR_IO_IS_NOT_FILE,
  HH_ASSET_BUILDER_ERROR_IO_STAT,
  HH_ASSET_BUILDER_ERROR_MALLOC,
  HH_ASSET_BUILDER_ERROR_IO_READ,
  HH_ASSET_BUILDER_ERROR_IO_SEEK,
  HH_ASSET_BUILDER_ERROR_WAV_INVALID_MAGIC,
  HH_ASSET_BUILDER_ERROR_WAV_MALFORMED,
  HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_PCM,
  HH_ASSET_BUILDER_ERROR_WAV_SAMPLE_RATE_IS_NOT_48000,
  HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_S16LE,
  HH_ASSET_BUILDER_ERROR_WAV_NUM_CHANNELS_IS_BIGGER_THAN_2,
  HH_ASSET_BUILDER_ERROR_BMP_INVALID_MAGIC,
  HH_ASSET_BUILDER_ERROR_BMP_MALFORMED,
  HH_ASSET_BUILDER_ERROR_BMP_IS_NOT_ENCODED_PROPERLY,
  HH_ASSET_BUILDER_ERROR_TTF_MALFORMED,
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
  enum hha_audio_chain chain;
};

struct font_info {
  char *fontPath;
  u32 codepointCount;

  struct bitmap_id *codepoints; // codepoints[codepointCount]
  f32 *horizontalAdvanceTable;  // horizontalAdvanceTable[codepointCount][codepointCount]

  // to be used with font_glyph_info
  struct loaded_font *loadedFont;
  u32 writtenCodepointCount;
};

struct font_glyph_info {
  struct font_id fontId;
  u32 codepoint;
};

enum asset_metadata_type {
  ASSET_METADATA_TYPE_AUDIO,
  ASSET_METADATA_TYPE_BITMAP,
  ASSET_METADATA_TYPE_FONT,
  ASSET_METADATA_TYPE_FONT_GLYPH,
};

struct asset_metadata {
  enum asset_metadata_type type;

  u32 tagIndexFirst;
  u32 tagIndexOnePastLast;

  union {
    struct bitmap_info bitmapInfo;
    struct audio_info audioInfo;
    struct font_info fontInfo;
    struct font_glyph_info fontGlyphInfo;
  };
};

#define TAG_COUNT 0x1000
#define ASSET_COUNT 0x1000

struct asset_context {
  u32 tagCount;
  struct hha_tag tags[TAG_COUNT];

  u32 assetTypeCount;
  struct hha_asset_type assetTypes[ASSET_TYPE_COUNT];

  u32 assetCount;
  struct asset_metadata assetMetadatas[ASSET_COUNT];
  struct hha_asset assets[ASSET_COUNT];

  struct hha_asset_type *currentAssetType;
  struct asset_metadata *currentAsset;
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
BeginAssetType(struct asset_context *context, enum asset_type_id typeId)
{
  assert(context->currentAssetType == 0 && "another already in progress, one at a time");
  context->currentAssetType = context->assetTypes + typeId;

  struct hha_asset_type *type = context->currentAssetType;
  type->typeId = typeId;
  type->assetIndexFirst = context->assetCount;
  type->assetIndexOnePastLast = type->assetIndexFirst;
}

struct added_asset {
  u32 id;
  struct asset_metadata *metadata;
};

internal struct added_asset
AddAsset(struct asset_context *context)
{
  assert(context->currentAssetType && "you must call BeginAssetType()");
  assert(context->currentAssetType->assetIndexOnePastLast < ARRAY_COUNT(context->assets) && "asset count exceeded");

  struct hha_asset_type *type = context->currentAssetType;
  context->currentAsset = context->assetMetadatas + type->assetIndexOnePastLast;
  type->assetIndexOnePastLast++;

  struct asset_metadata *metadata = context->currentAsset;
  metadata->tagIndexFirst = context->tagCount;
  metadata->tagIndexOnePastLast = metadata->tagIndexFirst;

  u32 id = context->assetCount;
  context->assetCount++;

  return (struct added_asset){
      .id = id,
      .metadata = metadata,
  };
}

internal struct bitmap_id
AddBitmapAsset(struct asset_context *context, char *filename, f32 alignPercentageX, f32 alignPercentageY)
{
  struct added_asset asset = AddAsset(context);
  struct asset_metadata *metadata = asset.metadata;
  metadata->type = ASSET_METADATA_TYPE_BITMAP;

  struct bitmap_id id = {asset.id};
  struct bitmap_info *info = &metadata->bitmapInfo;
  info->filename = filename;
  info->alignPercentageX = alignPercentageX;
  info->alignPercentageY = alignPercentageY;

  return id;
}

internal struct audio_id
AddAudioAssetTrimmed(struct asset_context *context, char *filename, u32 sampleIndex, u32 sampleCount)
{
  struct added_asset asset = AddAsset(context);
  struct asset_metadata *metadata = asset.metadata;
  metadata->type = ASSET_METADATA_TYPE_AUDIO;

  struct audio_id id = {asset.id};
  struct audio_info *info = &metadata->audioInfo;
  info->filename = filename;
  info->sampleIndex = sampleIndex;
  info->sampleCount = sampleCount;
  info->chain = HHA_AUDIO_CHAIN_NONE;

  return id;
}

internal struct audio_id
AddAudioAsset(struct asset_context *context, char *filename)
{
  return AddAudioAssetTrimmed(context, filename, 0, 0);
}

// fontInfo.codepoints is allocated on heap, after used call DeallocateMemory() on it.
// fontInfo.horizontalAdvanceTable is allocated on heap, after used call DeallocateMemory() on it.
internal struct font_id
AddFontAsset(struct asset_context *context, char *fontPath, u32 codepointCount)
{
  struct added_asset asset = AddAsset(context);
  struct asset_metadata *metadata = asset.metadata;
  metadata->type = ASSET_METADATA_TYPE_FONT;

  struct font_id id = {asset.id};
  struct font_info *fontInfo = &metadata->fontInfo;
  fontInfo->fontPath = fontPath;
  fontInfo->codepointCount = codepointCount;

  u32 codepointsSize = fontInfo->codepointCount * sizeof(struct bitmap_id);
  fontInfo->codepoints = AllocateMemory(codepointsSize);

  u32 horizontalAdvanceTableSize = fontInfo->codepointCount * fontInfo->codepointCount * sizeof(f32);
  fontInfo->horizontalAdvanceTable = AllocateMemory(horizontalAdvanceTableSize);

  return id;
}

internal struct bitmap_id
AddFontGlyphAsset(struct asset_context *context, struct font_id fontId, u32 codepoint)
{
  struct added_asset asset = AddAsset(context);
  struct asset_metadata *metadata = asset.metadata;
  metadata->type = ASSET_METADATA_TYPE_FONT_GLYPH;

  struct bitmap_id id = {asset.id};
  struct font_glyph_info *fontGlyphInfo = &metadata->fontGlyphInfo;
  fontGlyphInfo->fontId = fontId;
  fontGlyphInfo->codepoint = codepoint;

  return id;
}

internal void
AddAssetTag(struct asset_context *context, enum asset_tag_id tagId, f32 value)
{
  assert(context->currentAsset && "you must call one of Add...Asset()");
  assert(context->currentAsset->tagIndexOnePastLast < ARRAY_COUNT(context->tags) && "tag count exceeded");

  struct asset_metadata *asset = context->currentAsset;
  struct hha_tag *tag = context->tags + context->tagCount;
  asset->tagIndexOnePastLast++;

  tag->id = tagId;
  tag->value = value;

  context->tagCount++;
}

internal void
EndAssetType(struct asset_context *context)
{
  assert(context->currentAssetType && "cannot finish something that is not started");
  context->assetCount = context->currentAssetType->assetIndexOnePastLast;
  context->currentAssetType = 0;
  context->currentAsset = 0;
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
 * LOADING FILES
 *****************************************************************/

struct read_file_result {
  enum hh_asset_builder_error error;
  u64 size;
  void *data;
};

internal struct read_file_result
ReadEntireFile(char *path)
{
  struct read_file_result result = {};
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    result.error = HH_ASSET_BUILDER_ERROR_IO_OPEN;
    return result;
  }

  struct stat stat;
  if (fstat(fd, &stat)) {
    result.error = HH_ASSET_BUILDER_ERROR_IO_STAT;
    return result;
  }

  if (!S_ISREG(stat.st_mode)) {
    result.error = HH_ASSET_BUILDER_ERROR_IO_IS_NOT_FILE;
    return result;
  }

  result.size = (u64)stat.st_size;
  result.data = AllocateMemory(result.size);
  if (!result.data) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    return result;
  }

  ssize_t bytesRead = read(fd, result.data, (size_t)result.size);
  if (bytesRead <= 0) {
    result.error = HH_ASSET_BUILDER_ERROR_IO_READ;
    DeallocateMemory(result.data);
    result.data = 0;
    return result;
  }

  close(fd);

  return result;
}

/*****************************************************************
 * LOADING WAV FILES
 *****************************************************************/

struct loaded_audio {
  void *_filememory;

  u32 channelCount;
  u32 sampleCount;
  s16 *samples[2];
};

#pragma pack(push, 1)

struct wave_header {
  u32 riffId;
  u32 fileSize;
  u32 waveId;
};

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
};

struct wave_fmt {
  u16 audioFormat;
  u16 numChannels;
  u32 sampleRate;
  u32 byteRate;
  u16 blockAlign;
  u16 bitsPerSample;
};

#define WAVE_FORMAT_PCM 0x0001

struct wave_chunk_iterator {
  struct wave_chunk *chunk;
  void *eof;
};

#pragma pack(pop)

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

#define AUDIO_INFO_SAMPLE_COUNT_ALL 0

struct load_wav_result {
  enum hh_asset_builder_error error;
  struct loaded_audio loadedAudio;
};

internal void
FreeWav(struct loaded_audio *audio)
{
  DeallocateMemory(audio->_filememory);
  audio->_filememory = 0;
}

internal struct load_wav_result
LoadWav(char *filename, u32 sectionSampleIndex, u32 sectionSampleCount)
{
  struct load_wav_result result = {};

  struct read_file_result readResult = ReadEntireFile(filename);
  if (readResult.error != HH_ASSET_BUILDER_ERROR_NONE) {
    result.error = readResult.error;
    return result;
  }

  struct loaded_audio *loadedAudio = &result.loadedAudio;
  loadedAudio->_filememory = readResult.data;

  struct wave_header *header = readResult.data;
  if (!((header->riffId == WAVE_CHUNKID_RIFF) && (header->waveId == WAVE_CHUNKID_WAVE))) {
    result.error = HH_ASSET_BUILDER_ERROR_WAV_INVALID_MAGIC;
    goto onError;
  }

  struct wave_fmt *fmt = 0;
  s16 *sampleData = 0;
  u32 sampleDataSize = 0;
  for (struct wave_chunk_iterator iterator = WaveChunkParse(header); IsWaveChunkValid(iterator);
       iterator = WaveChunkNext(iterator)) {
    if (iterator.chunk->id != WAVE_CHUNKID_FMT)
      continue;

    fmt = WaveChunkData(iterator.chunk);
    if (fmt->audioFormat != WAVE_FORMAT_PCM) {
      result.error = HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_PCM;
      goto onError;
    }

    if (fmt->sampleRate != 48000) {
      result.error = HH_ASSET_BUILDER_ERROR_WAV_SAMPLE_RATE_IS_NOT_48000;
      goto onError;
    }

    if ((fmt->bitsPerSample != 16) || (fmt->blockAlign != sizeof(u16) * fmt->numChannels)) {
      result.error = HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_S16LE;
      goto onError;
    }

    if (fmt->numChannels > 2) {
      result.error = HH_ASSET_BUILDER_ERROR_WAV_NUM_CHANNELS_IS_BIGGER_THAN_2;
      goto onError;
    }

    iterator = WaveChunkNext(iterator);
    if (iterator.chunk->id != WAVE_CHUNKID_DATA) {
      result.error = HH_ASSET_BUILDER_ERROR_WAV_MALFORMED;
      goto onError;
    }

    sampleData = (s16 *)WaveChunkData(iterator.chunk);
    sampleDataSize = iterator.chunk->size;

    break;
  }

  assert(fmt && sampleData);

  loadedAudio->channelCount = fmt->numChannels;
  u32 sampleCount = sampleDataSize / (u32)(fmt->numChannels * sizeof(u16));
  switch (fmt->numChannels) {
  case 1:
    loadedAudio->samples[0] = sampleData;
    loadedAudio->samples[1] = 0;
    break;

  case 2:
    loadedAudio->samples[0] = sampleData;
    loadedAudio->samples[1] = sampleData + sampleCount;

    for (u32 sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
      s16 source = sampleData[sampleIndex * 2];
      sampleData[sampleIndex * 2] = sampleData[sampleIndex];
      sampleData[sampleIndex] = source;
    }

    // TODO(e2dk4r): load right channels
    break;

  default:
    assert(0 && "Unsupported number of channels");
  }

  b32 isBufferEnd = 0;
  if (sectionSampleCount != AUDIO_INFO_SAMPLE_COUNT_ALL) {
    assert(sectionSampleIndex + sectionSampleCount <= sampleCount);
    isBufferEnd = sectionSampleIndex + sectionSampleCount == sampleCount;
    sampleCount = sectionSampleCount;
    for (u32 channelIndex = 0; channelIndex < loadedAudio->channelCount; channelIndex++) {
      loadedAudio->samples[channelIndex] += sectionSampleIndex;
    }
  }

  if (isBufferEnd) {
    for (u32 channelIndex = 0; channelIndex < loadedAudio->channelCount; channelIndex++) {
      for (u32 sampleIndex = sampleCount; sampleIndex < sampleCount + 8; sampleIndex++) {
        loadedAudio->samples[channelIndex][sampleIndex] = 0;
      }
    }
  }

  loadedAudio->sampleCount = sampleCount;

  return result;

onError:
  FreeWav(loadedAudio);
  return result;
}

/*****************************************************************
 * LOADING BMP FILES
 *****************************************************************/

#pragma pack(push, 1)

#define BITMAP_COMPRESSION_RGB 0
#define BITMAP_COMPRESSION_BITFIELDS 3
struct bitmap_header {
  u16 fileType;
  u32 fileSize;
  u16 reserved1;
  u16 reserved2;
  u32 bitmapOffset;
  u32 size;
  s32 width;
  s32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 imageSize;
  u32 horzResolution;
  u32 vertResolution;
  u32 colorsPalette;
  u32 colorsImportant;
};

#define BITMAP_BYTES_PER_PIXEL 4
struct loaded_bitmap {
  void *_filememory;

  u32 width;
  u32 height;
  u32 stride;
  void *memory;
};

struct bitmap_header_compressed {
  struct bitmap_header header;
  u32 redMask;
  u32 greenMask;
  u32 blueMask;
};

#pragma pack(pop)

struct load_bmp_result {
  enum hh_asset_builder_error error;
  struct loaded_bitmap loadedBitmap;
};

void
FreeBmp(struct loaded_bitmap *bitmap)
{
  DeallocateMemory(bitmap->_filememory);
}

internal struct load_bmp_result
LoadBmp(char *filename)
{
  struct load_bmp_result result = {};

  struct read_file_result readResult = ReadEntireFile(filename);
  if (readResult.size == 0) {
    result.error = readResult.error;
    return result;
  }

  struct loaded_bitmap *loadedBitmap = &result.loadedBitmap;
  loadedBitmap->_filememory = readResult.data;

  struct bitmap_header *header = readResult.data;
  u16 bmpMagic = 'B' << 0x00 | 'M' << 0x08;
  if (header->fileType != bmpMagic) {
    result.error = HH_ASSET_BUILDER_ERROR_BMP_INVALID_MAGIC;
    goto onError;
  }

  u8 *pixels = readResult.data + header->bitmapOffset;

  if (header->compression == BITMAP_COMPRESSION_BITFIELDS) {
    struct bitmap_header_compressed *cHeader = (struct bitmap_header_compressed *)header;

    s32 redShift = FindLeastSignificantBitSet((s32)cHeader->redMask);
    s32 greenShift = FindLeastSignificantBitSet((s32)cHeader->greenMask);
    s32 blueShift = FindLeastSignificantBitSet((s32)cHeader->blueMask);
    if (redShift == greenShift) {
      result.error = HH_ASSET_BUILDER_ERROR_BMP_MALFORMED;
      goto onError;
    }

    u32 alphaMask = ~(cHeader->redMask | cHeader->greenMask | cHeader->blueMask);
    s32 alphaShift = FindLeastSignificantBitSet((s32)alphaMask);

    u32 *srcDest = (u32 *)pixels;
    for (s32 y = 0; y < header->height; y++) {
      for (s32 x = 0; x < header->width; x++) {

        u32 value = *srcDest;

        // extract pixel from file
        f32 texelR = (f32)((value >> redShift) & 0xff);
        f32 texelG = (f32)((value >> greenShift) & 0xff);
        f32 texelB = (f32)((value >> blueShift) & 0xff);
        f32 texelA = (f32)((value >> alphaShift) & 0xff);

        // texel = sRGB255toLinear1(texel);
        f32 inv255 = 1.0f / 255.0f;
        texelR = Square(inv255 * texelR);
        texelG = Square(inv255 * texelG);
        texelB = Square(inv255 * texelB);
        texelA = inv255 * texelA;

        /*
         * Store channels values pre-multiplied with alpha.
         */
        // v3_mul_ref(&texel.rgb, texel.a);
        texelR *= texelA;
        texelG *= texelA;
        texelB *= texelA;

        // texel = Linear1tosRGB255(texel);
        texelR = 255.0f * SquareRoot(texelR);
        texelG = 255.0f * SquareRoot(texelG);
        texelB = 255.0f * SquareRoot(texelB);
        texelA = 255.0f * texelA;

        *srcDest = (u32)(texelA + 0.5f) << 0x18 | (u32)(texelR + 0.5f) << 0x10 | (u32)(texelG + 0.5f) << 0x08 |
                   (u32)(texelB + 0.5f) << 0x00;

        srcDest++;
      }
    }
  }

  loadedBitmap->width = (u32)header->width;
  if (header->width < 0)
    loadedBitmap->width = (u32)-header->width;

  loadedBitmap->height = (u32)header->height;
  if (header->height < 0)
    loadedBitmap->height = (u32)-header->height;

  assert(loadedBitmap->width != 0);
  assert(loadedBitmap->height != 0);

  loadedBitmap->stride = loadedBitmap->width * BITMAP_BYTES_PER_PIXEL;
  loadedBitmap->memory = pixels;

  if (loadedBitmap->stride != (loadedBitmap->width * sizeof(u32))) {
    result.error = HH_ASSET_BUILDER_ERROR_BMP_IS_NOT_ENCODED_PROPERLY;
    goto onError;
  }

  return result;

onError:
  FreeBmp(loadedBitmap);
  return result;
}

/*****************************************************************
 * LOADING TTF FILES
 *****************************************************************/

struct loaded_font {
  void *_filememory;
  f32 ascent;
  f32 descent;
  f32 lineGap;

#if TRUETYPE_BACKEND_FREETYPE
  FT_Library library;
  FT_Face face;
#elif TRUETYPE_BACKEND_STBTT
  stbtt_fontinfo font;
  f32 scale;
  s32 baseline;
#endif
};

struct load_font_result {
  enum hh_asset_builder_error error;
  struct loaded_font *loadedFont;
};

struct load_font_glyph_result {
  enum hh_asset_builder_error error;
  struct loaded_bitmap loadedBitmap;
  f32 alignPercentageX;
  f32 alignPercentageY;
};

// loadedFont is allocated on heap, after used call DeallocateMemory() on it.
// loadedFont._filememory is allocated on heap, after used call DeallocateMemory() on it.
internal struct load_font_result
LoadFont(char *fontPath, u32 codepointCount, f32 *horizontalAdvanceTable);

// loadedBitmap.memory is allocated on heap, after used call DeallocateMemory() on it.
internal struct load_font_glyph_result
LoadFontGlyph(struct loaded_font *loadedFont, u32 codepoint);

#if TRUETYPE_BACKEND_FREETYPE

internal struct load_font_result
LoadFont(char *fontPath, u32 codepointCount, f32 *horizontalAdvanceTable)
{
  struct load_font_result result = {};

  struct read_file_result ttfFile = ReadEntireFile(fontPath);
  if (ttfFile.error != HH_ASSET_BUILDER_ERROR_NONE) {
    result.error = ttfFile.error;
    return result;
  }

  struct loaded_font *loadedFont = AllocateMemory(sizeof(*loadedFont));
  if (!loadedFont) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    return result;
  }
  loadedFont->_filememory = ttfFile.data;

  FT_Error error = FT_Init_FreeType(&loadedFont->library);
  if (error) {
    result.error = HH_ASSET_BUILDER_ERROR_TTF_MALFORMED;
    goto cleanupFile;
  }

  FT_Library library = loadedFont->library;
  error = FT_New_Memory_Face(library, ttfFile.data, (FT_Long)ttfFile.size, 0, &loadedFont->face);
  if (error) {
    result.error = HH_ASSET_BUILDER_ERROR_TTF_MALFORMED;
    goto cleanupLibrary;
  }

  FT_Face face = loadedFont->face;
  error = FT_Set_Pixel_Sizes(face, 0, 128);
  if (error) {
    result.error = HH_ASSET_BUILDER_ERROR_TTF_MALFORMED;
    goto cleanupFace;
  }

  FT_Size_Metrics *metrics = &face->size->metrics;
  assert(FT_IS_SCALABLE(face));
  loadedFont->ascent = (f32)(metrics->ascender >> 6);
  loadedFont->descent = (f32)(-metrics->descender >> 6);
  loadedFont->lineGap = (f32)((metrics->height - (metrics->ascender - metrics->descender)) >> 6);

  // started from 1 because 0 is used for empty,
  // for this to work horizontalAdvanceTable must be initialized to zero
  for (u32 codepointIndex = 1; codepointIndex < codepointCount; codepointIndex++) {
    /* retrieve glyph index from character code */
    FT_UInt glyphIndex = FT_Get_Char_Index(face, codepointIndex);

    /* load glyph image into the slot (erase previous one) */
    error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
    if (error)
      continue; /* ignore errors */

    f32 horizontalAdvance = (f32)(face->glyph->advance.x >> 6);

    for (u32 otherCodepointIndex = 0; otherCodepointIndex < codepointCount; otherCodepointIndex++) {
      u32 offset = (codepointIndex * codepointCount) + otherCodepointIndex;
      *(horizontalAdvanceTable + offset) = horizontalAdvance;
    }
  }

  b32 hasKerning = FT_HAS_KERNING(face);
  if (hasKerning) {
    for (u32 codepointIndex = 0; codepointIndex < codepointCount; codepointIndex++) {
      for (u32 otherCodepointIndex = 0; otherCodepointIndex < codepointCount; otherCodepointIndex++) {
        FT_Vector delta;
        FT_Get_Kerning(face, codepointIndex, otherCodepointIndex, FT_KERNING_DEFAULT, &delta);
        // f32 kerningAdvance = delta.x;
        if (delta.x == 0)
          continue;

        f32 kerningAdvanceScaled = (f32)(FT_MulFix(delta.x, metrics->y_scale) >> 6);
        u32 offset = (codepointIndex * codepointCount) + otherCodepointIndex;
        *(horizontalAdvanceTable + offset) += kerningAdvanceScaled;
      }
    }
  }

  result.loadedFont = loadedFont;

  return result;

  // cleanup
cleanupFace:
  FT_Done_Face(loadedFont->face);
cleanupLibrary:
  FT_Done_FreeType(loadedFont->library);
cleanupFile:
  DeallocateMemory(loadedFont->_filememory);

  return result;
}

internal struct load_font_glyph_result
LoadFontGlyph(struct loaded_font *loadedFont, u32 codepoint)
{
  struct load_font_glyph_result result = {};

  FT_Face face = loadedFont->face;
  FT_Error error = FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
  if (error) {
    result.error = HH_ASSET_BUILDER_ERROR_TTF_MALFORMED;
    return result;
  }

  assert(FT_HAS_HORIZONTAL(face));
  FT_Glyph_Metrics *glyphMetrics = &face->glyph->metrics;
  result.alignPercentageX = (1.0f - (f32)glyphMetrics->horiBearingX) / (f32)(glyphMetrics->width);
  result.alignPercentageY = (f32)(glyphMetrics->height - glyphMetrics->horiBearingY) / (f32)(glyphMetrics->height);

#if 0
  // TODO: fix aligning bitmap
  // result.alignPercentageX = (f32)face->glyph->bitmap_left / (f32)face->glyph->bitmap.width;
  // result.alignPercentageY = (1.0f - (f32)face->glyph->bitmap_top) / (f32)face->glyph->bitmap.rows;
#endif

  FT_Bitmap *bitmap = &face->glyph->bitmap;

  // transform
  u32 padding = 2;
  struct loaded_bitmap loadedBitmap = {};
  loadedBitmap.width = bitmap->width + padding;
  loadedBitmap.height = bitmap->rows + padding;
  loadedBitmap.stride = loadedBitmap.width * sizeof(u32);
  loadedBitmap.memory = AllocateMemory(loadedBitmap.height * loadedBitmap.stride);
  if (!loadedBitmap.memory) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    return result;
  }

  u8 *srcRow = bitmap->buffer + (((s32)bitmap->rows - 1) * bitmap->pitch); // start at bottom left
  s32 srcStride = -bitmap->pitch;                                          // go up
  u8 *destRow = loadedBitmap.memory;
  u32 destStride = loadedBitmap.stride;
  for (u32 y = padding; y < loadedBitmap.height; y++) {
    u8 *src = srcRow;
    u32 *dest = (u32 *)destRow;
    for (u32 x = padding; x < loadedBitmap.width; x++) {
      u32 alpha = (u32)*src++;
      u32 color = (alpha << 0x18) | (alpha << 0x10) | (alpha << 0x08) | (alpha << 0x00);
      *dest++ = color;
    }

    srcRow += srcStride;
    destRow += destStride;
  }

  result.loadedBitmap = loadedBitmap;

  return result;
}

#elif TRUETYPE_BACKEND_STBTT

internal struct load_font_result
LoadFont(char *fontPath, u32 codepointCount, f32 *horizontalAdvanceTable)
{
  struct load_font_result result = {};
  struct read_file_result ttfFile = ReadEntireFile(fontPath);
  if (ttfFile.error != HH_ASSET_BUILDER_ERROR_NONE) {
    result.error = ttfFile.error;
    return result;
  }

  struct loaded_font *loadedFont = AllocateMemory(sizeof(*loadedFont));
  if (!loadedFont) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    return result;
  }
  loadedFont->_filememory = ttfFile.data;

  stbtt_fontinfo *font = &loadedFont->font;
  int ok = stbtt_InitFont(font, ttfFile.data, stbtt_GetFontOffsetForIndex(ttfFile.data, 0));
  if (!ok) {
    result.error = HH_ASSET_BUILDER_ERROR_TTF_MALFORMED;
    DeallocateMemory(loadedFont->_filememory);
    DeallocateMemory(loadedFont);
    return result;
  }

  loadedFont->scale = stbtt_ScaleForPixelHeight(font, 128.0f);
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
  loadedFont->baseline = (s32)((f32)descent * loadedFont->scale);
  loadedFont->ascent = (f32)ascent * loadedFont->scale;
  loadedFont->descent = (f32)-descent * loadedFont->scale;
  loadedFont->lineGap = (f32)lineGap * loadedFont->scale;

  // started from 1 because 0 is used for empty,
  // for this to work horizontalAdvanceTable must be initialized to zero
  for (u32 codepointIndex = 1; codepointIndex < codepointCount; codepointIndex++) {
    int advance;
    stbtt_GetCodepointHMetrics(font, (int)codepointIndex, &advance, 0);
    f32 horizontalAdvance = (f32)advance * loadedFont->scale;

    for (u32 otherCodepointIndex = 0; otherCodepointIndex < codepointCount; otherCodepointIndex++) {
      u32 offset = (codepointIndex * codepointCount) + otherCodepointIndex;
      *(horizontalAdvanceTable + offset) = horizontalAdvance;
    }
  }

  for (u32 codepointIndex = 0; codepointIndex < codepointCount; codepointIndex++) {
    for (u32 otherCodepointIndex = 0; otherCodepointIndex < codepointCount; otherCodepointIndex++) {
      int kerningAdvance = stbtt_GetCodepointKernAdvance(font, (int)otherCodepointIndex, (int)codepointIndex);
      if (kerningAdvance == 0)
        continue;
      f32 kerningAdvanceScaled = (f32)kerningAdvance * loadedFont->scale;

      u32 offset = (codepointIndex * codepointCount) + otherCodepointIndex;
      *(horizontalAdvanceTable + offset) += kerningAdvanceScaled;
    }
  }

  result.loadedFont = loadedFont;

  return result;
}

internal struct load_font_glyph_result
LoadFontGlyph(struct loaded_font *loadedFont, u32 codepoint)
{
  struct load_font_glyph_result result = {};
  stbtt_fontinfo *font = &loadedFont->font;
  f32 scale = loadedFont->scale;
  s32 baseline = loadedFont->baseline;

  int x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBox(font, (int)codepoint, scale, scale, &x0, &y0, &x1, &y1);

  // stbtt renders bitmap in top down left right, we pack in bottom up left right order.
  // Y axis operations are flipped.
  result.alignPercentageY = (f32)(baseline + y1) / (f32)(y1 - y0);

  int leftSideBearing;
  stbtt_GetCodepointHMetrics(font, (int)codepoint, 0, &leftSideBearing);
  f32 bearingX = (f32)leftSideBearing * loadedFont->scale;
  result.alignPercentageX = (1.0f - bearingX) / (f32)(x1 - x0);

  s32 width;
  s32 height;
  s32 xOffset;
  s32 yOffset;
  // 8bpp, stored as left-to-right, top-to-bottom
  u8 *codepointBitmap = stbtt_GetCodepointBitmap(font, 0, scale, (int)codepoint, &width, &height, &xOffset, &yOffset);
  if (!codepointBitmap) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    return result;
  }

  // transform
  struct loaded_bitmap loadedBitmap = {};
  loadedBitmap.width = (u32)width;
  loadedBitmap.height = (u32)height;
  loadedBitmap.stride = loadedBitmap.width * sizeof(u32);
  loadedBitmap.memory = AllocateMemory(loadedBitmap.height * loadedBitmap.stride);
  if (!loadedBitmap.memory) {
    result.error = HH_ASSET_BUILDER_ERROR_MALLOC;
    goto cleanupCodepointBitmap;
  }

  u8 *srcRow = codepointBitmap + (width * (height - 1)); // start at bottom left
  s32 srcStride = -width;                                // go up
  u8 *destRow = loadedBitmap.memory;
  u32 destStride = loadedBitmap.stride;
  for (u32 y = 0; y < loadedBitmap.height; y++) {
    u8 *src = srcRow;
    u32 *dest = (u32 *)destRow;
    for (u32 x = 0; x < loadedBitmap.width; x++) {
      f32 a = (f32)*src++;

      // texel = sRGB255toLinear1(texel);
      a /= 255.0f;

      /*
       * Store channels values pre-multiplied with alpha.
       */
      // v3_mul_ref(&texel.rgb, texel.a);
      f32 r = 1.0f * a;
      f32 g = 1.0f * a;
      f32 b = 1.0f * a;

      // texel = Linear1tosRGB255(texel);
      r = 255.0f * SquareRoot(r);
      g = 255.0f * SquareRoot(g);
      b = 255.0f * SquareRoot(b);
      a = 255.0f * a;

      *dest++ = (u32)(a + 0.5f) << 0x18 | (u32)(r + 0.5f) << 0x10 | (u32)(g + 0.5f) << 0x08 | (u32)(b + 0.5f) << 0x00;
    }

    srcRow += srcStride;
    destRow += destStride;
  }

  result.loadedBitmap = loadedBitmap;

  // cleanup
cleanupCodepointBitmap:
  stbtt_FreeBitmap(codepointBitmap, 0);

  return result;
}

#endif
/*****************************************************************
 * PACKING
 *****************************************************************/

internal enum hh_asset_builder_error
WriteHHAFile(char *filename, struct asset_context *context)
{
  enum hh_asset_builder_error errorCode = HH_ASSET_BUILDER_ERROR_NONE;

  char logBuffer[256];
  s64 logLength;

  int outFd = -1;
  outFd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (outFd < 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot open file\n  filename: %s\n", filename);
    assert(logLength > 0);
    fatal(logBuffer, (u64)logLength);

    errorCode = HH_ASSET_BUILDER_ERROR_IO_OPEN;
    goto exitWithErrorCode;
  }

  struct hha_header header = {
      .magic = HHA_MAGIC,
      .version = HHA_VERSION,
  };

  // TODO: First tag is null always. Should it be written to file? => count -1
  header.tagCount = context->tagCount;
  header.assetCount = context->assetCount;
  // TODO: sparseness?
  header.assetTypeCount = ASSET_TYPE_COUNT;
  header.tagsOffset = sizeof(header);
  header.assetTypesOffset = header.tagsOffset + (header.tagCount * sizeof(*context->tags));
  header.assetsOffset = header.assetTypesOffset + (header.assetTypeCount * sizeof(*context->assetTypes));

  s64 writtenBytes = write(outFd, &header, sizeof(header));
  assert(writtenBytes > 0);

  // 1 - tags
  struct hha_tag *tags = context->tags;
  if (header.tagCount) {
    // TODO: Should first tag be null?
    u64 tagArraySize = sizeof(*tags) * header.tagCount;
    writtenBytes = write(outFd, tags, tagArraySize);
    assert(writtenBytes > 0);

    logLength = snprintf(logBuffer, sizeof(logBuffer), "%" PRIu32 " tags written\n", header.tagCount);
    assert(logLength > 0);
    info(logBuffer, (u64)logLength);
  }

  // 2 - assetTypes
  struct hha_asset_type *assetTypes = context->assetTypes;
  u64 assetTypeArraySize = sizeof(*assetTypes) * header.assetTypeCount;
  writtenBytes = write(outFd, assetTypes, assetTypeArraySize);
  assert(writtenBytes > 0);

  logLength = snprintf(logBuffer, sizeof(logBuffer), "%" PRIu32 " asset types written\n", header.assetTypeCount);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

  // 3 - assets
  u64 assetArraySize = sizeof(struct hha_asset) * header.assetCount;
  s64 seekResult = lseek64(outFd, (s64)assetArraySize, SEEK_CUR);
  assert(seekResult != -1 && "file seek failed");
  for (u32 assetIndex = 1; assetIndex < header.assetCount; assetIndex++) {
    struct asset_metadata *src = context->assetMetadatas + assetIndex;
    struct hha_asset *dest = context->assets + assetIndex;

    dest->tagIndexFirst = src->tagIndexFirst;
    dest->tagIndexOnePastLast = src->tagIndexOnePastLast;

    s64 lseekResult = lseek64(outFd, 0, SEEK_CUR);
    assert(lseekResult != -1);
    dest->dataOffset = (u64)lseekResult;

    switch (src->type) {
    case ASSET_METADATA_TYPE_AUDIO: {
      struct audio_info *audioInfo = &src->audioInfo;
      struct load_wav_result loadWavResult =
          LoadWav(audioInfo->filename, audioInfo->sampleIndex, audioInfo->sampleCount);
      if (loadWavResult.error != HH_ASSET_BUILDER_ERROR_NONE) {
        switch (loadWavResult.error) {
        case HH_ASSET_BUILDER_ERROR_IO_OPEN:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "file cannot be opened\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_IS_NOT_FILE:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "given path is not file\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_STAT:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "stat failed\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_MALLOC:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot allocate memory\n");
          break;
        case HH_ASSET_BUILDER_ERROR_IO_READ:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "cannot read file.\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_SEEK:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "cannot seek file.\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_BMP_INVALID_MAGIC:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "file is not bmp.\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_BMP_MALFORMED:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "bmp is malformed.\n  filename: %s\n", audioInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_BMP_IS_NOT_ENCODED_PROPERLY:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "bmp could not encoded properly.\n  filename: %s\n",
                               audioInfo->filename);
          break;

        default:
          assert(0 && "error not presented to user");
          break;
        };
        assert(logLength > 0);
        error(logBuffer, (u64)logLength);

        errorCode = loadWavResult.error;
        continue;
      }

      struct loaded_audio *loadedAudio = &loadWavResult.loadedAudio;

      dest->audio.channelCount = loadedAudio->channelCount;
      dest->audio.sampleCount = loadedAudio->sampleCount;

      for (u32 channelIndex = 0; channelIndex < loadedAudio->channelCount; channelIndex++) {
        writtenBytes = write(outFd, loadedAudio->samples[channelIndex], loadedAudio->sampleCount * sizeof(s16));
        assert(writtenBytes > 0);
      }

      FreeWav(loadedAudio);
    } break;

    case ASSET_METADATA_TYPE_BITMAP: {
      struct bitmap_info *bitmapInfo = &src->bitmapInfo;
      struct load_bmp_result loadBmpResult = LoadBmp(bitmapInfo->filename);
      if (loadBmpResult.error != HH_ASSET_BUILDER_ERROR_NONE) {
        switch (loadBmpResult.error) {
        case HH_ASSET_BUILDER_ERROR_IO_OPEN:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "file cannot be opened\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_IS_NOT_FILE:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "given path is not file\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_STAT:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "stat failed\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_MALLOC:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot allocate memory\n");
          break;
        case HH_ASSET_BUILDER_ERROR_IO_READ:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "cannot read file.\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_SEEK:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "cannot seek file.\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_INVALID_MAGIC:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "file is not wav.\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_MALFORMED:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "wav is malformed.\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_PCM:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "wav format is not PCM.\n  filename: %s\n", bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_SAMPLE_RATE_IS_NOT_48000:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "wav sample rate is not 48000.\n  filename: %s\n",
                               bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_FORMAT_IS_NOT_S16LE:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "wav format is not S16LE.\n  filename: %s\n",
                               bitmapInfo->filename);
          break;
        case HH_ASSET_BUILDER_ERROR_WAV_NUM_CHANNELS_IS_BIGGER_THAN_2:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "wav has more channels than 2.\n  filename: %s\n",
                               bitmapInfo->filename);
          break;

        default:
          assert(0 && "error not presented to user");
          break;
        };
        assert(logLength > 0);
        error(logBuffer, (u64)logLength);

        errorCode = loadBmpResult.error;
        continue;
      }

      struct loaded_bitmap *loadedBitmap = &loadBmpResult.loadedBitmap;

      dest->bitmap.width = loadedBitmap->width;
      dest->bitmap.height = loadedBitmap->height;
      dest->bitmap.alignPercentage[0] = bitmapInfo->alignPercentageX;
      dest->bitmap.alignPercentage[1] = bitmapInfo->alignPercentageY;

      writtenBytes = write(outFd, loadedBitmap->memory, (size_t)(loadedBitmap->stride * loadedBitmap->height));
      assert(writtenBytes > 0);

      FreeBmp(loadedBitmap);
    } break;

    case ASSET_METADATA_TYPE_FONT: {
      struct font_info *fontInfo = &src->fontInfo;
      struct load_font_result loadFontResult =
          LoadFont(fontInfo->fontPath, fontInfo->codepointCount, fontInfo->horizontalAdvanceTable);

      if (loadFontResult.error != HH_ASSET_BUILDER_ERROR_NONE) {
        switch (loadFontResult.error) {
        case HH_ASSET_BUILDER_ERROR_IO_OPEN:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "file cannot be opened\n  filename: %s\n", fontInfo->fontPath);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_IS_NOT_FILE:
          logLength =
              snprintf(logBuffer, sizeof(logBuffer), "given path is not file\n  filename: %s\n", fontInfo->fontPath);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_STAT:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "stat failed\n  filename: %s\n", fontInfo->fontPath);
          break;
        case HH_ASSET_BUILDER_ERROR_MALLOC:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot allocate memory\n");
          break;
        case HH_ASSET_BUILDER_ERROR_IO_READ:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot read file.\n  filename: %s\n", fontInfo->fontPath);
          break;
        case HH_ASSET_BUILDER_ERROR_IO_SEEK:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot seek file.\n  filename: %s\n", fontInfo->fontPath);
          break;
        case HH_ASSET_BUILDER_ERROR_TTF_MALFORMED:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "file is not ttf or malformed.\n  filename: %s\n",
                               fontInfo->fontPath);
          break;
        default:
          assert(0 && "error not presented to user");
          break;
        };
        assert(logLength > 0);
        error(logBuffer, (u64)logLength);

        errorCode = loadFontResult.error;
        continue;
      }

      struct loaded_font *loadedFont = loadFontResult.loadedFont;
      dest->font.codepointCount = fontInfo->codepointCount;
      dest->font.ascent = loadedFont->ascent;
      dest->font.descent = loadedFont->descent;
      dest->font.lineGap = loadedFont->lineGap;

      u32 codepointsSize = fontInfo->codepointCount * sizeof(struct bitmap_id);
      writtenBytes = write(outFd, fontInfo->codepoints, codepointsSize);
      assert(writtenBytes > 0);

      u32 horizontalAdvanceTableSize = fontInfo->codepointCount * fontInfo->codepointCount * sizeof(f32);
      writtenBytes = write(outFd, fontInfo->horizontalAdvanceTable, horizontalAdvanceTableSize);
      assert(writtenBytes > 0);

      DeallocateMemory(fontInfo->codepoints);
      DeallocateMemory(fontInfo->horizontalAdvanceTable);

      fontInfo->loadedFont = loadedFont;
    } break;

    case ASSET_METADATA_TYPE_FONT_GLYPH: {
      struct font_glyph_info *fontGlyphInfo = &src->fontGlyphInfo;
      struct font_info *fontInfo = &(context->assetMetadatas + fontGlyphInfo->fontId.value)->fontInfo;
      struct load_font_glyph_result loadFontGlyphResult = LoadFontGlyph(fontInfo->loadedFont, fontGlyphInfo->codepoint);
      if (loadFontGlyphResult.error != HH_ASSET_BUILDER_ERROR_NONE) {
        switch (loadFontGlyphResult.error) {
        case HH_ASSET_BUILDER_ERROR_MALLOC:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot allocate memory\n");
          break;
        case HH_ASSET_BUILDER_ERROR_TTF_MALFORMED:
          logLength = snprintf(logBuffer, sizeof(logBuffer), "file is not ttf or malformed.\n  filename: %s\n",
                               fontInfo->fontPath);
          break;
        default:
          assert(0 && "error not presented to user");
          break;
        };
        assert(logLength > 0);
        error(logBuffer, (u64)logLength);

        errorCode = loadFontGlyphResult.error;
        continue;
      }

      struct loaded_bitmap *loadedBitmap = &loadFontGlyphResult.loadedBitmap;
      dest->bitmap.width = loadedBitmap->width;
      dest->bitmap.height = loadedBitmap->height;
      dest->bitmap.alignPercentage[0] = loadFontGlyphResult.alignPercentageX;
      dest->bitmap.alignPercentage[1] = loadFontGlyphResult.alignPercentageY;

      writtenBytes = write(outFd, loadedBitmap->memory, (size_t)(loadedBitmap->stride * loadedBitmap->height));
      assert(writtenBytes > 0);

      DeallocateMemory(loadedBitmap->memory);

      if (fontInfo->writtenCodepointCount == fontInfo->codepointCount) {
        // on last codepoint from file close it
        DeallocateMemory(fontInfo->loadedFont->_filememory);
        DeallocateMemory(fontInfo->loadedFont);
      }
      fontInfo->writtenCodepointCount++;
    } break;
    }
  }

  seekResult = lseek64(outFd, (s64)header.assetsOffset, SEEK_SET);
  if (seekResult == -1) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "file seek failed\n  filename: '%s'\n", filename);
    assert(logLength > 0);
    fatal(logBuffer, (u64)logLength);

    errorCode = HH_ASSET_BUILDER_ERROR_IO_SEEK;
    goto fsyncOutFd;
  }

  struct hha_asset *assets = context->assets;
  writtenBytes = write(outFd, assets, assetArraySize);
  assert(writtenBytes > 0);
  assert(writtenBytes > 0);

  logLength = snprintf(logBuffer, sizeof(logBuffer), "%" PRIu32 " assets written\n", header.assetCount);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

  // Packing finished
  s32 fsyncResult;
fsyncOutFd:
  fsyncResult = fsync(outFd);
  if (fsyncResult != 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot sync data\n");
    assert(logLength > 0);
    warn(logBuffer, (u64)logLength);
  }

  s32 closeResult = close(outFd);
  if (closeResult != 0) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "cannot close the file.\n  filename: '%s'\n", filename);
    assert(logLength > 0);
    warn(logBuffer, (u64)logLength);
  }
  outFd = -1;

  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE) {
    logLength = snprintf(logBuffer, sizeof(logBuffer), "Assets could NOT be packed into '%s'!\n", filename);
    assert(logLength > 0);
    fatal(logBuffer, (u64)logLength);
    goto exitWithErrorCode;
  }

  logLength = snprintf(logBuffer, sizeof(logBuffer), "Assets are packed into '%s' successfully.\n", filename);
  assert(logLength > 0);
  info(logBuffer, (u64)logLength);

exitWithErrorCode:
  return errorCode;
}

internal enum hh_asset_builder_error
WriteAll(char *outFilename)
{
  /*----------------------------------------------------------------
   * INGEST
   *----------------------------------------------------------------*/
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  BeginAssetType(context, ASSET_TYPE_SHADOW);
  AddBitmapAsset(context, "test/test_hero_shadow.bmp", 0.5f, 0.156682029f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_TREE);
  AddBitmapAsset(context, "test2/tree00.bmp", 0.493827164f, 0.295652181f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_SWORD);
  AddBitmapAsset(context, "test2/rock03.bmp", 0.5f, 0.65625f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GRASS);
  AddBitmapAsset(context, "test2/grass00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/grass01.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GROUND);
  AddBitmapAsset(context, "test2/ground00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground02.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground03.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_TUFT);
  AddBitmapAsset(context, "test2/tuft00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/tuft01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/tuft02.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  f32 angleRight = 0.00f * TAU32;
  f32 angleBack = 0.25f * TAU32;
  f32 angleLeft = 0.50f * TAU32;
  f32 angleFront = 0.75f * TAU32;

  BeginAssetType(context, ASSET_TYPE_HEAD);

  AddBitmapAsset(context, "test/test_hero_right_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(context, "test/test_hero_back_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(context, "test/test_hero_left_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(context, "test/test_hero_front_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_TORSO);

  AddBitmapAsset(context, "test/test_hero_right_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(context, "test/test_hero_back_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(context, "test/test_hero_left_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(context, "test/test_hero_front_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_CAPE);

  AddBitmapAsset(context, "test/test_hero_right_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(context, "test/test_hero_back_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(context, "test/test_hero_left_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(context, "test/test_hero_front_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(context);

  // audios
  BeginAssetType(context, ASSET_TYPE_BLOOP);
  AddAudioAsset(context, "test3/bloop_00.wav");
  AddAudioAsset(context, "test3/bloop_01.wav");
  AddAudioAsset(context, "test3/bloop_02.wav");
  AddAudioAsset(context, "test3/bloop_03.wav");
  AddAudioAsset(context, "test3/bloop_04.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_CRACK);
  AddAudioAsset(context, "test3/crack_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_DROP);
  AddAudioAsset(context, "test3/drop_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GLIDE);
  AddAudioAsset(context, "test3/glide_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_MUSIC);
  u32 sampleRate = 48000;
  u32 chunkSampleCount = 10 * sampleRate;
  u32 totalSampleCount = 7468095;

  for (u32 sampleIndex = 0; sampleIndex < totalSampleCount; sampleIndex += chunkSampleCount) {
    u32 sampleCount = totalSampleCount - sampleIndex;
    if (sampleCount > chunkSampleCount) {
      sampleCount = chunkSampleCount;
    }

    struct audio_id thisMusic = AddAudioAssetTrimmed(context, "test3/music_test.wav", sampleIndex, sampleCount);
    if ((sampleIndex + chunkSampleCount) < totalSampleCount) {
      struct hha_audio *thisAudio = &(context->assets + thisMusic.value)->audio;
      thisAudio->chain = HHA_AUDIO_CHAIN_ADVANCE;
    }
  }
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_PUHP);
  AddAudioAsset(context, "test3/puhp_00.wav");
  AddAudioAsset(context, "test3/puhp_01.wav");
  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

internal enum hh_asset_builder_error
WriteOnlyHero1(void)
{
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  BeginAssetType(context, ASSET_TYPE_SHADOW);
  AddBitmapAsset(context, "test/test_hero_shadow.bmp", 0.5f, 0.156682029f);
  EndAssetType(context);

  f32 angleRight = 0.00f * TAU32;
  f32 angleBack = 0.25f * TAU32;
  f32 angleLeft = 0.50f * TAU32;
  f32 angleFront = 0.75f * TAU32;

  BeginAssetType(context, ASSET_TYPE_HEAD);

  AddBitmapAsset(context, "test/test_hero_right_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);

  AddBitmapAsset(context, "test/test_hero_back_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);

  AddBitmapAsset(context, "test/test_hero_left_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(context, "test/test_hero_front_head.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_TORSO);
  AddBitmapAsset(context, "test/test_hero_right_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);
  AddBitmapAsset(context, "test/test_hero_back_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);
  AddBitmapAsset(context, "test/test_hero_left_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);
  AddBitmapAsset(context, "test/test_hero_front_torso.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_CAPE);
  AddBitmapAsset(context, "test/test_hero_right_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleRight);
  AddBitmapAsset(context, "test/test_hero_back_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleBack);
  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  char outFilename[] = "test1.hha";
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

internal enum hh_asset_builder_error
WriteOnlyHero2(void)
{
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  f32 angleRight = 0.00f * TAU32;
  f32 angleBack = 0.25f * TAU32;
  f32 angleLeft = 0.50f * TAU32;
  f32 angleFront = 0.75f * TAU32;

  BeginAssetType(context, ASSET_TYPE_CAPE);

  AddBitmapAsset(context, "test/test_hero_left_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleLeft);

  AddBitmapAsset(context, "test/test_hero_front_cape.bmp", 0.5f, 0.156682029f);
  AddAssetTag(context, ASSET_TAG_FACING_DIRECTION, angleFront);

  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  char outFilename[] = "test2.hha";
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

internal enum hh_asset_builder_error
WriteNoneHero(void)
{
  /*----------------------------------------------------------------
   * INGEST
   *----------------------------------------------------------------*/
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  BeginAssetType(context, ASSET_TYPE_TREE);
  AddBitmapAsset(context, "test2/tree00.bmp", 0.493827164f, 0.295652181f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_SWORD);
  AddBitmapAsset(context, "test2/rock03.bmp", 0.5f, 0.65625f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GRASS);
  AddBitmapAsset(context, "test2/grass00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/grass01.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GROUND);
  AddBitmapAsset(context, "test2/ground00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground02.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/ground03.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_TUFT);
  AddBitmapAsset(context, "test2/tuft00.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/tuft01.bmp", 0.5f, 0.5f);
  AddBitmapAsset(context, "test2/tuft02.bmp", 0.5f, 0.5f);
  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  char outFilename[] = "test3.hha";
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

internal enum hh_asset_builder_error
WriteAudios(void)
{
  /*----------------------------------------------------------------
   * INGEST
   *----------------------------------------------------------------*/
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  // audios
  BeginAssetType(context, ASSET_TYPE_BLOOP);
  AddAudioAsset(context, "test3/bloop_00.wav");
  AddAudioAsset(context, "test3/bloop_01.wav");
  AddAudioAsset(context, "test3/bloop_02.wav");
  AddAudioAsset(context, "test3/bloop_03.wav");
  AddAudioAsset(context, "test3/bloop_04.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_CRACK);
  AddAudioAsset(context, "test3/crack_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_DROP);
  AddAudioAsset(context, "test3/drop_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_GLIDE);
  AddAudioAsset(context, "test3/glide_00.wav");
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_MUSIC);
  u32 sampleRate = 48000;
  u32 chunkSampleCount = 10 * sampleRate;
  u32 totalSampleCount = 7468095;

  for (u32 sampleIndex = 0; sampleIndex < totalSampleCount; sampleIndex += chunkSampleCount) {
    u32 sampleCount = totalSampleCount - sampleIndex;
    if (sampleCount > chunkSampleCount) {
      sampleCount = chunkSampleCount;
    }

    struct audio_id thisMusic = AddAudioAssetTrimmed(context, "test3/music_test.wav", sampleIndex, sampleCount);
    if ((sampleIndex + chunkSampleCount) < totalSampleCount) {
      struct hha_audio *thisAudio = &(context->assets + thisMusic.value)->audio;
      thisAudio->chain = HHA_AUDIO_CHAIN_ADVANCE;
    }
  }
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_PUHP);
  AddAudioAsset(context, "test3/puhp_00.wav");
  AddAudioAsset(context, "test3/puhp_01.wav");
  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  char outFilename[] = "test4.hha";
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

internal enum hh_asset_builder_error
WriteFonts(void)
{
  /*----------------------------------------------------------------
   * INGEST
   *----------------------------------------------------------------*/
  struct asset_context *context = &(struct asset_context){};

  context->tagCount = 0;
  context->assetCount = 1;

  // fonts
  BeginAssetType(context, ASSET_TYPE_FONT);
  char *fontPath = "/usr/share/fonts/liberation-fonts/LiberationSerif-Regular.ttf";
  struct font_id fontId = AddFontAsset(context, fontPath, ('~' + 1));
  EndAssetType(context);

  BeginAssetType(context, ASSET_TYPE_FONT_GLYPH);
  struct font_info *fontInfo = &(context->assetMetadatas + fontId.value)->fontInfo;
  for (u32 codepoint = '!'; codepoint <= '~'; codepoint++) {
    struct bitmap_id bitmapId = AddFontGlyphAsset(context, fontId, codepoint);
    fontInfo->codepoints[codepoint] = bitmapId;
  }
  EndAssetType(context);

  /*----------------------------------------------------------------
   * PACKING
   *----------------------------------------------------------------*/
  char outFilename[] = "test5_fonts.hha";
  enum hh_asset_builder_error errorCode = WriteHHAFile(outFilename, context);
  return errorCode;
}

/*****************************************************************
 * STARTING POINT
 *****************************************************************/

int
main(int argc, char *argv[])
{
  s32 errorCode = 0;

  // argc 0 is program path
  char *exepath = argv[0];
  argc--;
  argv++;

  // parse arguments
  if (argc >= 2) {
    usage();
    errorCode = HH_ASSET_BUILDER_ERROR_ARGUMENTS;
    goto end;
  }

  char *outFilename = "test.hha";
  if (argc == 1)
    outFilename = argv[1];

  // errorCode = (s32)WriteAll(outFilename);

  errorCode = (s32)WriteOnlyHero1();
  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE)
    goto end;

  errorCode = (s32)WriteOnlyHero2();
  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE)
    goto end;

  errorCode = (s32)WriteNoneHero();
  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE)
    goto end;

  errorCode = (s32)WriteAudios();
  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE)
    goto end;

  errorCode = (s32)WriteFonts();
  if (errorCode != HH_ASSET_BUILDER_ERROR_NONE)
    goto end;

end:
  return errorCode;
}
