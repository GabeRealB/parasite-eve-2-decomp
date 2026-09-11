#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"
#include "actors/actor_342400.h"

void func_actor_342400_80168394(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    Actor342400Work* next;
    Actor342400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor342400Work*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = 0x14;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work2                                           = (Actor342400Work*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor342400Work*)arg0->idMap;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor342400Work*)arg0->idMap;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Sub-state handler, the steering counterpart of `func_actor_342400_80168B74`:
/// while the enemy lives it turns `field_7A` toward `field_70` and pushes the
/// root back along it, then slides toward `field_70` accelerating with
/// `field_42A`. Past 120 frames (or once dead) it eases y in and marks
/// `field_438`; while alive a hit flag plays sound 1. Within 800 units it
/// advances `field_422`, otherwise a dead enemy queues its follow-up animation.
void func_actor_342400_80168530(Task* arg0)
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
    s32              dist;
    s32              cond;
    s32              soundId;
    s32              pan;

    obj   = arg0->extra;
    work  = (Actor342400Work*)arg0->idMap;
    coord = obj->field_8;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work->field_412++;
    if (enemy->field_40 > 0) {
        Actor342400Work* w;
        s32              diff;
        s32              k;
        s32              step;

        if ((u32)(work->field_44F >> 1) < 0x40) {
            work->field_41C = work->field_44F >> 2;
            work->field_44F++;
        } else {
            work->field_41C = 0x40;
        }
        w      = (Actor342400Work*)arg0->idMap;
        c      = ((TmdObject*)arg0->extra)->field_8;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->field_7A - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->field_7A -= 0x18;
        } else if (diff < -0x100) {
            w->field_7A += 0x18;
        }
        angle                                           = work->field_7A;
        k                                               = -0x10;
        step                                            = ((((Actor342400Work*)arg0->idMap)->field_41C * k) << 12) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    work->field_428++;
    work->field_42A += work->field_428;
    {
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
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    __asm__ volatile("nop; nop; .word 0x4AA00428"); // sqr 0
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->field_412 > 120) {
        if (dist <= 3000) {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 4;
        } else {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->field_40 > 0) {
        Actor342400Work* w2 = (Actor342400Work*)arg0->idMap;

        if ((w2->flags_EC.half & 1) || (w2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
            pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
    } else {
        work->field_438    = 1;
        coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->field_422++;
        return;
    }
    if (enemy->field_40 <= 0) {
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
                s16              next;

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
