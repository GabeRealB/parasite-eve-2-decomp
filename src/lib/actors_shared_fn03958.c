#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_80135d50.h"
#include "actors/actors_shared_8014ae08.h"
#include "actors/actors_shared_8014af2c.h"
#include "actors/actors_shared_8014df20.h"
#include "actors/actor_207200.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.

extern u8 D_801153F4;

void ActorsSharedFn03958(GpEnemy* arg0, Task* arg1)
{
    Actor207200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              i;
    Actor207200Work* anim;

    work  = arg1->work;
    obj   = arg1->extra;
    coord = obj->coords;
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags      |= 0x80;
            arg0->node.flags = 1;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        ActorsShared8014b128(arg1, coord);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    ActorsShared8014b128(arg1, coord);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C.obj);
        Gp_UnlinkObj(&work->field_FC.obj);
        Gp_UnlinkObj(&work->field_184.obj);
    }
    anim = arg1->work;
    i    = 1;
    if (anim->field_28C != (s16)anim->field_28E) {
        anim->field_28E = anim->field_28C;
        anim->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)anim, i, anim->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    anim->field_290 = (u16)(anim->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)anim, i);
        i++;
    } while (i < 3);
}
