#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Approach mode the last `func_actor_143900_801333C4` call selected.
extern s16 D_actor_143900_801496CC;

/// Draws the second variant's ground shadow quad under the model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's brightness
/// follows the room's current ground shade.
void func_actor_143900_80132F14(Task* task)
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

/// Helper-task handler of the second variant: state 0 hangs the task's own
/// coordinate frame off part `spawnArg1` of the second variant's model and
/// steps to state 1; every later tick relights the helper's model from a point
/// 0x320 above that model's root translation.
void func_actor_143900_80132FB0(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_143900_801496C8->extra)->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg      = 0;
            extra->flags    = 0;
            extra->otOffset = 0xF;
            coord->sub      = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Ticks animation slots 1..0x13 of the second variant's animation context.
void func_actor_143900_80133068(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496C4->anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 of the second variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
void func_actor_143900_801330B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496C4->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496C4->anim, i, D_actor_143900_801496C4->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->field_4B6 = D_actor_143900_801496C4->field_4B8;
}

/// Reseeds animation slots 1..0x13 of the second variant's work block from the
/// current animation id with the latched reset argument, and records that id as
/// the one now playing.
void func_actor_143900_80133144(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_143900_801496C4->anim, i, D_actor_143900_801496C4->field_4B8, 0,
                      D_actor_143900_80149630);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496C4->field_4B6 = D_actor_143900_801496C4->field_4B8;
}

/// Message 0x7D3 handler of the second variant: adopts `preset`'s animation id
/// when it is one of the first 0xC, latches the reset mode and the reset
/// argument the reseed uses, then hands the published task to the per-frame
/// update. Ids past the range are rejected with -1 and leave the work block
/// untouched.
s32 func_actor_143900_801331C4(Task* task, s32 arg1, Actor143900AnimPreset* preset)
{
    if (preset->field_4 < 0xC) {
        D_actor_143900_801496C4->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_143900_801496C4->field_4B4 = 1;
            D_actor_143900_80149630            = preset->field_C;
        } else {
            D_actor_143900_801496C4->field_4B4 = 2;
        }
        D_actor_143900_801496C4->field_4BA = 0;
        func_actor_143900_80132A9C(D_actor_143900_801496C8);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler of the second variant: applies `arg2` to the three
/// models it owns - its own task's and the two helper tasks'. Bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden); bit 1 ORs in 0x4.
s32 func_actor_143900_80133254(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_143900_801496C8->extra;
    TmdObject* first  = D_actor_143900_801496C4->field_4F0->extra;
    TmdObject* second = D_actor_143900_801496C4->field_4F4->extra;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
    }
    if (arg2 & 2) {
        own->flags    |= 4;
        first->flags  |= 4;
        second->flags |= 4;
    }
    return 0;
}

/// Message 0x7D4 handler of the second variant: seeds the task's `TmdObject`
/// coordinate frame from `placement`. Only the yaw is used, remembered in the
/// work block and applied with `Gfx_RotMatrixY`, then the three longs become
/// the coordinate's translation.
s32 func_actor_143900_801332E4(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_143900_801496C4->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler of the second variant: the payload's halfword at 0x2
/// picks which of the two helper tasks' models is shown - 0 shows the second
/// (`field_4F4`) and hides the first, 1 the reverse; any other value leaves
/// both.
s32 func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg)
{
    TmdObject* first;
    TmdObject* second;

    first  = D_actor_143900_801496C4->field_4F0->extra;
    second = D_actor_143900_801496C4->field_4F4->extra;
    switch (msg->field_2) {
        case 0:
            second->flags = 0;
            first->flags  = 0x80;
            break;
        case 1:
            first->flags  = 0;
            second->flags = 0x80;
            break;
    }
    return 0;
}

/// Message 0x7DD handler of the second variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_801333C4(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*    coord;
    Actor143900Work2* work;
    s32               dx;
    s32               dz;
    s32               steps;
    s32               dist;
    s32               angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor143900Work2*)task->work;
    D_actor_143900_801496CC = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_143900_801496CC == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496CC) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}
