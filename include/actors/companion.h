#ifndef INCLUDE_ACTORS_COMPANION_H
#define INCLUDE_ACTORS_COMPANION_H

#include "types.h"

#include "main/task_types.h"

/// Overlay-imported s16 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// Overlay-imported s16 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and passed
/// as the third argument of `Gp_AttachActorObj` (`Gp_SetupAllyWeapon`).
extern s16 D_80167224[];

/// Overlay-imported u8 table indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant` and stored
/// at `CompanionWork.activity.combat.attacksRemaining` (`Gp_SetupAllyWeapon`).
extern u8 D_80167230[];

/// Overlay import. `func_801088D4` calls it with `gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)` when
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 1`.
void func_actor_800100_80166E94(Task* arg0, s32 arg1);

#endif // INCLUDE_ACTORS_COMPANION_H
