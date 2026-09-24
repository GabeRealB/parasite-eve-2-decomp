#include "common.h"

#include "psyq/abs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_801633D4(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    func_actor_341700_801685F0(arg0, work->field_436);
    speed                                          = func_actor_341700_80168444(arg0, -0x10);
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    if (func_actor_341700_80168468(arg0)) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}

void func_actor_341700_80163600(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    GsCOORDINATE2*   root = ((TmdObject*)arg0->extra)->coords;
    MATRIX           local;
    s16              angle;
    s32              soundId;
    s32              pan;
    s16              facing;
    s16              speed;

    if ((s16)++work->field_412 < 40) {
        if ((s16)func_actor_341700_80168234(arg0)) {
            return;
        }
    } else {
        work->field_438 = 1;
    }
    if ((s16)work->field_412 == 43) {
        GsCOORDINATE2* coords = ((TmdObject*)arg0->extra)->coords;
        SVECTOR*       v;

        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        coords[6].flg = 0;
        Gp_UpdateCoord(&coords[6]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[6].workm, &local);
        v             = &work->field_98;
        v->vx         = local.t[0];
        v->vy         = local.t[1];
        v->vz         = local.t[2];
        coords[6].flg = 0;
    }
    if (work->field_412 >= 43 && work->field_412 <= 46) {
        work->field_432 = 1;
    } else {
        work->field_432 = 0;
    }
    if ((s16)work->field_412 == 46) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0005;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_412 == 45) {
        facing = (work->field_444 + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else {
            work->field_40C = work->field_7A;
        }
    }
    if (work->field_412 >= 45 && work->field_412 <= 53) {
        angle                                          = work->field_40C;
        speed                                          = -250;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
        work->obj_3AC.flags                           |= 0x8000;
    } else {
        work->obj_3AC.flags &= 0x7FFF;
    }
    if (work->field_412 >= 45 && work->field_412 <= 48 && work->field_43A < 0x171) {
        Actor341700Work* w;

        work->field_428      = 0;
        work->field_42A      = -200;
        w                    = (Actor341700Work*)arg0->work;
        w->field_426         = 2;
        w->field_41C         = 0x10;
        w->field_418         = 0x10;
        w->field_414         = 1;
        work->field_432      = 0;
        work->obj_3AC.flags &= 0x7FFF;
        work->field_422     += 2;
        return;
    }
    if ((s16)work->field_412 >= 47) {
        root->coord.t[1]     += work->field_42A;
        work->obj_2CC.pos.vy += work->field_42A;
        work->field_428      += 30;
        work->field_42A      += work->field_428;
        if (root->coord.t[1] >= (s16)work->field_92) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_426         = 2;
            w->field_41C         = 0x10;
            w->field_418         = 0x12;
            w->field_414         = 1;
            root->coord.t[1]     = (s16)work->field_92;
            work->obj_2CC.pos.vy = 0;
            work->field_412      = 0;
            work->field_422++;
        }
    }
}
