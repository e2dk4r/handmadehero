#ifndef HANDMADEHERO_H
#define HANDMADEHERO_H

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

enum game_asset_id {
  GAI_Background,
  GAI_Shadow,
  GAI_Tree,
  GAI_Sword,
  GAI_Stairwell,

  GAI_COUNT
};

struct bitmap_hero {
  struct bitmap head;
  struct bitmap torso;
  struct bitmap cape;
};

enum asset_state {
  ASSET_STATE_UNLOADED,
  ASSET_STATE_QUEUED,
  ASSET_STATE_LOADED,
  ASSET_STATE_LOCKED,
};

struct asset_slot {
  enum asset_state state;
  struct bitmap *bitmap;
};

struct game_assets {
  // TODO(e2dk4r): copy of known, not ideal because
  // we want AssetLoad to called from anywhere
  struct transient_state *transientState;
  struct memory_arena arena;
  pfnPlatformReadEntireFile PlatformReadEntireFile;

  struct asset_slot slots[GAI_COUNT];

  // array'd assets
  struct bitmap textureGrass[2];
  struct bitmap textureGround[4];
  struct bitmap textureTuft[3];

  // structures assets
#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0
  struct bitmap_hero textureHero[4];
};

internal inline struct bitmap *
AssetTextureGet(struct game_assets *assets, enum game_asset_id assetId)
{
  if (assetId >= GAI_COUNT)
    return 0;

  struct asset_slot *slot = assets->slots + assetId;
  return slot->bitmap;
}

void
AssetLoad(struct game_assets *assets, enum game_asset_id assetId);

struct game_state {
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
};

struct task_with_memory {
  b32 isUsed : 1;
  struct memory_arena arena;
  struct memory_temp memoryFlush;
};

struct transient_state {
  u8 initialized : 1;

  struct memory_arena transientArena;
  struct task_with_memory tasks[4];

  u32 groundBufferCount;
  struct ground_buffer *groundBuffers;

  struct platform_work_queue *highPriorityQueue;
  struct platform_work_queue *lowPriorityQueue;

  u32 envMapWidth;
  u32 envMapHeight;
  // NOTE(e2dk4r): 0 is bottom, 1 is middle, 2 is top
#define ENV_MAP_BOTTOM 0
#define ENV_MAP_MIDDLE 1
#define ENV_MAP_TOP 2
  struct environment_map envMaps[3];

  struct game_assets assets;
};

void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer);
typedef void (*pfnGameUpdateAndRender)(struct game_memory *memory, struct game_input *input,
                                       struct game_backbuffer *backbuffer);

struct stored_entity *
StoredEntityGet(struct game_state *state, u32 index);

struct pairwise_collision_rule *
CollisionRuleGet(struct game_state *state, u32 storageIndexA);

void
CollisionRuleAdd(struct game_state *state, u32 storageIndexA, u32 storageIndexB, u8 shouldCollide);

#endif /* HANDMADEHERO_H */
