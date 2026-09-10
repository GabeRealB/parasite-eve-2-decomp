#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "kyle/kyle_800102.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

/// Spawn state: allocates the work block, seeds the thrown object at the
/// muzzle coordinate and links its two `GpObj` nodes.
void func_kyle_800102_80167A84(Task* arg0)
{
    void**          scratch;
    u8*             head;
    SVECTOR*        blk;
    SVECTOR*        vec;
    MATRIX*         mtx;
    TmdObject*      extra;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  muzzle;
    Kyle800102Work* work;
    s32             idx;
    s32             flags;
    s32             speed;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (SVECTOR*)(head - 8);
    *scratch = blk;
    extra    = arg0->extra;
    idx      = ((u32)arg0->spawnArg1 >> 16) & 0xF;
    coord    = extra->field_8;
    muzzle   = coord->sub;
    work     = Mem_Calloc(sizeof(Kyle800102Work), 0);
    vec      = blk;
    if (work == NULL) {
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
        Task_Kill(arg0);
        return;
    }
    arg0->idMap        = (TaskIdMap*)work;
    arg0->exitCallback = func_kyle_800102_80168270;
    arg0->state++;
    Mem_Set(work, 0, sizeof(Kyle800102Work));
    blk->vx     = D_kyle_800102_80177424[idx].vx;
    blk->vy     = D_kyle_800102_80177424[idx].vy;
    blk->vz     = D_kyle_800102_80177424[idx].vz;
    muzzle->flg = 0;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr_real();
    gte_stlvnl(coord->workm.t);
    mtx = &coord->coord;
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coord->workm, mtx);
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    extra->field_C = 0;
    Gfx_RotMatrixX(mtx, -0x400, 0);
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    speed              = D_kyle_800102_8017743C[idx];
    work->field_8C     = 1;
    work->field_90     = 0;
    work->obj.field_8  = coord;
    work->obj.field_C  = work->rec0;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->field_88.w   = speed << 16;
    flags              = (u16)arg0->spawnArg1 | 0x20000;
    work->obj.field_18 = flags;
    if (arg0->spawnArg1 & 0x100000) {
        work->obj.field_18 = flags | 0x80;
    }
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
    work->d4rec.field_A  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->obj.flags     |= 0xC400;
    work->d4rec.field_C  = -(work->field_88.w >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.field_14, 1, 0);
    work->obj2.flags       |= 0x4400;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

/// Flight state of Kyle's thrown object. Detonates when the shot has touched
/// world geometry (`rec0` with 0x30000), when a wall record it hit is solid,
/// or when the flight timer runs past 0xDFFFF; otherwise it steps the object
/// by `dir / field_88.h.hi`, lets gravity pull `dir.vy` down, and trails smoke
/// every `field_8C` frames - a divisor that grows by one every seven frames up
/// to four, so the trail thins as the object slows.
///
/// The shot is parameterised through `Task::spawnArg1`: its low byte is the
/// attachment id driving the explosion effect, the byte above it seeds the
/// sound bank, and bit 0x100000 marks the shot that plays the fixed
/// `0x40660002` clip instead.
void func_kyle_800102_80167DE0(Task* arg0)
{
    Kyle800102Scratch* blk;
    Kyle800102Work*    work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GpRoomParamRec*    param;
    u8*                head;
    s32                idx;
    s32                count;
    s32                clip;
    s32                step;
    s32                sfxarg;
    s32                sfxbase;

    work                    = (Kyle800102Work*)arg0->idMap;
    coord                   = ((TmdObject*)arg0->extra)->field_8;
    head                    = *(u8**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = head - sizeof(Kyle800102Scratch);
    blk                     = (Kyle800102Scratch*)(head - sizeof(Kyle800102Scratch));
    coord->flg              = 0;
    if (Gp_CountRec18Hi(work->rec0, 0x30000) != 0) {
    explode:
        blk->field_30 = arg0->spawnArg1 & 0xFF00;
        blk->sfx      = (u8)arg0->spawnArg1;
        arg0->state   = 2;
        Gp_SpawnEff(0x60071, coord, blk->sfx, NULL);
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
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Kyle800102Scratch);
        work->obj.field_1C      = D_kyle_800102_80177434[blk->sfx - 0xA];
        return;
    }

    count = Gp_CountRec18Hi(work->rec1, 0x100000);
    rec   = work->rec1;
    if (count == 0) {
        goto try_rec0;
    }
check:
    SOFT_USE_REG2(head, head);
    func_800E0FEC(rec, (GpDeltaScratch*)(head - 0x18), 1, &idx);
    idx = func_800E1ACC((u8*)&idx);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Kyle800102Scratch);
}

void func_kyle_800102_80168244(Task* arg0)
{
    Kyle800102Work* work  = (Kyle800102Work*)arg0->idMap;
    s32             timer = work->field_88.w - 1;

    work->field_88.w = timer;
    if (timer <= 0) {
        arg0->state = 3;
    }
}

void func_kyle_800102_80168270(Task* arg0)
{
    Kyle800102Work* work = (Kyle800102Work*)arg0->idMap;

    Gp_UnlinkObj(&work->obj);
    Gp_UnlinkObj(&work->obj2);
    Task_Kill(arg0);
}

INCLUDE_RODATA("kyle/nonmatchings/kyle_800102/kyle_800102", D_kyle_800102_80167A70);

/// Per-frame entry point: runs the task's current state. The table is a local,
/// so GCC copies it from `.rodata` onto the stack every frame.
void func_kyle_800102_801682B4(Task* arg0)
{
    Kyle800102StateFn states[4] = {
        func_kyle_800102_80167A84,
        func_kyle_800102_80167DE0,
        func_kyle_800102_80168244,
        func_kyle_800102_80168270,
    };

    states[arg0->state](arg0);
}
