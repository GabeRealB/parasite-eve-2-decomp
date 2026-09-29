#ifndef ROOMS_DRYFIELD_FACTORY_H
#define ROOMS_DRYFIELD_FACTORY_H

#include "gameplay/collision.h"
#include "main/task_types.h"

#include "common.h"
#include "gameplay/pad_script.h"

/// Shows (non-zero) or hides (zero) the second sprite command of view 9 of the
/// current room, in stage 2 only.
void func_dryfield_factory_80181620(s32 show);

/// As `func_dryfield_factory_80181620`, for view 11.
void func_dryfield_factory_80181B38(s32 show);

// Stage-2 pad scripts referenced by the factory task in both stage variants.
extern GpScriptCmd D_dryfield_factory_8018A39C[3];
extern GpScriptRec D_dryfield_factory_8018A3A8[3];

// Variant-specific factory task and collision tables.
extern TaskDesc D_dryfield_factory_80186E28[];
extern GpGridParams D_dryfield_factory_80187BF8;

void func_dryfield_factory_801825F0(Task* task);

#endif // ROOMS_DRYFIELD_FACTORY_H
