#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "weapons/grenade_pistol.h"
#include "weapons/m4a1_grenade.h"
#include "weapons/weapon.h"

void func_grenade_pistol_8011DB8C(Task* task);

/// Which weapon this build is: 0 for the Grenade Pistol, 1 for the MM1. The two
/// packages are this source built once each, and each declares its variant in
/// the manifest. Both carry the other's row of the per-projectile tables.
#ifndef GRENADE_VARIANT
#error "GRENADE_VARIANT is a per-package build parameter"
#endif

/// The weapon's index. It also keys the firing sound and the shot effect.
#define GRENADE_WEAPON (0xB + GRENADE_VARIANT)

void func_grenade_pistol_8011D1D4(GpActorWork* arg0)
{
    GameActor* actor;
    s32        anim;

    actor = arg0->actor;
    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95A  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
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
            actor->field_95E = 3;
            actor->field_981 = 0;
            actor->field_940 = 0x28;
            Gp_PlayObjSfx(arg0->extra->coords,
                          ((Player_Status.weaponSlotItem - 0xA) << 24) | 0x20000004 | (GRENADE_WEAPON << 16), 1);
            Gp_SpawnEff(0x6006C,
                        (GsCOORDINATE2*)((TmdObject*)actor->field_91C->extra)->coords, GRENADE_WEAPON,
                        NULL);
            Gp_ConsumeSlotQty(WEAPON_ITEM(GRENADE_WEAPON), 1);
            /* The projectile's kind and its row of the muzzle-offset and speed tables
               (bits 16-19 of its spawn argument) both follow the variant. */
            func_80104490(arg0, 0, 1 + GRENADE_VARIANT,
                          Player_Status.weaponSlotItem | (GRENADE_VARIANT << 16) | (GRENADE_WEAPON << 8));
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 3);
            break;
        case 3:
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
}

/// Spawn state of the projectile: allocates the 0xA0 `M4a1GrenadeWork` block,
/// places the projectile at the per-ammo muzzle offset from `D_grenade_pistol_8012B420`, parents it
/// to the world coordinate, sets its launch speed from `D_grenade_pistol_8012B438` and links its two
/// collision nodes. The Grenade Pistol and MM1 share it by being one source.
void func_grenade_pistol_8011D3A0(Task* arg0)
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
    s32              idx;
    s32              flags;
    s32              speed;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (SVECTOR*)(head - 8);
    *scratch = blk;
    extra    = arg0->extra;
    idx      = ((u32)arg0->spawnArg1 >> 16) & 0xF;
    coord    = extra->coords;
    muzzle   = coord->sub;
    work     = memCalloc(sizeof(M4a1GrenadeWork), 0);
    vec      = blk;
    if (work == NULL) {
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
        taskKill(arg0);
        return;
    }
    arg0->work         = (TaskIdMap*)work;
    arg0->exitCallback = func_grenade_pistol_8011DB8C;
    arg0->state++;
    Mem_Set(work, 0, sizeof(M4a1GrenadeWork));
    blk->vx     = D_grenade_pistol_8012B420[idx].vx;
    blk->vy     = D_grenade_pistol_8012B420[idx].vy;
    blk->vz     = D_grenade_pistol_8012B420[idx].vz;
    muzzle->flg = 0;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr();
    gte_stlvnl(coord->workm.t);
    mtx = &coord->coord;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, mtx);
    coord->sub   = &gGfxViewCoord;
    coord->flg   = 0;
    extra->flags = 0;
    Gfx_RotMatrixX(mtx, -0x400, 0);
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    speed              = D_grenade_pistol_8012B438[idx];
    work->field_8C     = 1;
    work->field_90     = 0;
    work->obj.coord    = coord;
    work->obj.ctx.recs = work->rec0;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->field_88.w   = speed << 16;
    flags              = (u16)arg0->spawnArg1 | 0x20000;
    work->obj.key      = flags;
    if (arg0->spawnArg1 & 0x100000) {
        work->obj.key = flags | 0x80;
    }
    work->obj.radius = 0x94;
    work->obj.flags  = 1;
    Gp_LinkObj(1, &work->obj);
    Gp_InitRec18Table(work->obj.ctx.recs, 1, 0);
    work->obj2.ctx.d4rec   = &work->d4rec;
    work->obj2.flags       = 3;
    work->d4rec.recs       = work->rec1;
    work->obj2.coord       = coord;
    work->obj2.pos.vx      = 0;
    work->obj2.pos.vy      = 0;
    work->obj2.pos.vz      = 0;
    work->obj2.key         = 0;
    work->obj2.radius      = 0;
    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = 0;
    work->d4rec.end0Radius = 1;
    work->d4rec.end1Radius = 1;
    work->obj.flags       |= 0xC400;
    work->d4rec.end1.vz    = -(work->field_88.w >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.recs, 1, 0);
    work->obj2.flags       |= 0x4400;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

/// Flight state of the projectile. Detonates when the shot
/// has touched world geometry (`rec0` with 0x30000), when a wall record it hit
/// is solid, or when the flight timer runs past 0xDFFFF; otherwise it steps
/// the projectile by `dir / field_88.h.hi`, lets gravity pull `dir.vy` down,
/// and trails smoke every `field_8C` frames — a divisor that grows by one
/// every seven frames up to four, so the trail thins as the grenade slows.
///
/// The shot is fired through `Task::spawnArg1`: its low byte is the
/// attachment id driving the explosion effect, the byte above it seeds the
/// sound bank, and bit 0x100000 marks the shot that plays the fixed
/// `0x40660002` clip instead.
void func_grenade_pistol_8011D6FC(Task* arg0)
{
    GrenadePistolScratch* blk;
    M4a1GrenadeWork*      work;
    GsCOORDINATE2*        coord;
    GpRec18*              rec;
    GpRoomParamRec*       param;
    u8*                   head;
    s32                   idx;
    s32                   count;
    s32                   clip;
    s32                   step;
    s32                   sfxarg;
    s32                   sfxbase;

    work  = (M4a1GrenadeWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    head  = *(u8**)G_SCRATCH_HEAD;
    /* Pushed and then re-derived rather than stored from `blk`: the scratch
       head has to stay live in its own register, because the `GpDeltaScratch`
       handed to `func_800E0FEC` below is addressed off it and not off `blk`. */
    *(void**)G_SCRATCH_HEAD = head - sizeof(GrenadePistolScratch);
    blk                     = (GrenadePistolScratch*)(head - sizeof(GrenadePistolScratch));
    coord->flg              = 0;
    if (Gp_CountRec18Hi(work->rec0, 0x30000) != 0) {
    explode:
        blk->field_30 = arg0->spawnArg1 & 0xFF00;
        blk->sfx      = (u8)arg0->spawnArg1;
        arg0->state   = 2;
        Gp_SpawnEff(0x60071, coord, blk->sfx, NULL);
        /* Two calls, not one call on a selected argument: the identical tails
           are what cross-jumping merges into a single `jal` with an unfilled
           delay slot. */
        if (arg0->spawnArg1 & 0x100000) {
            Gp_PlayObjSfx(coord, 0x40660002, 1);
        } else {
            sfxbase = blk->field_30 << 8;
            sfxarg  = ((blk->sfx - 0xA) << 24) | 0x20000005;
            Gp_PlayObjSfx(coord, sfxbase | sfxarg, 1);
        }
        clip = 8;
        if (blk->sfx == 0xB) {
            clip = 1;
        }
        work->field_88.w        = clip;
        work->obj.flags        &= 0xBFFF;
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GrenadePistolScratch);
        work->obj.radius        = D_grenade_pistol_8012B430[blk->sfx - 0xA];
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
    /* `head - 0x18` is `&blk->delta`; spelling it off `head` is what keeps the
       two scratch pointers apart, and the reference count is what wins `head`
       the lower of the two call-saved registers. */
    SOFT_USE_REG2(head, head);
    func_800E0FEC(rec, (GpDeltaScratch*)(head - 0x18), 1, &idx);
    idx = func_800E1ACC((u8*)&idx);
    /* `func_800E1ACC` writes through `&idx` as well as returning it, so the
       index is re-read from the slot instead of kept in the return register. */
    SOFT_COMPILER_BARRIER();
    param = Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1][idx];
    if (param->field_1 == 0) {
        if (param->field_2 != 0) {
            goto explode;
        }
        arg0->state = 3;
        goto move;
    }
    if (idx == 1 && Mc_SaveData.at4.loc.area == 0x14 && (u32)(Mc_SaveData.at4.loc.stage - 2) < 2U) {
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
    work->d4rec.end1.vz = -(work->field_88.w >> 9);
    work->field_88.w   += 0x1800;
    if (work->field_88.w > 0xDFFFF) {
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GrenadePistolScratch);
}

/// Flight state: steps the `field_88` flight timer down and moves the task to
/// state 3 once it runs out. The M4A1 Grenade carries an identical copy.
void func_grenade_pistol_8011DB60(Task* task)
{
    M4a1GrenadeWork* work  = task->work;
    s32              timer = work->field_88.w - 1;

    work->field_88.w = timer;
    if (timer <= 0) {
        task->state = 3;
    }
}

/// Exit callback: unlinks both collision nodes the spawn state linked and kills
/// the task.
void func_grenade_pistol_8011DB8C(Task* task)
{
    M4a1GrenadeWork* work = task->work;

    Gp_UnlinkObj(&work->obj);
    Gp_UnlinkObj(&work->obj2);
    taskKill(task);
}

const TaskFuncTable4 D_grenade_pistol_8011D1C4 = { {
    func_grenade_pistol_8011D3A0,
    func_grenade_pistol_8011D6FC,
    func_grenade_pistol_8011DB60,
    func_grenade_pistol_8011DB8C,
} };

void func_grenade_pistol_8011DBD0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_grenade_pistol_8011D1C4;
    sp.funcs[arg0->state](arg0);
}
