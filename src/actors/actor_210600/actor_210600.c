#include "common.h"

#include "actors/actor_210600.h"
#include "actors/actors_shared_80135a60.h"

#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_80149E30);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A13C);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A484);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014A9D0);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600", func_actor_210600_8014AB74);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation request handler: step 1 of the work block's `field_87C` seeks
/// every slot 1..18 to the clip in `field_882` through `func_800B4114`,
/// passing `field_886`'s rate byte into the slot and the step `field_880`'s row
/// of `D_actor_210600_8015A498` as the request's fifth argument, then latches
/// the clip into `field_880`; step 2 does the same through
/// `Gp_AnimResetSlot`. Both settle on step 3 and clear the frame counter at
/// `field_884`, which is counted from here on while every slot is ticked.
void func_actor_210600_8014B2C0(Task* task)
{
    Actor210600Work* work;
    Actor210600Work* start;
    Actor210600Work* reset;
    Actor210600Work* tick;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor210600Work*)task->work;
    if (work->field_87C == 1) {
        start = (Actor210600Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->slots[i].rate = start->field_886.byte;
            func_800B4114(&start->anim, i, (s16)start->field_882, 0,
                          D_actor_210600_8015A498[start->field_880][(s16)start->field_882]);
        }
        start->field_880 = start->field_882;
        goto advance;
    }
    if (work->field_87C == 2) {
        reset = (Actor210600Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->slots[j].rate = reset->field_886.byte;
            Gp_AnimResetSlot(&reset->anim, j, (s16)reset->field_882);
        }
        reset->field_880 = reset->field_882;
    advance:
        work->field_87C = 3;
        work->field_884 = 0;
    }
    if (work->field_88A == 2) {
        work->field_88A = 3;
    }
    work->field_884++;
    tick = (Actor210600Work*)task->work;
    for (k = 1; k < 0x13; k++) {
        tick->slots[k].rate = tick->field_886.byte;
        Gp_AnimTickIndex(&tick->anim, k);
    }
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds the model's root part rotation around the yaw it already faces and
/// rescales it uniformly through a 0x34-byte block borrowed from
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Same body as `ActorsShared80135a60`, inlined so the
/// scratch-head accesses stay absolute.
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale)
{
    ActorShared80135a60Scratch* blk;
    GsCOORDINATE2*              coord;
    u8*                         head;
    s16                         ang;
    u16                         m22;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    coord                                         = ((TmdObject*)task->extra)->coords;
    blk                                           = (ActorShared80135a60Scratch*)(head - 0x34);
    *(ActorShared80135a60Scratch**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]  = *(u16*)&((ActorShared80135a60Scratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]  = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]  = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]  = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]  = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]  = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]  = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]  = *(u16*)&blk->m.m[2][1];
    m22                   = *(u16*)&blk->m.m[2][2];
    coord->flg            = 0;
    coord->coord.m[2][2]  = m22;
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x34;
}

/// Update body of the actor's state machine. While the work block's 0x890 flag
/// is clear it runs the animation pass, rebuilds the model root's Y rotation at
/// 0.75 scale, and -- the first time animation slot 1 holds clip 7 -- spawns the
/// effect `Gp_GetIdParam1(0x1001)` on part 1 of the model through
/// `Actor210600Work::field_896`, which remembers the clip slot 0 holds so the
/// spawn is not repeated.
void func_actor_210600_8014B434(void* spawnArg2, Task* task)
{
    Actor210600Work* work;
    SVECTOR          vec;
    GpEffArg         eff;
    s32              id;

    work = (Actor210600Work*)task->work;
    if (work->field_890 == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->slots[1].curRec & 0x3FF;
        if (id == 7 && work->field_896 != id) {
            memset(&vec, 0, 8);
            eff.coord      = ((TmdObject*)task->extra)->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, ((TmdObject*)task->extra)->coords + 1, &vec, &eff);
        }
        work->field_896 = work->slots[0].curRec & 0x3FF;
    }
}

/// Display-object mode handler. `arg2` selects the mode: 0 hides the display
/// object by setting bit 0x80 of `TmdObject.flags`, 1 clears `field_C` and so
/// shows it, 2 sets bit 0x4, and any other value clears the field and then sets
/// bit 0x4. Modes 0 and 1 reinstate the object's buffers through
/// `Tmd_AllocBuffers`; modes 0 and 2 arm the work block's 0x890 flag where the
/// other two clear it. `arg1` is unused; it exists because the dispatch passes
/// three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor210600Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_890 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_890 = 0;
            break;
        case 2:
            obj->flags     |= 4;
            work->field_890 = 1;
            break;
        default:
            obj->flags      = 4;
            work->field_890 = 0;
            break;
    }
    return 0;
}
INCLUDE_RODATA("actors/nonmatchings/actor_210600/actor_210600", D_actor_210600_80149E24);
