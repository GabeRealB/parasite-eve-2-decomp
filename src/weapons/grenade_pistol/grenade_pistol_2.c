#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/grenade_pistol.h"
#include "weapons/m4a1_grenade.h"

/// Impact clip id per attachment, indexed by `sfx - 0xA`.
extern u16 D_grenade_pistol_8012B430[];

/// Flight state of the grenade pistol's projectile. Detonates when the shot
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

    work  = (M4a1GrenadeWork*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
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
            Gp_PlayObjSfx((GpObj38*)coord, 0x40660002, 1);
        } else {
            sfxbase = blk->field_30 << 8;
            sfxarg  = ((blk->sfx - 0xA) << 24) | 0x20000005;
            Gp_PlayObjSfx((GpObj38*)coord, sfxbase | sfxarg, 1);
        }
        clip = 8;
        if (blk->sfx == 0xB) {
            clip = 1;
        }
        work->field_88.w        = clip;
        work->obj.flags        &= 0xBFFF;
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GrenadePistolScratch);
        work->obj.field_1C      = D_grenade_pistol_8012B430[blk->sfx - 0xA];
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
    work->d4rec.field_C = -(work->field_88.w >> 9);
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
