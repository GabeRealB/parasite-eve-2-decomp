#include "common.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "psyq/inline_c.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_80163354.h"

/// Sub-state handler: slides the model's root toward `field_70` in x/z,
/// accelerating with `field_42A`; after 90 frames it also eases y in and marks
/// `field_438`. Within 800 units it advances `field_422`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `field_438` when
/// state 4 is pending).
void func_actor_342400_80168B74(Task* arg0)
{
    TmdObject*       obj;
    Actor342400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s16              next;

    obj   = arg0->extra;
    work  = (Actor342400Work*)arg0->idMap;
    enemy = (GpEnemy*)arg0->spawnArg2;
    coord = obj->field_8;
    work->field_412++;
    work->field_428++;
    work->field_42A += work->field_428;
    if ((s16)work->field_412 < 0x5A) {
        s32 step = work->field_42A >> 6;

        c      = ((TmdObject*)arg0->extra)->field_8;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                           = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    } else {
        s32 step;

        work->field_438 = 1;
        step            = work->field_42A >> 5;
        c               = ((TmdObject*)arg0->extra)->field_8;
        dir.vx          = work->field_70.vx - c->coord.t[0];
        dir.vy          = 0;
        dir.vz          = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                           = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
        coord->coord.t[1]                              += (work->field_70.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    __asm__ volatile("nop; nop; .word 0x4AA00428"); // sqr 0
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->field_422++;
        return;
    }
    if (enemy->field_40 <= 0) {
        SndEvt_EnqueueType7(0x402C0002, 1);
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor342400Work* w = (Actor342400Work*)arg0->idMap;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor342400Work* w = (Actor342400Work*)arg0->idMap;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor342400Work* w;

                next         = D_actor_342400_80173A98[work->field_418 - 1];
                w            = (Actor342400Work*)arg0->idMap;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Five state handlers, indexed by `Actor342400Work::field_420`; copied to
/// the stack before dispatch.
extern TaskFuncTable5 D_actor_342400_80162028;

/// Seven state handlers, indexed by `Actor342400Work::field_420`; copied to
/// the stack before dispatch.
extern TaskFuncTable7 D_actor_342400_8016203C;

/// Same helper as in `actor_342400_6.c`: colours `enemy` from `coord`'s world
/// position through a 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`.
static __inline__ void update_color(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Per-frame callback, the five-state counterpart of
/// `func_actor_342400_801690FC`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
void func_actor_342400_80168F14(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable5   sp    = D_actor_342400_80162028;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->field_C &= ~0x80;
            return;
    }
}

/// Per-frame callback, the seven-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
void func_actor_342400_801690FC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor342400Work* work  = (Actor342400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable7   sp    = D_actor_342400_8016203C;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            update_color(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                ActorsShared80163354(arg0, 2, 6, 0xC8, 0, 0xFF);
                ActorsShared80163354(arg0, 1, 7, 0x80, 0, 0xFF);
                ActorsShared80163354(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
