#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "weapons/m4a1_grenade.h"

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

/// Equipped-weapon index; `Gp_GetItemSlot(D_80073BA9 + 0x7F)` is the slot the
/// player is holding, and its `field_2` is the attachment id the sound bank is
/// keyed on. A main-executable global with no module header yet.
extern u8 D_80073BA9;

/// Impact clip id per attachment, indexed by `sfx - 0xA`: 0x1F4, 0x4B0, 0x7D0.
extern u16 D_m4a1_grenade_8012E08C[];

void WeaponsShared8011de24(Task* task);

/// Per-frame firing state machine for the M4A1 grenade launcher. State 0 arms
/// the shot and raises the weapon (clip 8 instead of 1 when it was already up),
/// state 1 waits for that clip. State 2 branches on `field_97F`: a held trigger
/// (bit 0) drops into the three-round burst of state 3, a tap (bit 1) fires the
/// single 0x101 grenade of state 4, and anything else falls straight into the
/// burst. State 3 counts `field_934` down to each round, spending one grenade,
/// playing `0x201B0004` and spawning the muzzle flash, and picks the lock-on
/// target on the frame after. States 4/5 pick the target once, then state 5
/// walks the animation, emitting `0x201B0008 + field_93E` on every record whose
/// `field_3` has both 0x10 and 0x20, and hands back to `func_80106550` when the
/// clip is done or the recoil timer has run out.
void func_m4a1_grenade_8011D1EC(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* spot;
    GpAnimRec*     rec;
    GpItemSlot*    slot;
    s32            anim;
    s32            delay;
    s32            sfx;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    slot  = Gp_GetItemSlot(D_80073BA9 + 0x7F);
    /* Reloaded rather than reused: the store leaves the block address in a
       caller-saved register and the copy into `spot` is a second read of
       `G_SCRATCH_HEAD` that CSE folds back onto it, which is what keeps the
       two uses in separate registers. */
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 0x50;
    spot                    = (GsCOORDINATE2*)*(void**)G_SCRATCH_HEAD;
    sfx                     = slot->field_2 - 0x9F;
    if (sfx < 0) {
        sfx = 0xA;
    }
    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95A  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            actor->field_12A |= 0xC00;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_981 = 0;
            if (actor->field_97F & 1) {
                actor->field_95E = 3;
                actor->field_934 = 0;
                actor->field_979 = 9;
                actor->field_93E = 3;
                func_80106238(arg0, 0, 1);
            } else if (actor->field_97F & 2) {
                actor->field_95E = 4;
                actor->field_940 = 0x28;
                actor->field_979 = 0x22;
                Gp_ConsumeSlotQty(0x9A, 0x101);
                Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                              ((sfx - 0xA) << 24) | 0x201B0006, 1);
                Gp_SpawnEff(0x6006C,
                            (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8, 0x1B,
                            NULL);
                func_80104490(arg0, 0, 0, sfx | 0x1B00);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 3);
                break;
            }
            /* fallthrough */
        case 3:
            if (actor->field_93E != 0) {
                delay = actor->field_934;
                if (delay == 0) {
                    actor->field_93E--;
                    actor->field_934  = 3;
                    actor->field_981  = 0;
                    actor->field_12A |= 0xC000;
                    Gp_ConsumeSlotQty(0x9A, 1);
                    if (func_80106264(1) == 0) {
                        actor->field_93E = 0;
                    }
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                                  ((sfx - 0xA) << 24) | 0x201B0004, 1);
                    Gp_SpawnEff(0x6006B,
                                (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->field_8,
                                0x1B, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                } else {
                    actor->field_934 = delay - 1;
                    if (delay - 1 == 2) {
                        actor->field_12A &= 0x3FFF;
                        if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                            Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->field_95E  = 5;
            actor->field_93E  = 0;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx((GpObj38*)spot, 0x17, 1);
            }
            /* fallthrough */
        case 5:
            rec = Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->field_438 + 1);
            if (rec != NULL && rec != actor->field_92C) {
                actor->field_92C = rec;
                if ((rec->field_3 & 0x30) == 0x30) {
                    Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                                  (actor->field_93E + 0x201B0008) | ((sfx - 0xA) << 24), 0);
                    actor->field_93E++;
                }
            }
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

/// Spawn state: allocates the grenade's work block, places the projectile a
/// little above and in front of the muzzle coordinate, parents it to world,
/// and links its two collision nodes.
void func_m4a1_grenade_8011D654(Task* arg0)
{
    void**           scratch;
    u8*              head;
    SVECTOR*         blk;
    SVECTOR*         vec;
    MATRIX*          mtx;
    TmdObject*       extra;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   muzzle;
    M4a1GrenadeWork* work;

    scratch  = (void**)G_SCRATCH_HEAD;
    extra    = arg0->extra;
    head     = *scratch;
    coord    = extra->field_8;
    blk      = (SVECTOR*)(head - 0x28);
    *scratch = blk;
    muzzle   = coord->sub;
    work     = Mem_Calloc(sizeof(M4a1GrenadeWork), 0);
    vec      = blk;
    if (work == NULL) {
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
        Task_Kill(arg0);
        return;
    }
    arg0->idMap        = (TaskIdMap*)work;
    arg0->exitCallback = WeaponsShared8011de24;
    arg0->state++;
    Mem_Set(work, 0, sizeof(M4a1GrenadeWork));
    blk->vx     = 0;
    blk->vy     = 0x220;
    blk->vz     = 0x28;
    muzzle->flg = 0;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr_real();
    gte_stlvnl(coord->workm.t);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coord->workm, &coord->coord);
    mtx            = (MATRIX*)(head - 0x20);
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    extra->field_C = 0;
    *mtx           = coord->coord;
    Gfx_RotMatrixX(mtx, -0x400, 0);
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    work->field_88.w   = 0xA0000;
    work->field_8C     = 1;
    work->field_90     = 0;
    work->obj.field_8  = coord;
    work->obj.field_C  = work->rec0;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.field_18 = (u16)arg0->spawnArg1 | 0x20000;
    work->obj.field_1C = 0x94;
    work->obj.flags    = 1;
    Gp_LinkObj(1, &work->obj);
    Gp_InitRec18Table(work->obj.field_C, 1, 0);
    work->obj2.field_C   = (GpRec18*)&work->d4rec;
    work->obj2.flags     = 3;
    work->d4rec.field_14 = work->rec1;
    work->obj2.field_8   = coord;
    work->obj2.field_10  = 0;
    work->obj2.field_12  = 0;
    work->obj2.field_14  = 0;
    work->obj2.field_18  = 0;
    work->obj2.field_1C  = 0;
    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->obj.flags     |= 0xC400;
    work->d4rec.field_A  = -(work->field_88.w >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.field_14, 1, 0);
    work->obj2.flags       |= 0x4400;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}

/// Flight state: steps the grenade along `dir`, and detonates when it hits
/// something, when it crosses a room record that blocks it, or when the 16.16
/// flight timer runs past 0xFFFFF. `rec0` collects the solid hits, `rec1` the
/// room-boundary ones; whichever table has a `0x100000` record is handed to
/// `func_800E0FEC` / `func_800E1ACC` to name the room parameter it crossed.
/// A record with `field_1` set is a doorway, which only detonates the grenade
/// in the one scripted case (room 0x14, floors 2 and 3); one with `field_1`
/// clear detonates on `field_2` and otherwise hands the task to state 3.
void func_m4a1_grenade_8011D994(Task* arg0)
{
    M4a1GrenadeScratch* blk;
    M4a1GrenadeWork*    work;
    GsCOORDINATE2*      coord;
    GpItemSlot*         slot;
    GpRec18*            rec;
    GpRoomParamRec*     param;
    u8*                 head;
    s32                 idx;
    s32                 count;
    s32                 clip;
    s32                 step;
    s32                 sfxbase;
    s32                 sfxarg;

    work  = (M4a1GrenadeWork*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    slot  = Gp_GetItemSlot(D_80073BA9 + 0x7F);
    head  = *(u8**)G_SCRATCH_HEAD;
    /* Pushed and then re-derived rather than stored from `blk`: the scratch
       head has to stay live in its own register, because the `GpDeltaScratch`
       handed to `func_800E0FEC` below is addressed off it and not off `blk`. */
    *(void**)G_SCRATCH_HEAD = head - sizeof(M4a1GrenadeScratch);
    blk                     = (M4a1GrenadeScratch*)(head - sizeof(M4a1GrenadeScratch));
    coord->flg              = 0;
    if (Gp_CountRec18Hi(work->rec0, 0x30000) != 0) {
    explode:
        blk->sfx = slot->field_2 - 0x9F;
        if (blk->sfx < 0) {
            blk->sfx = 0xA;
        }
        arg0->state = 2;
        Gp_SpawnEff(0x60071, coord, blk->sfx, NULL);
        sfxbase = D_80073BA9 << 16;
        sfxarg  = ((blk->sfx - 0xA) << 24) | 0x20000007;
        Gp_PlayObjSfx((GpObj38*)coord, sfxbase | sfxarg, 1);
        clip = 8;
        if (blk->sfx == 0xB) {
            clip = 1;
        }
        work->field_88.w        = clip;
        work->obj.flags        &= 0xBFFF;
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(M4a1GrenadeScratch);
        work->obj.field_1C      = D_m4a1_grenade_8012E08C[blk->sfx - 0xA];
        return;
    }

    /* `rec` is picked after each count, not before: assigning it first would
       make it cross the call and cost a call-saved register. */
    count = Gp_CountRec18Hi(work->rec1, 0x100000);
    rec   = work->rec1;
    if (count == 0) {
        goto try_rec0;
    }
check:
    /* `head - 0x14` is `&blk->delta`; spelling it off `head` is what keeps the
       two scratch pointers apart, and the extra reference is what wins `head`
       its call-saved register. */
    SOFT_USE_REG(head);
    func_800E0FEC(rec, (GpDeltaScratch*)(head - 0x14), 1, &idx);
    idx = func_800E1ACC((u8*)&idx);
    /* `func_800E1ACC` writes through `&idx` as well as returning it, so the
       index is re-read from the slot instead of kept in the return register. */
    SOFT_COMPILER_BARRIER();
    param = Gp_RoomParamTables[Game_Session->field_7 - 1][Game_Session->field_6 - 1][idx];
    if (param->field_1 == 0) {
        if (param->field_2 != 0) {
            goto explode;
        }
        arg0->state = 3;
        goto move;
    }
    if (idx == 1 && Mc_SaveData.field_6 == 0x14 && (u32)(Mc_SaveData.field_7 - 2) < 2U) {
        goto explode;
    }
    goto move;
try_rec0:
    count = Gp_CountRec18Hi(work->rec0, 0x100000);
    rec   = work->rec0;
    if (count != 0) {
        goto check;
    }
move:
    blk->delta.vx.w     = work->dir.vx / work->field_88.h.hi;
    blk->delta.vy.w     = work->dir.vy / work->field_88.h.hi;
    blk->delta.vz.w     = work->dir.vz / work->field_88.h.hi;
    coord->coord.t[0]  += blk->delta.vx.w;
    coord->coord.t[1]  += blk->delta.vy.w;
    coord->coord.t[2]  += blk->delta.vz.w;
    work->d4rec.field_A = -(work->field_88.w >> 10);
    work->field_88.w   += 0x1800;
    if (work->field_88.w > 0xFFFFF) {
        goto explode;
    }
    work->dir.vy   = work->dir.vy + 0x10;
    step           = work->field_90 + 1;
    work->field_90 = step;
    if (work->field_8C < 4 && step % 7 == 0) {
        work->field_8C = work->field_8C + 1;
    }
    if (work->field_90 % work->field_8C == 0) {
        Gp_SpawnEff(0x60070, coord, 0, NULL);
    }
    Gp_ClearRec18Occupied(work->rec0);
    Gp_ClearRec18Occupied(work->rec1);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(M4a1GrenadeScratch);
}
