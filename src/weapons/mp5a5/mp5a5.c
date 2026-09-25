#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "weapons/mp5a5.h"
#include "weapons/weapon.h"
#include "weapons/weapons_shared_8011d468.h"
#include "weapons/weapons_shared_8011d864.h"

void func_mp5a5_8011D468(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_mp5a5_8011D864(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

/// The weapon's index: 0x1E for the MP5A5, 0x1F and 0x20 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sounds, the shot id and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

/// Muzzle offset of the weapon, in the firing hand's coordinate frame.
SVECTOR D_mp5a5_8011E128 = { 0, 0x240, 0x40, 0 };

/// Per-frame muzzle-flash task for the MP5A5 and its upgrades. Frame 0 claims room-coord slot 0
/// as a white 0x1000 light at the weapon's world position, parks the task's own
/// coordinate on the muzzle offset under the hand frame, and rolls the flash
/// size (`scale`), its spin (`angle`) and the four quad angles; every
/// later frame just halves the size and the brightness. Each frame then draws
/// the core (`func_mp5a5_8011D468`), a full-screen fade at the current
/// brightness and the four flash quads, decays the light's range by 0x190 and
/// releases the pool block after seven frames. Nothing runs at all once
/// `Gp_State1C` is fading out (`field_4 >= 2`).
void func_mp5a5_8011D1E0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpPointLight*  slot;
    u8             rgb[3];
    s32            i;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    base  = &Gp_RoomCoords[0];
    slot  = &base->data.light;

    if (Gp_State1C->eventState >= 2) {
        return;
    }

    work->age++;
    switch (task->state) {
        case 0:
            slot->head.u.coord.coord.t[0] = coord->coord.t[0];
            slot->head.u.coord.coord.t[1] = coord->coord.t[1];
            slot->head.u.coord.coord.t[2] = coord->coord.t[2];
            base->data.coord.flg          = 0;
            slot->head.r                  = 0x1000;
            slot->head.g                  = 0x1000;
            slot->head.b                  = 0x1000;
            slot->inner                   = 0xFA0;
            slot->outer                   = 0x12C0;
            base->framesLeft              = 4;

            coord->sub        = work->parent;
            coord->coord.t[0] = D_mp5a5_8011E128.vx;
            coord->coord.t[1] = D_mp5a5_8011E128.vy;
            coord->coord.t[2] = D_mp5a5_8011E128.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);

            work->period = 0xC0;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale  = (((u32)Gp_LcgState >> 16) & 0x3FF) + 0x600;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->angle  = ((u32)Gp_LcgState >> 16) & 0xFFF;
            task->state  = 1;
            for (i = 0; i < 4; i++) {
                Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
                D_mp5a5_8012B508[i] = ((i & 3) << 10) + (((u32)Gp_LcgState >> 16) & 0x3FF);
            }
            break;
        case 1:
            /* The `(u16)` casts are load-shape, not arithmetic: the ROM reads
               both fields with `lhu` and sign-extends in the shift pair
               (`sll 16` / `sra 17`). A plain `>>= 1` on the `s16` field emits
               `lh` / `sra 1` instead. */
            work->scale  = work->scale >> 1;
            work->period = work->period >> 1;
            break;
    }

    func_mp5a5_8011D468(coord, work->scale, work->angle);
    /* Chained on purpose: it is one `lbu` stored three times, in reverse index
       order. Three separate assignments reload the field each time, because the
       stores into `rgb` may alias it. */
    rgb[0] = rgb[1] = rgb[2] = work->period;
    Gp_DrawFadeQuad(rgb, 1);
    for (i = 0; i < 4; i++) {
        func_mp5a5_8011D864(coord, D_mp5a5_8012B508[i], work->period);
    }
    if (slot->inner >= 0x191) {
        slot->inner -= 0x190;
    }
    if (work->age >= 7) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws the core of a gun's muzzle flash: one semi-transparent, shade-blended
/// `POLY_FT4` billboarded on `arg0`'s world position. `arg1` is the flash size
/// (scaled down by the projected depth) and `arg2` its spin, so the quad is a
/// square rotated by `arg2` rather than an axis-aligned sprite.
/* `otzp` is a second name for the same block on purpose: `gte_stszotz` takes
   its address in a register of its own, so the ROM keeps a `move` the single
   pointer would have coalesced away. The `gte_ldv0` / `gte_stsxy` addresses
   and every `otz` reload are spelled out from `head` for the same reason -
   off `blk` they would reuse the block register instead. */

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */

void func_mp5a5_8011D468(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    OverlaySpriteScratch* blk;
    OverlaySpriteScratch* otzp;
    POLY_FT4*             prim;
    s32                   ang;
    u16                   vz;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (OverlaySpriteScratch*)(head - sizeof(OverlaySpriteScratch));
    blk->vec.vx = *(u16*)&arg0->workm.t[0];
    blk->vec.vy = *(u16*)&arg0->workm.t[1];
    vz          = *(u16*)&arg0->workm.t[2];
    otzp        = blk;
    *scratch    = blk;
    blk->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlaySpriteScratch*)(head - 0x18))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((OverlaySpriteScratch*)(head - 0x18))->sxy);
    gte_stszotz(&otzp->otz);
    if (((OverlaySpriteScratch*)(head - 0x18))->otz >= 0x11) {
        ang         = arg2;
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        setcode(prim, getcode(prim) | 3);
        blk->dx  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x0 = *(u16*)&blk->sxy.vx + *(u16*)&blk->dx;
        prim->x3 = *(u16*)&blk->sxy.vx - *(u16*)&blk->dx;
        prim->y0 = *(u16*)&blk->sxy.vy - *(u16*)&blk->dy;
        ang      = ang + 0x400;
        prim->y3 = *(u16*)&blk->sxy.vy + *(u16*)&blk->dy;
        blk->dx  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg1 * 55) / ((OverlaySpriteScratch*)(head - 0x18))->otz) * rcos(ang)) >> 12;
        prim->x1 = *(u16*)&blk->sxy.vx + *(u16*)&blk->dx;
        prim->x2 = *(u16*)&blk->sxy.vx - *(u16*)&blk->dx;
        prim->y1 = *(u16*)&blk->sxy.vy - *(u16*)&blk->dy;
        prim->y2 = *(u16*)&blk->sxy.vy + *(u16*)&blk->dy;
        addPrim((u_long*)(((((u32)((OverlaySpriteScratch*)(head - 0x18))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + sizeof(OverlaySpriteScratch);
}

/// Draws a gun's muzzle flash as one Gouraud quad: three corners on a 0x100
/// circle around `arg1` (at `-0xC0`, `0`, `+0xC0`) and one tip 0x600 out and
/// 0x200 towards the camera, all in the muzzle coordinate's frame. `arg2` is
/// the flash brightness; only the corner along `arg1` is lit, with half of
/// `arg2` in red and green and all of it in blue.
void func_mp5a5_8011D864(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    WeaponQuadScratch* blk;
    POLY_G4*           prim;
    MATRIX*            wm;
    s32                ang;
    s32                back;
    s32                len;
    s32                depth;

    /* `len` and `depth` are locals rather than literals on purpose: as
       constants GCC turns the `* 0x600` into a shift-and-add and drops the
       `mult` the ROM keeps. */
    depth    = -0x200;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (WeaponQuadScratch*)(head - sizeof(WeaponQuadScratch));
    *scratch = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    ang = arg1;

    /* Corner 0: on the small circle, 0xC0 behind the flash direction. */
    back         = ang - 0xC0;
    blk->v[0].vx = (u32)rsin(back) >> 4;
    blk->v[0].vy = (u32)rcos(back) >> 4;
    blk->v[0].vz = 0;
    wm           = &arg0->workm;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    gte_rtv0();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    *(u16*)&blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 1: the far tip, a full 0x600 out and 0x200 towards the camera. */
    len          = 0x600;
    blk->v[1].vx = (rsin(ang) * len) >> 12;
    blk->v[1].vy = (rcos(ang) * len) >> 12;
    blk->v[1].vz = depth;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[1]);
    gte_rtv0();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[1]);
    *(u16*)&blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 2: on the small circle, straight along the flash direction. This
       is the only lit corner. */
    blk->v[2].vx = (u32)rsin(ang) >> 4;
    blk->v[2].vy = (u32)rcos(ang) >> 4;
    blk->v[2].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[2]);
    gte_rtv0();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[2]);
    *(u16*)&blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&arg0->workm.t[0];
    ang                  = ang + 0xC0;
    *(u16*)&blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&arg0->workm.t[2];

    /* Corner 3: on the small circle, 0xC0 ahead of the flash direction. */
    blk->v[3].vx = (u32)rsin(ang) >> 4;
    blk->v[3].vy = (u32)rcos(ang) >> 4;
    blk->v[3].vz = 0;
    gte_SetRotMatrix(wm);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[3]);
    gte_rtv0();
    gte_stsv(&((WeaponQuadScratch*)(head - 0x24))->v[3]);
    *(u16*)&blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((WeaponQuadScratch*)(head - 0x24))->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&((WeaponQuadScratch*)(head - 0x24))->v[1], &((WeaponQuadScratch*)(head - 0x24))->v[2],
             &((WeaponQuadScratch*)(head - 0x24))->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (((WeaponQuadScratch*)(head - 0x24))->otz >= 0x11) {
        setRGB0(prim, 0, 0, 0);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, arg2 >> 1, arg2 >> 1, arg2);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)(((((u32)((WeaponQuadScratch*)(head - 0x24))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((WeaponQuadScratch*)(head - 0x24))->otz);
    }
    *scratch = (u8*)*scratch + sizeof(WeaponQuadScratch);
}

/// Per-frame firing state machine for the MP5A5 and its upgrades. State 0 arms the shot and
/// starts the raise animation (clip 5 instead of 1 when the weapon was already
/// up), state 1 waits for that clip, and states 2/3 count `field_934` down to
/// the frame the round leaves the barrel. That frame branches on `field_97F`:
/// single fire (`== 1`) spends one round, plays sound 4 of the weapon's bank, spawns the plain
/// muzzle flash and runs the recoil clip, while burst fire spends 0x101, plays
/// sound 5, holds the pose for 0x12 frames and reparents the longer flash
/// effect under the weapon task. States 4/5 pick the lock-on target once (only
/// while still below 6) and state 6 loops back to `fire` while the trigger is
/// held, the ammo check passes and the burst timer has run out.
void func_mp5a5_8011DDA4(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    GpActorD4Rec*  rec;
    GpEffWork*     eff;
    s32            anim;

    /* Pinned to `$v0`: the scratch block's address is stored back to
       `G_SCRATCH_HEAD` from `$v0` and copied into the callee-saved `spot`,
       so the two uses must not be coalesced into one register. */
    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - 0x50;
        spot                    = (GsCOORDINATE2*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    actor = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (actor->field_95E) {
        case 0:
            actor->field_956  = 4;
            actor->field_95A  = 2;
            actor->field_954  = 0;
            actor->field_95C  = 0;
            actor->field_934  = 1;
            actor->field_95E += 1;
            actor->field_12A |= 0x400;
            anim              = 1;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 5;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
        fire:
            actor->field_95E = 3;
            actor->field_981 = 0;
            /* fallthrough */
        case 3:
            if (--actor->field_934 == 0) {
                rec = &actor->field_14C;
                if (actor->field_97F == 1) {
                    actor->field_95E  = 4;
                    actor->field_934  = 3;
                    actor->field_940  = 0;
                    actor->field_124  = Player_Status.weaponSlotItem | 0x20000 | (WEAPON_ID << 8);
                    rec->end0Radius   = rec->end1Radius;
                    actor->field_12A |= 0x800;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx(((TmdObject*)arg0->extra)->coords, 0x20000004 | (WEAPON_ID << 16), 1);
                    Gp_SpawnEff(0x6002B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->coords,
                                WEAPON_ID, NULL);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                } else {
                    actor->field_95E  = 5;
                    actor->field_940  = 0x12;
                    actor->field_934  = 0x12;
                    actor->field_124  = 0x20016 | (WEAPON_ID << 8);
                    rec->end0Radius   = 0xC00;
                    actor->field_12A &= 0xF7FF;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx(((TmdObject*)arg0->extra)->coords, 0x20000005 | (WEAPON_ID << 16), 0);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 0x101);
                    eff = Gp_SpawnEff(0x60041,
                                      (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->coords,
                                      WEAPON_ID, NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->field_91C, eff->task);
                    }
                }
                actor->field_12A |= 0xC000;
            }
            break;
        case 4:
        case 5:
            if (actor->field_95E < 5 && Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            actor->field_95E  = 6;
            actor->field_12A &= 0x3FFF;
            /* fallthrough */
        case 6:
            if ((s8)func_801060E0(arg0) == 1 && func_80106264(1) > 0 && actor->field_940 == 0) {
                goto fire;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}
