#ifndef GAMEPLAY_AREA_TRANSITIONS_H
#define GAMEPLAY_AREA_TRANSITIONS_H

#include "gameplay/area_flags.h"

#include "main/task_types.h"

// Area transitions and persistent area-flag updates.

struct GpAreaKey;

/// Mirror of `Gp_SetCurAreaFlag4` for an explicit key: clears bit 2 of
/// `GpAreaObj.field_1` on the record selected by `Gp_AreaTables[arg0->stage]`
/// + `arg0->area`. Null records are skipped, as in the setter.
void Gp_ClearAreaFlag4(struct GpAreaKey* arg0);

void func_800AEE8C(Task* arg0);

void Gp_ApplyAreaRecs(GpAreaApplyRec* arg0);

#endif // GAMEPLAY_AREA_TRANSITIONS_H
