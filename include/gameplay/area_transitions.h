#ifndef GAMEPLAY_AREA_TRANSITIONS_H
#define GAMEPLAY_AREA_TRANSITIONS_H

#include "gameplay/area_flags.h"

#include "main/session_types.h"
#include "main/task_types.h"

// Area transitions and persistent area-flag updates.

/// Mirror of `Gp_SetCurAreaFlag4` for an explicit key: clears
/// `AREA_SAVED_MAP_MARK` in `AreaSavedState.spawnFlags` on the record selected
/// by `Gp_AreaTables[key->stage]` + `key->area`. Null records are skipped, as
/// in the setter.
void Gp_ClearAreaFlag4(GameLocationKey* key);

void func_800AEE8C(Task* arg0);

void Gp_ApplyAreaRecs(GpAreaApplyRec* arg0);

#endif // GAMEPLAY_AREA_TRANSITIONS_H
