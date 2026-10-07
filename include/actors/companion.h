#ifndef INCLUDE_ACTORS_COMPANION_H
#define INCLUDE_ACTORS_COMPANION_H

#include "types.h"

#include "main/task_types.h"

/// Overlay-imported s16 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and passed
/// to `playerActorResetWeaponAttack` (`_companionBeginScriptedControl` / `companionPlayScriptedAnimation` / `companionAwaitButtonPresses`).
extern s16 D_actor_800100_80167218[];

/// Overlay-imported s16 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and passed
/// as the third argument of `playerActorInitWeaponCollision` (`Gp_SetupAllyWeapon`).
extern s16 D_actor_800100_80167224[];

/// Overlay-imported u8 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and stored
/// at `CompanionWork.activity.combat.attacksRemaining` (`Gp_SetupAllyWeapon`).
extern u8 D_actor_800100_80167230[];

/// Starts the armed companion's reload behavior and stops its movement and turning.
///
/// Requires the live actor_800100 task with native animation and equipped-weapon
/// resources. `animationVariant` offsets native reload set 14 and is also stored
/// in the halfword auxiliary state; current callers pass 0. The low halfword of
/// 14 + the variant must select a loaded set, and the addition must fit s32.
/// Blends for one normal-rate frame and decays aim without clearing the target.
/// The reload handler restores the weapon's attack allowance and returns to
/// combat decisions when the clip finishes. Retains no new pointers.
void actor800100EnterReload(Task* task, s32 animationVariant);

#endif // INCLUDE_ACTORS_COMPANION_H
