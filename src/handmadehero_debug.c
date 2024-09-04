#include <handmadehero/math.h>
#include <handmadehero/render_group.h>
#include <handmadehero/text.h>

// TODO: stop using sprintf()
#include <stdio.h>

struct debug_counter_snapshot {
  u32 hitCount;
  u32 cycleCount;
};

struct debug_counter_state {
  char *filename;
  char *function;
  u32 line;

  struct debug_counter_snapshot snapshots[128];
};

struct debug_state {
  struct debug_counter_state counterStates[512];
  u32 counterCount;
  u32 snapshotCount;
};

struct debug_statistics {
  f64 min;
  f64 max;
  f64 avg;
  u32 count;
};

#if HANDMADEHERO_INTERNAL

global_variable f32 atY;
global_variable f32 leftEdge;
global_variable f32 fontScale;
global_variable struct font_id fontId;

void
DEBUGReset(struct game_assets *assets, u32 width, u32 height)
{
  struct asset_vector matchVector = {};
  struct asset_vector weightVector = {};
  matchVector.e[ASSET_TAG_FONT_TYPE] = ASSET_FONT_TYPE_DEBUG;
  weightVector.e[ASSET_TAG_FONT_TYPE] = 1.0f;
  fontId = BestMatchFont(assets, ASSET_TYPE_FONT, &matchVector, &weightVector);

  fontScale = 1.0f;
  RenderGroupOrthographic(DEBUG_TEXT_RENDER_GROUP, width, height, 1.0f);
  leftEdge = -0.5f * (f32)width;

  struct hha_font *fontInfo = FontInfoGet(assets, fontId);
  atY = (0.5f * (f32)height) - (FontGetStartingBaselineY(fontInfo) * fontScale);
}

void
DEBUGTextLine(char *line)
{
  struct render_group *renderGroup = DEBUG_TEXT_RENDER_GROUP;
  struct game_assets *assets = renderGroup->assets;

  struct font *font = Font(renderGroup, fontId);
  if (!font)
    return;

  struct hha_font *fontInfo = FontInfoGet(assets, fontId);

  f32 atX = leftEdge;

  u32 prevCodepoint = 0;

  struct v4 color = v4(1.0f, 1.0f, 1.0f, 1.0f);
  for (char *character = line; *character; /* handled in if */) {
    // color mode
    if (character[0] == '#' && IsHex(character[1]) && IsHex(character[2]) && IsHex(character[3]) &&
        IsHex(character[4]) && IsHex(character[5]) && IsHex(character[6]) && character[7] == '#') {
      u8 rHex = (u8)(GetHex(character[1]) << 4 | GetHex(character[2]) << 0);
      u8 gHex = (u8)(GetHex(character[3]) << 4 | GetHex(character[4]) << 0);
      u8 bHex = (u8)(GetHex(character[5]) << 4 | GetHex(character[6]) << 0);

      f32 r = (f32)rHex / 255.0f;
      f32 g = (f32)gHex / 255.0f;
      f32 b = (f32)bHex / 255.0f;
      color = v4(r, g, b, 1.0f);
      character += 8;
    }

#if 0
    // size mode
    if (character[0] == '/' && character[1] != 0 && character[2] == '/') {
      f32 characterScale = 1.0f / 9.0f;
      inlineScale = Clamp01((f32)(character[1] - '0') * characterScale);
      character += 3;
    }
#endif

    // character mode
    else {
      u32 codepoint = (u32)*character;

      if (character[0] == '/' && IsHex(character[1]) && IsHex(character[2]) && IsHex(character[3]) &&
          IsHex(character[4])) {
        codepoint = (u32)(GetHex(character[1]) << 12 | GetHex(character[2]) << 8 | GetHex(character[3]) << 4 |
                          GetHex(character[4]) << 0);
        character += 4;
      }

      f32 advanceX = fontScale * FontGetHorizontalAdvanceForPair(fontInfo, font, prevCodepoint, codepoint);
      atX += advanceX;

      if (codepoint != ' ') {
        struct bitmap_id bitmapId = FontGetBitmapGlyph(assets, fontInfo, font, codepoint);
        struct hha_bitmap *bitmapInfo = BitmapInfoGet(assets, bitmapId);

        f32 height = fontScale * (f32)bitmapInfo->height;
        BitmapAsset(renderGroup, bitmapId, v3(atX, atY, 0.0f), height, color);
      }

      prevCodepoint = codepoint;
      character++;
    }
  }

  atY -= fontScale * FontGetLineAdvance(fontInfo);
}

#endif

internal inline void
DebugStatisticsBegin(struct debug_statistics *statistics)
{
  statistics->min = F64_MAX;
  statistics->max = F64_LOWEST;
  statistics->avg = 0.0;
  statistics->count = 0;
}

internal inline void
DebugStatisticsAccumulate(struct debug_statistics *statistics, f64 value)
{
  if (value < statistics->min)
    statistics->min = value;

  if (value > statistics->max)
    statistics->max = value;

  statistics->avg += value;

  statistics->count++;
}

internal inline void
DebugStatisticsEnd(struct debug_statistics *statistics)
{
  if (statistics->count) {
    statistics->avg /= (f64)statistics->count;
  } else {
    statistics->min = 0.0;
    statistics->max = 0.0;
  }
}

internal void
OverlayCycleCounters(struct game_memory *memory)
{
  struct debug_state *debugState = memory->debugStorage;
  struct render_group *renderGroup = DEBUG_TEXT_RENDER_GROUP;
  if (!debugState || !renderGroup)
    return;

  struct font *font = Font(renderGroup, fontId);
  if (!font)
    return;
  struct hha_font *fontInfo = FontInfoGet(renderGroup->assets, fontId);

  DEBUGTextLine("#7f1d1d#CYCLE #10b981#COUNTS:");
  for (u32 counterIndex = 0; counterIndex < debugState->counterCount; counterIndex++) {
    struct debug_counter_state *counterState = debugState->counterStates + counterIndex;

    // gather statistics
    struct debug_statistics hitCount, cycleCount, cyclesPerHit;
    DebugStatisticsBegin(&hitCount);
    DebugStatisticsBegin(&cycleCount);
    DebugStatisticsBegin(&cyclesPerHit);
    for (u32 snapshotIndex = 0; snapshotIndex < ARRAY_COUNT(counterState->snapshots); snapshotIndex++) {
      struct debug_counter_snapshot *snapshot = counterState->snapshots + snapshotIndex;
      DebugStatisticsAccumulate(&hitCount, (f64)snapshot->hitCount);
      DebugStatisticsAccumulate(&cycleCount, (f64)snapshot->cycleCount);

      f64 cph = 0.0f;
      if (snapshot->hitCount)
        cph = (f64)snapshot->cycleCount / (f64)snapshot->hitCount;
      DebugStatisticsAccumulate(&cyclesPerHit, cph);
    }
    DebugStatisticsEnd(&hitCount);
    DebugStatisticsEnd(&cycleCount);
    DebugStatisticsEnd(&cyclesPerHit);

    // draw graph
    if (cycleCount.max > 0.0f) {
      f32 chartLeft = 275.0f;
      f32 chartMinY = atY;
      f32 chartHeight = fontInfo->ascent * fontScale;

      f32 scale = 1.0f / (f32)cycleCount.max;

      for (u32 snapshotIndex = 0; snapshotIndex < ARRAY_COUNT(counterState->snapshots); snapshotIndex++) {
        struct debug_counter_snapshot *snapshot = counterState->snapshots + snapshotIndex;
        if (snapshot->cycleCount == 0)
          continue;

        f32 thisProportion = scale * (f32)snapshot->cycleCount;
        f32 thisHeight = chartHeight * thisProportion;

        Rect(renderGroup, v3(chartLeft + (f32)snapshotIndex, chartMinY + thisHeight * 0.5f, 0.0f), v2(1.0f, thisHeight),
             v4(thisProportion, 1.0f, 0.0f, 1.0f));
      }
    }

    // draw text
    char buf[128];
    snprintf(buf, sizeof(buf), "%32s(%4u): %10ucy %8uh %10ucy/h", counterState->function, counterState->line,
             (u32)cycleCount.avg, (u32)hitCount.avg, (u32)cyclesPerHit.avg);
    DEBUGTextLine(buf);
  }
}
