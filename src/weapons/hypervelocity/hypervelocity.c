#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
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

INCLUDE_RODATA("weapons/nonmatchings/hypervelocity/hypervelocity", D_hypervelocity_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity", func_hypervelocity_8011D1E8);

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
