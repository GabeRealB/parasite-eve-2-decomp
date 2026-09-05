#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/weapons_shared_8011d468.h"
#include "weapons/weapons_shared_8011d864.h"

extern s32 Gp_LcgState;

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
extern SVECTOR D_p229_8011E0F0;

/// The four flash angles rolled on the frame the shot goes off, one per
/// `WeaponsShared8011d864` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
extern s16 D_p229_8012B658[4];

/// Per-frame muzzle-flash task for the P229. Frame 0 claims room-coord slot 0
/// as a white 0x1000 light at the weapon's world position, parks the task's own
/// coordinate on the muzzle offset under the hand frame, and rolls the flash
/// size (`field_24`), its spin (`field_26`) and the four quad angles; every
/// later frame just halves the size and the brightness. Each frame then draws
/// the core (`WeaponsShared8011d468`), a full-screen fade at the current
/// brightness and the four flash quads, decays the light's range by 0x190 and
/// releases the pool block after seven frames. Nothing runs at all once
/// `Gp_State1C` is fading out (`field_4 >= 2`).
void func_p229_8011D1DC(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    u8             rgb[3];
    s32            i;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[0];
    slot  = (GpCoordTail*)&base->coord;

    if (Gp_State1C->field_4 >= 2) {
        return;
    }

    work->field_22++;
    switch (task->state) {
        case 0:
            slot->coord.coord.t[0] = coord->coord.t[0];
            slot->coord.coord.t[1] = coord->coord.t[1];
            slot->coord.coord.t[2] = coord->coord.t[2];
            base->coord.flg        = 0;
            slot->field_50         = 0x1000;
            slot->field_52         = 0x1000;
            slot->field_54         = 0x1000;
            slot->field_58         = 0xFA0;
            slot->field_5C         = 0x12C0;
            base->field_0          = 4;

            coord->sub        = work->field_8;
            coord->coord.t[0] = D_p229_8011E0F0.vx;
            coord->coord.t[1] = D_p229_8011E0F0.vy;
            coord->coord.t[2] = D_p229_8011E0F0.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);

            work->field_28 = 0xC0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_24 = (((u32)Gp_LcgState >> 16) & 0x3FF) + 0x600;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            task->state    = 1;
            for (i = 0; i < 4; i++) {
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                D_p229_8012B658[i] = ((i & 3) << 10) + (((u32)Gp_LcgState >> 16) & 0x3FF);
            }
            break;
        case 1:
            /* The `(u16)` casts are load-shape, not arithmetic: the ROM reads
               both fields with `lhu` and sign-extends in the shift pair
               (`sll 16` / `sra 17`). A plain `>>= 1` on the `s16` field emits
               `lh` / `sra 1` instead. */
            work->field_24 = (s16)(u16)work->field_24 >> 1;
            work->field_28 = (s16)(u16)work->field_28 >> 1;
            break;
    }

    WeaponsShared8011d468(coord, work->field_24, work->field_26);
    /* Chained on purpose: it is one `lbu` stored three times, in reverse index
       order. Three separate assignments reload the field each time, because the
       stores into `rgb` may alias it. */
    rgb[0] = rgb[1] = rgb[2] = work->field_28;
    Gp_DrawFadeQuad(rgb, 1);
    for (i = 0; i < 4; i++) {
        WeaponsShared8011d864(coord, D_p229_8012B658[i], work->field_28);
    }
    if (slot->field_58 >= 0x191) {
        slot->field_58 -= 0x190;
    }
    if (work->field_22 >= 7) {
        Gp_ReleaseState1CMem(work, task);
    }
}

INCLUDE_RODATA("weapons/nonmatchings/p229/p229", D_p229_8011D1C0);
