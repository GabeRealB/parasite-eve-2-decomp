#ifndef ROOMS_ACROPOLIS_CAFETERIA_H
#define ROOMS_ACROPOLIS_CAFETERIA_H

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// 0xD8 work block the falling-debris task keeps at `Task::idMap`
/// (`Mem_Calloc(0xD8)` in `func_acropolis_cafeteria_801818DC`, released by
/// `func_acropolis_cafeteria_80181E3C` through `Gp_UnlinkObj`).
///
/// It opens with the `GpObj` list node linked onto `Gp_ObjLists[4]`, whose
/// `field_C` points at the six `GpRec18` slots that follow it in the same
/// block. `field_B0` is the spawn-time random seed / countdown
/// (`(rand() & 0xFFF) + 0x3000`, decremented every frame);
/// `field_B4` / `field_B8` / `field_BC` are the per-axis velocities added into
/// the object's coordinate; `field_C4` is the rotation handed to `RotMatrix`
/// and `field_CC` the normalised surface direction from `Gfx_MatrixCol2` /
/// `VectorNormalSS`; `field_D4` is the task's own sub-state.
typedef struct AcropolisCafeteriaDebris {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 slots[6];
    /* 0xB0 */ s32     field_B0;
    /* 0xB4 */ s32     field_B4;
    /* 0xB8 */ s32     field_B8;
    /* 0xBC */ s32     field_BC;
    /* 0xC0 */ byte    pad_C0[4];
    /* 0xC4 */ SVECTOR field_C4;
    /* 0xCC */ SVECTOR field_CC;
    /* 0xD4 */ u16     field_D4;
    /* 0xD6 */ byte    pad_D6[2];
} AcropolisCafeteriaDebris;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaDebris, 0xD8);

void func_807245E4(void*);
void func_80724608(void*, s32, s32, void*);

void func_acropolis_cafeteria_8017D6AC(void);
s32  func_acropolis_cafeteria_8017D700(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

/// Spawns 40 effects on entry to session mode 9, then two per tick while it
/// remains active. Releases the work block when the room effect gate clears.
void func_acropolis_cafeteria_8017E89C(Task* task);

extern s32 Gp_LcgState;
extern s32 D_acropolis_cafeteria_80184164;

#endif // ROOMS_ACROPOLIS_CAFETERIA_H
