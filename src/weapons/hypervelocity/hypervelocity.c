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

/// Translation of the round's own coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x240, 0x80)`.
SVECTOR D_hypervelocity_8011FB74 = { 0, 0x240, 0x80, 0 };

extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// `rtpt`. Likewise.
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

/// Teardown callback shared with the other weapon overlays; it unlinks
/// `Task::idMap` and releases the `Gp_State1C` work block.
void WeaponsShared8011e4ac(Task* task);

void func_hypervelocity_8011DF34(GsCOORDINATE2* coord, s16 age, s16 spin, s32 side);
void func_hypervelocity_8011E494(GsCOORDINATE2* coord, s16 age, s16 spin, s16 ang);
void func_hypervelocity_8011E8A0(GsCOORDINATE2* ground, s32 spin);

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

/// Draws one half of the hypervelocity round's trail: a 16-segment
/// semi-transparent tube hanging off `coord`, built in the scratchpad as a
/// `HyperTrailScratch`. `age` is the round's frame counter, `spin` its ring
/// radius and `side` picks which half - `side` non-zero gives the short, fat
/// half (rim 0x600, hub 0x80) trailing `spin * 2 + age * 256` behind, and
/// `side` zero the long, thin one (rim 0x800, hub 0x40) trailing
/// `spin + age * 16`. Each segment is a `POLY_FT4` from the six-frame strip at
/// tpage 0x2A, the frame picked per segment by the stored jitter
/// `D_hypervelocity_8012EF0C[i]` plus `age`, and is linked into the OT bucket
/// its own projected depth names. Segments the GTE flags as behind the eye are
/// dropped.
void func_hypervelocity_8011DF34(GsCOORDINATE2* coord, s16 age, s16 spin, s32 side)
{
    HyperTrailScratch* sc;
    POLY_FT4*          prim;
    SVECTOR*           vert;
    s32                rimRad;
    s32                hubRad;
    s32                rimSize;
    s32                hubSize;
    s32                i;
    s32                next;
    s32                ang;
    s32                u0;
    s16                back;
    MATRIX*            rot;

    /* `rimSize` / `hubSize` are latched into the loop's own `rimRad` /
       `hubRad` on purpose: the ROM keeps the two copies the single pair would
       have coalesced away, and `rot` is a second spelling of `&coord->workm`
       for the same reason. `vert` reaches `hub[i]` through `rim[i]` rather
       than off `sc`, so the `gte_ldv0` / `gte_stsv` address stays a register
       of its own instead of being shared with the field stores. */
    sc = (HyperTrailScratch*)(SCRATCH_SP -= sizeof(HyperTrailScratch));
    if (side != 0) {
        back    = (spin << 1) + (age << 8);
        hubSize = 0x80;
        rimSize = 0x600;
    } else {
        back    = spin + (age << 4);
        hubSize = 0x40;
        rimSize = 0x800;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    i      = 0;
    rimRad = rimSize;
    rot    = &coord->workm;
    hubRad = hubSize;
    for (; i < 0x10; i++) {
        ang           = i << 8;
        sc->rim[i].vx = (rsin(ang) * rimRad) >> 12;
        sc->rim[i].vy = (rcos(ang) * rimRad) >> 12;
        sc->rim[i].vz = -back;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->rim[i]);
        gte_rtv0_real();
        gte_stsv(&sc->rim[i]);
        sc->rim[i].vx += *(u16*)&coord->workm.t[0];
        sc->rim[i].vy += *(u16*)&coord->workm.t[1];
        sc->rim[i].vz += *(u16*)&coord->workm.t[2];
        sc->hub[i].vx  = (rsin(ang) * hubRad) >> 12;
        vert           = &sc->rim[i] + 16;
        vert->vy       = (rcos(ang) * hubRad) >> 12;
        vert->vz       = 0;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->hub[i]);
        gte_rtv0_real();
        gte_stsv(&sc->hub[i]);
        sc->hub[i].vx += *(u16*)&coord->workm.t[0];
        vert->vy      += *(u16*)&coord->workm.t[1];
        vert->vz      += *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 0x10; i++) {
        gte_ldv0(&sc->rim[i]);
        gte_rtps_real();
        gte_stsxy(&sc->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&sc->rim[next], &sc->hub[i], &sc->hub[next]);
        gte_rtpt_real();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz);
            sc->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            setRGB0(prim, 0x30, 0x30, 0x30);
            setSemiTrans(prim, 1);
            u0 = (s16)((D_hypervelocity_8012EF0C[i] + age) % 6) * 40;
            setUV4(prim, u0, 0x60, u0 + 0x27, 0x60, u0, 0x87, u0 + 0x27, 0x87);
            setXY4(prim, sc->sxy0.vx, sc->sxy0.vy, sc->sxy1.vx, sc->sxy1.vy, sc->sxy2.vx, sc->sxy2.vy, sc->sxy3.vx,
                   sc->sxy3.vy);
            addPrim((u_long*)(((((u32)sc->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
        }
    }
    SCRATCH_SP += sizeof(HyperTrailScratch);
}

/// Links the billboarded charge quad into `Gpu_CurrentOt`, dropped entirely if
/// the coordinate's origin fails its `RTPS` `FLAG` check. `coord` supplies the
/// world-space centre through `workm.t[]`, `age` picks between the two 0x38-wide
/// animation columns of the flare texture, `spin` is the half-extent in world
/// units (scaled by 55 and divided by the projected depth, so the quad keeps a
/// constant screen size) and `ang` is the roll: the corner pairs sit at `ang`
/// and `ang + 0x400`, a quarter turn apart, so the quad stays square as it
/// spins.
void func_hypervelocity_8011E494(GsCOORDINATE2* coord, s16 age, s16 spin, s16 ang)
{
    void**            scratch;
    u8*               head;
    HyperQuadScratch* block;
    POLY_FT4*         prim;
    SVECTOR*          vec;
    s32               u0;
    s32               u1;
    s32               col;
    s32               ang2;
    u16               vz;

    scratch                                                        = (void**)G_SCRATCH_HEAD;
    head                                                           = *scratch;
    ((HyperQuadScratch*)(head - sizeof(HyperQuadScratch)))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                                          = (HyperQuadScratch*)(head - sizeof(HyperQuadScratch));
    block->vec.vy                                                  = *(u16*)&coord->workm.t[1];
    vz                                                             = *(u16*)&coord->workm.t[2];
    *scratch                                                       = block;
    block->vec.vz                                                  = vz;
    vec                                                            = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((HyperQuadScratch*)(head - sizeof(HyperQuadScratch)))->sx);
    gte_stflg(&((HyperQuadScratch*)(head - sizeof(HyperQuadScratch)))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((HyperQuadScratch*)(head - sizeof(HyperQuadScratch)))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x29;
        setClut(prim, 0xB0, 0x10A);
        col = (age & 1) * 0x38;
        u0  = col + 0x70;
        u1  = col - 0x59;
        setUV4(prim, u0, 0xC8, u1, 0xC8, u0, 0xFF, u1, 0xFF);
        block->dx = (((spin * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((spin * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((spin * 55) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((spin * 55) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *scratch = (u8*)*scratch + sizeof(HyperQuadScratch);
}

/// Paints the round's scorch quad on the ground point `Gp_TraceGroundCoord`
/// found under the flare. `ground`'s `workm` translation is the traced point
/// and `spin` the quad's half-size: the unit quad `D_80111E38` is scaled by it
/// in the ground plane (Y stays 0), rotated by `Gfx_ViewWorldMtx` so the quad
/// lies flat in world space, and moved onto the ground point. One `RTPS` plus
/// one `RTPT` project the four corners, and the whole quad is dropped if the
/// first corner fails its `FLAG` check. The texture is the two-frame 0x28-page
/// strip at rows 0x38..0x57, the frame picked by the low bit of
/// `Display_State.field_8` so it flickers every other field.
///
/// `u` is latched before each pair of stores on purpose: writing the `POLY_FT4`
/// byte straight from the expression lets GCC fold the store's truncation back
/// into the `Display_State.field_8` load and the `+ 0xC0` / `+ 0xDF`, which the
/// ROM does not do.
void func_hypervelocity_8011E8A0(GsCOORDINATE2* ground, s32 spin)
{
    void**              scratch;
    register u8*        head asm("v1");
    HyperGroundScratch* sc;
    POLY_FT4*           prim;
    GpQuadCorner*       tbl;
    register SVECTOR*   v asm("a2");
    s32                 i;
    s32                 otz;
    s32                 flag;
    s32                 u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - sizeof(HyperGroundScratch);
    sc       = (HyperGroundScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = sc->vec;
    tbl = D_80111E38;
    do {
        v->vx = tbl->x * spin;
        v->vy = 0;
        v->vz = tbl->y * spin;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&ground->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&ground->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&ground->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps_real();
    gte_stsxy(&sc->sxy0);
    gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->r0    = 0x30;
        prim->g0    = 0x30;
        prim->b0    = 0x30;
        prim->tpage = 0x28;
        prim->clut  = 0x428B;
        u           = ((Display_State.field_8 & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((Display_State.field_8 & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = sc->sxy0.vx;
        prim->y0    = sc->sxy0.vy;
        prim->x1    = sc->sxy1.vx;
        prim->y1    = sc->sxy1.vy;
        prim->x2    = sc->sxy2.vx;
        prim->y2    = sc->sxy2.vy;
        prim->x3    = sc->sxy3.vx;
        prim->y3    = sc->sxy3.vy;
        addPrim((u_long*)(((((u32)otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(HyperGroundScratch);
}

/// Draws the discharge cone `func_hypervelocity_8011F270` leaves behind: two
/// opposed `POLY_FT4` walls flaring out of `coord`, built in the scratchpad as
/// a `HyperConeScratch`. The collar sits at y `0x700` and is `radius` wide,
/// the mouth rises to `0x600 - age * 256 / 2` and flares to
/// `radius + age * 128 + 0x200`, so the cone climbs and opens as the puff
/// ages; both rings are `radius` deep in z. Wall `i` is therefore the quad
/// `rim[i]`, `rim[i + 2]`, `hub[i]`, `hub[i + 2]` - the -x pair, then the +x
/// pair. Each wall is a frame of the same six-frame strip at tpage 0x2A the
/// trail uses, picked by the stored jitter `D_hypervelocity_8012EF0C[i]` plus
/// `age`, tinted by `rgb` and linked into the OT bucket its own projected
/// depth names. Walls the GTE flags as behind the eye are dropped.
void func_hypervelocity_8011EC1C(GsCOORDINATE2* coord, s16 age, s32 radius, u8* rgb)
{
    HyperConeScratch* sc;
    POLY_FT4*         prim;
    GpQuadCorner*     tbl;
    SVECTOR*          vert;
    MATRIX*           rot;
    s32               i;
    s32               rise;
    s32               top;
    u16               flare;
    s32               half;
    s32               u0;

    /* `rise` is built in two steps and then walked in place, and `half` is a
       second spelling of `radius`, because the ROM keeps both copies the
       folded forms would have coalesced away. `flare` is 16-bit on purpose:
       it only ever feeds a halfword store, and widening it moves the whole
       prologue's register assignment. `vert` reaches `hub[i]` through
       `rim[i]` so the `gte_ldv0` / `gte_stsv` address stays a register of its
       own instead of being shared with the field stores. */
    sc    = (HyperConeScratch*)(SCRATCH_SP -= sizeof(HyperConeScratch));
    rise  = age;
    rise  = rise << 7;
    top   = 0x600 - rise;
    rise  = rise + 0x200;
    flare = radius + rise;
    half  = radius;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    rot = &coord->workm;
    tbl = D_80111E38;
    do {
        sc->rim[i].vx = tbl[i].x * flare;
        sc->rim[i].vy = top;
        sc->rim[i].vz = tbl[i].y * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->rim[i]);
        gte_rtv0_real();
        gte_stsv(&sc->rim[i]);
        *(u16*)&sc->rim[i].vx = *(u16*)&sc->rim[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sc->rim[i].vy = *(u16*)&sc->rim[i].vy + *(u16*)&coord->workm.t[1];
        *(u16*)&sc->rim[i].vz = *(u16*)&sc->rim[i].vz + *(u16*)&coord->workm.t[2];
        vert                  = &sc->rim[i] + 4;
        vert->vx              = tbl[i].x * radius;
        vert->vy              = 0x700;
        vert->vz              = tbl[i].y * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->hub[i]);
        gte_rtv0_real();
        gte_stsv(&sc->hub[i]);
        *(u16*)&vert->vx = *(u16*)&vert->vx + *(u16*)&coord->workm.t[0];
        i++;
        *(u16*)&vert->vy = *(u16*)&vert->vy + *(u16*)&coord->workm.t[1];
        *(u16*)&vert->vz = *(u16*)&vert->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        gte_ldv0(&sc->rim[i]);
        gte_rtps_real();
        gte_stsxy(&sc->sxy0);
        gte_ldv3(&sc->rim[i + 2], &sc->hub[i], &sc->hub[i + 2]);
        gte_rtpt_real();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz);
            sc->otz++;
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u0          = (s16)((D_hypervelocity_8012EF0C[i] + age) % 6) * 40;
            prim->u0    = u0;
            prim->v0    = 0x60;
            prim->u1    = u0 + 0x27;
            prim->v1    = 0x60;
            prim->u2    = u0;
            prim->u3    = u0 + 0x27;
            prim->v2    = 0x87;
            prim->v3    = 0x87;
            setXY4(prim, sc->sxy0.vx, sc->sxy0.vy, sc->sxy1.vx, sc->sxy1.vy, sc->sxy2.vx, sc->sxy2.vy, sc->sxy3.vx,
                   sc->sxy3.vy);
            addPrim((u_long*)(((((u32)sc->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
        }
        i++;
    } while (i < 2);
    SCRATCH_SP += sizeof(HyperConeScratch);
}
