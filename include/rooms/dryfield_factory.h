#ifndef INCLUDE_ROOMS_DRYFIELD_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_FACTORY_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Stage-2 pad scripts referenced by the factory task in both stage variants.
extern PadScriptCmd D_dryfield_factory_8018A39C[3];

extern GpScriptRec D_dryfield_factory_8018A3A8[3];

// Variant-specific factory task and collision tables.
extern TaskDesc D_dryfield_factory_80186E28[];

extern GpGridParams D_dryfield_factory_80187BF8;

// dryfield_factory
extern GpRoomObjRec D_dryfield_factory_80186F10[];

extern u8* D_dryfield_factory_80186F44[];

extern GpRoomCoordRec D_dryfield_factory_80186F4C[];

extern GpViewCountRec D_dryfield_factory_80186F5C[];

extern GpWarpRec D_dryfield_factory_80186F60[];

extern GpViewRec D_dryfield_factory_80187C1C[];

extern GpSprtRec D_dryfield_factory_801895B0[];

extern GpRoomParamRec* D_dryfield_factory_8018A37C[];

/// Shows (non-zero) or hides (zero) the second sprite command of view 9 of the
/// current room, in stage 2 only.
void func_dryfield_factory_80181620(s32 show);

/// As `func_dryfield_factory_80181620`, for view 11.
void func_dryfield_factory_80181B38(s32 show);

void func_dryfield_factory_801825F0(Task* task);

void func_dryfield_factory_8017DF88(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_FACTORY_H
