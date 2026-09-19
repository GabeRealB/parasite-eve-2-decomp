#include "common.h"
#include "actors/actor_350700.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350700_80162404`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350700_80169D1C[];

void func_actor_350700_801624D0(Task* arg0);
void func_actor_350700_801624D8(Task* arg0);

/// Per-frame tick of the enemy actor, the same body as
/// `func_actor_335800_80163568`: dispatches through the local two-entry table
/// the counter at `field_4C0` indexes, then integrates the local-space `step`
/// into the 16.16 accumulators at `field_4A0`, adds their high halves to the
/// root coordinate's translation and truncates them back to 16 bits. Ticks the
/// animation slots while `field_43C` is set, and -- unless the display object's
/// `field_C` carries 0x80 -- draws the ground-shadow quad from the second
/// part's world matrix, clears that part's `flg` and rebuilds its coordinate.
/// The `field_4C5` countdown then runs while it is non-negative, freeing the
/// model buffers on the frame it reaches zero; the init's -1 disables it.
void func_actor_350700_80161E88(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor350700Work* work     = (Actor350700Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350700_801624D0, func_actor_350700_801624D8 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    funcs[(s16)work->field_4C0](arg0);
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->field_4A0   += work->step.vx;
    work->field_4A4   += work->step.vy;
    work->field_4A8   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1] += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2] += (s16)(work->field_4A8 >> 16);
    coord->flg         = 0;
    work->field_4A0    = (u16)work->field_4A0;
    work->field_4A4    = (u16)work->field_4A4;
    work->field_4A8    = (u16)work->field_4A8;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_4C5 >= 0) {
        if (work->field_4C5 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C5--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162070);

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_350700_80169D0C[];

/// Spawn-placement message handler: seeds the work block's position and
/// rotation from `place`, picks the start animation from `anim` (or anim 3,
/// 2 once `field_4C4` is set) and installs it through the same preset body as
/// `func_actor_141000_801336DC`. Returns 0.
s32 func_actor_350700_801621B4(Task* task, s32 arg1, Actor350700Placement* place, Actor350700SpawnAnim* anim)
{
    Actor350700Work*       work;
    Actor350700Work*       w;
    Actor350700AnimPreset  preset;
    Actor350700AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor350700Work*)task->work;
    w->field_4C0   = 1;
    w->field_4C2   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4B8   = place->rot.vx;
    w->field_4BA   = place->rot.vy;
    w->field_4BC   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_43F   = anim->field_4;
    } else {
        if (w->field_4C4 != 0) {
            preset.field_4 = 2;
        } else {
            preset.field_4 = 3;
        }
        w->field_43F = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor350700Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84((GpAnimCtx*)work, D_actor_350700_80169D0C[work->field_43E], ext, work->poses,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162398);

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit handler. An allocation failure ends the task instead of
/// leaving a half-built actor behind.
void func_actor_350700_80162404(Task* arg0)
{
    Actor350700Work* work;

    work = memCalloc(sizeof(Actor350700Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C5 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_350700_801624B4(arg0);

    arg0->msgTable     = D_actor_350700_80169D1C;
    arg0->exitCallback = func_actor_350700_80162494;
    arg0->state       += 1;
}

void func_actor_350700_80162494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_350700_801624B4(Task* arg0)
{
    TmdObject*       ext;
    Actor350700Work* work;

    ext           = arg0->extra;
    work          = (Actor350700Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_350700_801624D0(Task* arg0)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801624D8);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E40);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E5C);
