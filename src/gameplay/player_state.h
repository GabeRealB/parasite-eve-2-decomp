#ifndef GAMEPLAY_PRIVATE_PLAYER_STATE_H
#define GAMEPLAY_PRIVATE_PLAYER_STATE_H

#include "types.h"

#include "actor.h"
#include "gameplay/actor_spawn_types.h"

#include "main/task_types.h"

Task* Gp_SetupAllyWeapon(void);

Task* Gp_SpawnAlly(const ActorSpawnTransform* spawnTransform, u16 arg1, s32 arg2, ActorSpawnOptions* options);

void func_80109FC4(Task* arg0);

void func_8010A42C(Task* arg0, s32 arg1);

void func_8010A670(Task* arg0);

/// Applies player HP damage with equipment modifiers and fatal-hit handling.
///
/// `damagePoints` is signed HP loss, narrowed to s16 by callers with no sign
/// clamp; negative values reverse the HP/MP changes. Holy Water removes the
/// arithmetic-shift quarter; MP Generation credits one MP per five reduced HP
/// before survival handling, capped at maximum MP. Impact resistance leaves one
/// HP on a lethal hit when starting HP is at least five. Active events likewise
/// retain one HP. Otherwise nonpositive resulting HP acquires a menu hold and
/// returns 1; every surviving path returns 0. Requires live player/session state
/// and a live player model when the resistance burst is emitted.
s32 playerStateApplyHpDamage(s16 damagePoints);

void func_8010AC54(Task* arg0);

void func_8010AD64(Task* arg0);

void Gp_PlayerStepSfx(Task* arg0);

/// Clears the actor's pending collision hit region, reaction and HP damage.
///
/// Requires live GameActor task work. Restores the ordinary reaction code;
/// the saved hit-body index is meaningful only while a hit region is pending.
void playerActorClearPendingHit(Task* task);

void func_8010B3F8(Task* arg0);

void func_8010B520(Task* arg0);

#endif // GAMEPLAY_PRIVATE_PLAYER_STATE_H
