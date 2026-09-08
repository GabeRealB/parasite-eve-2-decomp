#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/3CD8.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_143900_801413B8;

/// Draws the actor's ground shadow quad under the model root, skipping actors
/// that are pending a deferred kill (`field_C & 0x80`) or that have no aux
/// buffer yet. The world position is the translation of the root part's
/// `workm`, staged in a scratchpad VECTOR3 rather than on the stack, and the
/// quad's brightness follows the room's current `Gp_State1C` level.
void func_actor_143900_8013242C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (!(obj->field_C & 0x80) && obj->field_18 != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->field_8);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x14);
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132514);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_143900_801325A4(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8, 0,
                      D_actor_143900_801413B8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132624);
