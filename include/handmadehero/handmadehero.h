#ifndef HANDMADEHERO_H
#define HANDMADEHERO_H

#include "asset.h"
#include "memory_arena.h"
#include "platform.h"
#include "render_group.h"
#include "sim_region.h"
#include "world.h"

struct stored_entity {
  struct world_position position;
  struct entity sim;
};

struct pairwise_collision_rule {
  u8 shouldCollide : 1;
  u32 storageIndexA;
  u32 storageIndexB;

  struct pairwise_collision_rule *next;
};

struct controlled_hero {
  u32 entityIndex;

  struct v3 ddPosition;
  f32 dZ;
  struct v2 dSword;
};

struct ground_buffer {
  // NOTE(e2dk4r): invalid position tells us that this ground buffer has not been filled
  // NOTE(e2dk4r): this is center of the bitmap
  struct world_position position;
  struct bitmap bitmap;
};

struct hero_bitmap_ids {
  struct bitmap_id head;
  struct bitmap_id torso;
  struct bitmap_id cape;
};

struct playing_audio {
  struct audio_id id;
  f32 volume[2];
  u32 samplesPlayed;
  struct playing_audio *next;
};

struct game_state {
  b32 isInitialized : 1;

  struct memory_arena metaArena;
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct world_position cameraPosition;

  u32 storedEntityCount;
  struct stored_entity storedEntities[100000];

  struct controlled_hero controlledHeroes[ARRAY_COUNT(((struct game_input *)0)->controllers)];

  struct pairwise_collision_rule *collisionRules[256];
  struct pairwise_collision_rule *firstFreeCollisionRule;

  struct entity_collision_volume_group *heroCollision;
  struct entity_collision_volume_group *wallCollision;
  struct entity_collision_volume_group *familiarCollision;
  struct entity_collision_volume_group *monsterCollision;
  struct entity_collision_volume_group *swordCollision;
  struct entity_collision_volume_group *stairwellCollision;
  struct entity_collision_volume_group *roomCollision;

  f32 floorHeight;

  f32 time;

  struct bitmap testDiffuse;
  struct bitmap testNormal;

  struct playing_audio *firstPlayingAudio;
  struct playing_audio *firstFreePlayingAudio;
};

struct task_with_memory {
  b32 isUsed : 1;
  struct memory_arena arena;
  struct memory_temp memoryFlush;
};

struct transient_state {
  b32 isInitialized : 1;

  struct memory_arena transientArena;
  struct task_with_memory tasks[4];

  u32 groundBufferCount;
  struct ground_buffer *groundBuffers;

  struct platform_work_queue *highPriorityQueue;
  struct platform_work_queue *lowPriorityQueue;

  struct game_assets *assets;

  u32 envMapWidth;
  u32 envMapHeight;
  // NOTE(e2dk4r): 0 is bottom, 1 is middle, 2 is top
#define ENV_MAP_BOTTOM 0
#define ENV_MAP_MIDDLE 1
#define ENV_MAP_TOP 2
  struct environment_map envMaps[3];
};

struct stored_entity *
StoredEntityGet(struct game_state *state, u32 index);

struct pairwise_collision_rule *
CollisionRuleGet(struct game_state *state, u32 storageIndexA);

void
CollisionRuleAdd(struct game_state *state, u32 storageIndexA, u32 storageIndexB, u8 shouldCollide);

struct task_with_memory *
BeginTaskWithMemory(struct transient_state *transientState);

void
EndTaskWithMemory(struct task_with_memory *task);

#endif /* HANDMADEHERO_H */
