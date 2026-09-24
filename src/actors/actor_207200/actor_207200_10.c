#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_207200.h"

extern u8 D_801153F4;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Dying-state tick of the small enemy, under the shared `D_801153F4` mode
/// byte: 1 does nothing and 2 hides the model. Otherwise the root part's
/// matrix is saved into `field_264` and refolded with the decaying Y scale.
/// Once `field_288` is set the enemy is destroyed after 0x3D frames; before
/// that, the kill countdown running out releases the state-0xF0 reference,
/// sets `field_288` and unlinks the enemy's node and its three objects, and
/// the two animation slots are rebound or advanced.
void func_actor_207200_8014AA74(GpEnemy* arg0, Task* arg1)
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
        func_actor_207200_8014B128(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    func_actor_207200_8014B128(arg1);
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
