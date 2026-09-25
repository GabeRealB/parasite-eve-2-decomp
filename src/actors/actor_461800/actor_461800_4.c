#include "common.h"

#include "actors/actor_461800.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80133970(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->coords;
    D_actor_461800_801438A0->field_4AE = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, Actor461800Msg* msg, s32 arg3)
{
    s32 id;

    id = msg->field_2;
    switch (id) {
        case 0:
            D_actor_461800_801438A0->field_4B4 = 0x14;
            break;
        case 1:
            D_actor_461800_801438A0->field_4BC = id;
            break;
    }
    return 0;
}

/// Approach mode the last `func_actor_461800_80133A3C` call selected.
extern s16 D_actor_461800_801438A8;

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance in the second work block: the planar distance over 60
/// steps in mode 0, 15 in mode 1 and 25 in mode 2.
s32 func_actor_461800_80133A3C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*    coord;
    Actor461800Work2* work;
    s32               dx;
    s32               dz;
    s32               steps;
    s32               dist;
    s32               angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor461800Work2*)task->work;
    D_actor_461800_801438A8 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->field_4AE         = angle;
    if (D_actor_461800_801438A8 == 1) {
        work->field_4AE = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_461800_801438A8) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->field_4B2 = dist / steps;
    return 0;
}

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the ground shadow quad under the model root, unless the model is
/// hidden (`flags & 0x80`) or has no buffer yet. The root's world translation
/// is staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
void func_actor_461800_80133B98(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}
