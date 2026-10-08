#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/mad_chaser.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

extern EnemyParams   gMadChaserEnemyParams;  // the main enemy's `Enemy::param` record
extern AnimationSet* gMadChaserAnimBank[21]; // animation bank handed to `animationInitContext`
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gMadChaserMsgTable[3];   // stored into `Task::msgTable` by _madChaserSpawn
extern u8               gMadChaserAnimStance[];  // per animation id (1-based): value for `stateScratch`
extern u8               gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void _madChaserEmergeCreep3(Task* task);
static void _madChaserSpawn(Task* task);
static void _madChaserEmergeAtSpot(Task* task);
static void _madChaserLeapState(Task* task);
static void _madChaserShrink(Task* task);
static void _madChaserStartDespawn(Task* task);
static void _madChaserCombatToAlertState0(Task* task);
