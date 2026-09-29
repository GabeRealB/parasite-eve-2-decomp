#ifndef INCLUDE_ACTORS_COMPANION_H
#define INCLUDE_ACTORS_COMPANION_H

#include "types.h"

#include "main/task_types.h"

/// Overlay-imported s16 table indexed by `Mc_SaveData[0].state.companionVariant` and passed
/// to `func_80106350` (`func_8010C46C` / `func_8010C4F0` / `func_8010C75C`).
extern s16 D_80167218[];

/// Overlay-imported s16 table indexed by `Mc_SaveData[0].state.companionVariant` and passed
/// as the third argument of `Gp_AttachActorObj` (`Gp_SetupAllyWeapon`).
extern s16 D_80167224[];

/// Overlay-imported u8 table indexed by `Mc_SaveData[0].state.companionVariant` and stored
/// at `GpActorD4.actionCount` (`Gp_SetupAllyWeapon`).
extern u8 D_80167230[];

/// Overlay import. `func_801088D4` calls it with `gameGetPtrSlot(0xA)` when
/// `Mc_SaveData[0].state.companionType == 1`.
void func_80166E94(Task* arg0, s32 arg1);

#endif // INCLUDE_ACTORS_COMPANION_H
