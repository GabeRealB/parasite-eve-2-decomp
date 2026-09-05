#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/hypervelocity.h"

extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Teardown callback shared with the other weapon overlays; it unlinks
/// `Task::idMap` and releases the `Gp_State1C` work block.
void WeaponsShared8011e4ac(Task* task);

void func_hypervelocity_8011DF34(GsCOORDINATE2* coord, s16 age, s16 spin, s32 side);
void func_hypervelocity_8011E494(GsCOORDINATE2* coord, s16 age, s16 spin, s16 ang);
void func_hypervelocity_8011E8A0(GsCOORDINATE2* ground, s16 spin);

/// Per-frame task for the muzzle flare the hypervelocity round leaves behind.
/// `Task::spawnArg2` is the `Gp_State1C` work block holding the flare's drift
/// (`field_10` / `field_12` / `field_14`), its age (`field_22`), the ring
/// brightness (`field_24`), the ring radius (`field_26`), the arc brightness
/// (`field_28`) and the per-frame brightness step (`field_2A`);
/// `Task::extra` reaches the coordinate it hangs on and `Task::spawnArg1` is
/// the charge counter the firing code drives. Any room fade
/// (`Gp_State1C->field_4`) freezes the task, and a fade of 4 or more restarts
/// it at state 1.
///
/// - State 0 hangs the coordinate off `GpEffWork::field_8` at the fixed muzzle
///   offset `D_hypervelocity_8011FB74` with an identity rotation, then falls
///   through to state 1, which waits for `spawnArg1` to reach 1 before arming
///   the charge at state 2.
/// - State 2 charges: it jitters the drift, sparks every other frame, and
///   claims room-light slot 1 as a narrow (`0x100` / `0x1000`) light at a
///   random `0x400..0xB00` angle. A negative `spawnArg1` cancels back to state
///   1; holding past frame 0x40 caps the charge at 0x18; once the charge is 2
///   or more it seeds the ring and moves to state 3 with the brightness step
///   scaled so the ring fills over `spawnArg1` frames.
/// - State 3 fires: the light widens to `0x400` / `0x4000`, the ring brightens
///   by `field_2A` and grows by 8 a frame, both ring halves are drawn, and past
///   half brightness the arc is drawn too with a one-shot report. Running the
///   charge out spawns the discharge effect as a child task and moves to state
///   4; a negative charge cancels back to state 1 with the stop sound.
/// - State 4 fades the ring out 0x20 a frame while spawning smoke off a random
///   one of the player's two hand coordinates, and returns to state 1 once the
///   flare is 0x6F frames old or the charge goes negative.
void func_hypervelocity_8011D1E8(Task* task)
{
    u8             rgb[3];
    GsCOORDINATE2* coord;
    GsCOORDINATE2* light;
    GsCOORDINATE2* player;
    GpEffWork*     work;
    GpEffWork*     eff;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpMtxWords*    dstm;
    s32            pan;

    work  = task->spawnArg2;
    base  = &Gp_RoomCoords[1];
    light = &base->coord;
    slot  = (GpCoordTail*)light;
    coord = ((TmdObject*)task->extra)->field_8;

    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            task->state = 1;
        }
        return;
    }

    work->field_22 = (u16)work->field_22 + 1;
    switch (task->state) {
        case 0:
            dstm              = (GpMtxWords*)&coord->coord;
            coord->sub        = work->field_8;
            dstm->w0          = 0x1000;
            dstm->w1          = 0;
            dstm->w2          = 0x1000;
            dstm->w3          = 0;
            dstm->h4          = 0x1000;
            coord->coord.t[0] = D_hypervelocity_8011FB74.vx;
            coord->coord.t[1] = D_hypervelocity_8011FB74.vy;
            coord->coord.t[2] = D_hypervelocity_8011FB74.vz;
            coord->flg        = 0;
            task->state       = 1;
            /* fallthrough */
        case 1:
            if (task->spawnArg1 == 1) {
                task->state    = 2;
                work->field_22 = 0;
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            work->field_12 = -((work->field_22 & 0xF) << 5);
            if (work->field_22 & 1) {
                Gp_SpawnEff(0x600E1, coord, 0x180, (SVECTOR*)&work->field_10);
            }
            base->field_0  = 4;
            slot->field_58 = 0x100;
            slot->field_5C = 0x1000;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            slot->field_54 = (((u32)Gp_LcgState >> 16) & 0x700) + 0x400;
            slot->field_50 = (u16)slot->field_54 >> 1;
            slot->field_52 = (s16)(u16)slot->field_54 >> 1;
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
            light->flg = 0;
            if (task->spawnArg1 < 0) {
                task->spawnArg1 = 0;
                task->state     = 1;
                return;
            }
            if (work->field_22 >= 0x41) {
                task->spawnArg1 = 0x18;
            }
            if (task->spawnArg1 >= 2) {
                work->field_24 = 0;
                work->field_26 = 0x40;
                work->field_28 = 0;
                work->field_2A = 0x100 / task->spawnArg1;
                task->state    = 3;
            }
            return;
        case 3:
            Gp_UpdateCoord(coord);
            work->field_12 = -((work->field_22 & 0xF) << 6);
            Gp_SpawnEff(0x600E0, coord, 0x180, (SVECTOR*)&work->field_10);
            base->field_0  = 4;
            slot->field_58 = 0x400;
            slot->field_5C = 0x4000;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            slot->field_54 = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            slot->field_50 = (u16)slot->field_54 >> 1;
            slot->field_52 = (s16)(u16)slot->field_54 >> 1;
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
            light->flg      = 0;
            work->field_24 += work->field_2A;
            if (work->field_24 >= 0x100) {
                work->field_24 = 0xFF;
            }
            work->field_26 += 8;
            if (work->field_26 >= 0x201) {
                work->field_26 = 0x200;
            }
            rgb[0] = (u16)work->field_24 >> 1;
            rgb[1] = (u16)work->field_24 >> 1;
            rgb[2] = work->field_24;
            Gp_DrawRing(coord, work->field_26, rgb);
            Gp_DrawRing(coord, (s16)((u16)work->field_26 * 2), rgb);
            if (work->field_24 >= 0x81) {
                if (work->field_28 == 0) {
                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(0x20160006, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
                work->field_28 += (u16)work->field_2A * 2;
                if (work->field_28 >= 0x100) {
                    work->field_28 = 0xFF;
                }
                rgb[0] = (u16)work->field_28 >> 1;
                rgb[1] = (u16)work->field_28 >> 1;
                rgb[2] = work->field_28;
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 128), 0x60, rgb);
            }
            if (task->spawnArg1 < 0) {
                SndEvt_EnqueueType7(0x20160006, 1);
                task->spawnArg1 = 0;
                task->state     = 1;
                return;
            }
            task->spawnArg1 = task->spawnArg1 - 1;
            if (task->spawnArg1 == 0) {
                task->state = 4;
                eff         = Gp_SpawnEff(0x6000C, coord, 0, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->field_0);
                }
                work->field_24 = 0xFF;
            }
            return;
        case 4:
            Gp_UpdateCoord(coord);
            work->field_12 = -((work->field_22 & 0xF) << 6);
            Gp_SpawnEff(0x600E1, coord, 0x180, (SVECTOR*)&work->field_10);
            if (work->field_26 > 0) {
                rgb[0] = (u16)work->field_24 >> 1;
                rgb[1] = (u16)work->field_24 >> 1;
                rgb[2] = work->field_24;
                Gp_DrawRing(coord, work->field_26, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->field_26 * 2), rgb);
                Gp_DrawFadeQuad(rgb, 1);
                work->field_24 = (u16)work->field_24 - 0x20;
                work->field_26 = (u16)work->field_26 - 0x20;
            }
            player      = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60054, &player[((((u32)Gp_LcgState >> 16) & 1) * 3) + 15], 0x2300, NULL);
            if (work->field_22 >= 0x6F || task->spawnArg1 < 0) {
                task->state = 1;
            }
            return;
    }
}

/// Per-frame task for the hypervelocity round in flight. `Task::spawnArg2` is
/// the `Gp_State1C` work block holding the round's velocity (`field_10` /
/// `field_12` / `field_14`), its age (`field_22`), the trail brightness
/// (`field_24`), the ring spin (`field_26`) and the ring's start angle
/// (`field_28`); `Task::extra` reaches the coordinate it flies on. A room fade
/// (`Gp_State1C->field_4`) winds the age back down instead of advancing, and
/// tears the round down once the fade reaches 4.
///
/// - State 0 allocates the `HyperBeam` list node, copies the player's rotation
///   onto the round's own frame, rotates the fixed `(0, 0, 0x400)` muzzle
///   velocity through it, re-rolls the 16 trail jitters and the ring angle,
///   links the node, spawns the launch effect as a child task and claims room
///   -light slot 0.
/// - State 1 flies the round, draws the ring plus both trail halves, traces the
///   ground under it for a splash, and until frame 0x15 keeps spawning sparks.
///   It then re-aims the room light and asks `func_800DE7CC` whether the step
///   crossed geometry: a hit unlinks the node and switches to state 2, and
///   living past frame 0x15 releases the pool block.
/// - State 2 shrinks the ring by 0x40 a frame, spawning one more spark burst
///   per frame until the ring falls under 0x80.
void func_hypervelocity_8011D830(Task* task)
{
    GsCOORDINATE2  ground;
    SVECTOR        after;
    SVECTOR        before;
    u8             rgb[3];
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GsCOORDINATE2* light;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpEffWork*     work;
    GpEffWork*     eff;
    HyperBeam*     beam;
    GpMtxWords*    dstm;
    GpMtxWords*    srcm;
    u32            ang;
    s32            i;

    beam  = (HyperBeam*)task->idMap;
    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[0];
    light = &base->coord;
    slot  = (GpCoordTail*)light;

    if (Gp_State1C->field_4 != 0) {
        work->field_22 = (u16)work->field_22 - 1;
        if (Gp_State1C->field_4 >= 4) {
            if (task->state != 0) {
                Gp_UnlinkObj(&beam->obj);
            }
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->field_22 = (u16)work->field_22 + 1;
    switch (task->state) {
        case 0:
            beam = Mem_Calloc(sizeof(HyperBeam), 0);
            if (beam == NULL) {
                work->field_22 = 0;
                return;
            }
            task->exitCallback = WeaponsShared8011e4ac;
            player             = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            dstm               = (GpMtxWords*)&coord->coord;
            srcm               = (GpMtxWords*)&player->coord;
            dstm->w0           = srcm->w0;
            dstm->w1           = srcm->w1;
            dstm->w2           = srcm->w2;
            dstm->w3           = srcm->w3;
            dstm->h4           = srcm->h4;
            coord->flg         = 0;
            Gfx_ViewCoord.flg  = 0;
            Gp_UpdateCoord(coord);
            work->field_10 = 0;
            work->field_12 = 0;
            work->field_14 = 0x400;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&work->field_10);
            gte_rtv0_real();
            gte_stsv(&work->field_10);
            for (i = 0; i < 0x10; i++) {
                Gp_LcgState                 = Gp_LcgState * 5 + 0x71357911;
                D_hypervelocity_8012EF0C[i] = ((u32)Gp_LcgState >> 16) & 0xFF;
            }
            work->field_24     = 0xC0;
            work->field_26     = 0x500;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_28     = ((u32)Gp_LcgState >> 16) & 0xFFF;
            task->idMap        = (TaskIdMap*)beam;
            beam->obj.field_C  = beam->rec;
            beam->obj.field_1C = 0x800;
            beam->obj.field_8  = coord;
            beam->obj.field_18 = 0x2161A;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].field_0 = 2;
            beam->obj.flags     |= 0x8000;
            eff                  = Gp_SpawnEff(0x6000D, coord, 0, NULL);
            if (eff != NULL) {
                Task_Reparent(task, eff->field_0);
            }
            task->state       = 1;
            base->field_0     = 4;
            slot->field_58    = (work->field_20 << 9) + 0x200;
            slot->field_5C    = slot->field_58 * 16;
            ang               = Gp_LcgState * 5 + 0x71357911;
            slot->field_54    = ((ang >> 16) & 0x700) + 0x800;
            slot->field_50    = (u16)slot->field_54 >> 1;
            slot->field_52    = (s16)(u16)slot->field_54 >> 1;
            light->coord.t[0] = coord->coord.t[0];
            light->coord.t[1] = coord->coord.t[1];
            light->coord.t[2] = coord->coord.t[2];
            light->flg        = 0;
            rgb[0]            = (u16)work->field_24 >> 2;
            rgb[1]            = (u16)work->field_24 >> 2;
            rgb[2]            = (u16)work->field_24 >> 1;
            Gp_LcgState       = ang;
            func_hypervelocity_8011E494(coord, work->field_22, work->field_26, work->field_28);
            Gp_DrawRing(coord, work->field_26, rgb);
            return;
        case 1:
            Gp_UpdateCoord(coord);
            before.vx          = coord->workm.t[0];
            before.vy          = coord->workm.t[1];
            before.vz          = coord->workm.t[2];
            coord->coord.t[0] += work->field_10;
            coord->coord.t[1] += work->field_12;
            coord->coord.t[2] += work->field_14;
            coord->flg         = 0;
            Gfx_ViewCoord.flg  = 0;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            rgb[0]   = (u16)work->field_24 >> 2;
            rgb[1]   = (u16)work->field_24 >> 2;
            rgb[2]   = (u16)work->field_24 >> 1;
            func_hypervelocity_8011E494(coord, work->field_22, work->field_26, work->field_28);
            Gp_DrawRing(coord, work->field_26, rgb);
            func_hypervelocity_8011DF34(coord, work->field_22, work->field_26, 0);
            func_hypervelocity_8011DF34(coord, work->field_22, work->field_26, 1);
            if (Gp_State1C->field_6 != 0 && Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_hypervelocity_8011E8A0(&ground, work->field_26);
            }
            if (work->field_22 < 0x15) {
                Gp_SpawnEff(0x600E0, coord, 0x400, NULL);
                eff = Gp_SpawnEff(0x6000B, coord, 0, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->field_0);
                }
            }
            light->coord.t[0] = coord->coord.t[0];
            light->coord.t[1] = coord->coord.t[1];
            light->coord.t[2] = coord->coord.t[2];
            light->flg        = 0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            slot->field_54    = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            slot->field_50    = (u16)slot->field_54 >> 1;
            base->field_0     = 4;
            slot->field_52    = (s16)(u16)slot->field_54 >> 1;
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state = 2;
                return;
            }
            if (work->field_22 >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            Gp_UpdateCoord(coord);
            work->field_26 = (u16)work->field_26 - 0x40;
            rgb[0]         = (u16)work->field_24 >> 2;
            rgb[1]         = (u16)work->field_24 >> 2;
            rgb[2]         = (u16)work->field_24 >> 1;
            func_hypervelocity_8011E494(coord, work->field_22, work->field_26, work->field_28);
            Gp_DrawRing(coord, work->field_26, rgb);
            if (work->field_26 < 0x80) {
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_SpawnEff(0x600E0, coord, 0x400, NULL);
            return;
    }
}

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity", func_hypervelocity_8011DF34);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity", func_hypervelocity_8011E494);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity", func_hypervelocity_8011E8A0);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity", func_hypervelocity_8011EC1C);
