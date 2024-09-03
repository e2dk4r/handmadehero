#include <handmadehero/math.h>
#include <handmadehero/render_group.h>

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
  if (!debugState)
    return;

  DEBUGTextLine("#7f1d1d#CYCLE #10b981#COUNTS:");
  for (u32 counterIndex = 0; counterIndex < debugState->counterCount; counterIndex++) {
    struct debug_counter_state *counterState = debugState->counterStates + counterIndex;

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

    if (hitCount.max <= 0.0f)
      continue;

    char buf[128];
    snprintf(buf, sizeof(buf), "%32s(%4u): %10ucy %8uh %10ucy/h", counterState->function, counterState->line,
             (u32)cycleCount.avg, (u32)hitCount.avg, (u32)cyclesPerHit.avg);
    DEBUGTextLine(buf);
  }
}
