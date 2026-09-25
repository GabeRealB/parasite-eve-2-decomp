#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_350500.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Optional start animation the placement handler takes: the preset's
/// `field_4` and the `field_43F` byte. Absent, the defaults are anim 3 (or 2
/// once `field_4C4` is set) and 1.
typedef struct Actor350500SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor350500SpawnAnim;

/// Ground-shadow quad the tick feeds the second part's world translation to.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Global freeze byte in the main executable; the state dispatcher runs
/// nothing while it is non-zero.
extern u8 D_801153F4;

void func_actor_350500_80161E50(Task* arg0);
void func_actor_350500_801623CC(Task* arg0);

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
const TaskFuncTable3 D_actor_350500_80161E24 = { {
    func_actor_350500_801623CC,
    func_actor_350500_80161E50,
    func_actor_350500_8016245C,
} };

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350500_801623CC`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350500_80168EB0[];

/// Per-frame tick: runs the idle or the walk handler `field_4C0` selects,
/// then integrates the world-space `step` into the 16.16 accumulators at
/// `field_4A0`, adds their high halves to the root coordinate's translation
/// and truncates them back to 16 bits. Ticks the animation slots while
/// `field_43C` is set, and -- unless the model is hidden -- draws the
/// ground-shadow quad under the second part, clears that part's `flg` and
/// rebuilds its coordinate. The `field_4C5` countdown then runs while it is
/// non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
void func_actor_350500_80161E50(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350500_80162498, func_actor_350500_801624A0 };
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
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
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

/// Walk step 2, the approach test. Once the X/Z distances from the root
/// coordinate to `target` stop shrinking below `limit`, plays the `field_43F`
/// animation, clears `step` and advances `field_4C2`; otherwise records the
/// distances as the new `limit`.
void func_actor_350500_80162038(Task* arg0)
{
    Actor350500Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor350500AnimPreset preset;

    work  = (Actor350500Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_43F;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4C2++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Placement message handler: seeds the work block's target and placement
/// rotation from `place`, starts the walk sequence, and picks the start
/// animation from `anim` (or anim 3, 2 once `field_4C4` is set), installing
/// it with the body of `func_actor_350500_80162828` written out inline.
/// Returns 0.
s32 func_actor_350500_8016217C(Task* task, s32 arg1, Actor350500Placement* place, Actor350500SpawnAnim* anim)
{
    Actor350500Work*       work;
    Actor350500Work*       w;
    Actor350500AnimPreset  preset;
    Actor350500AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor350500Work*)task->work;
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
    work = (Actor350500Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_350500_80168EA0[work->field_43E], ext, work->poses,
                      work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

/// Per-frame dispatcher: runs the spawn, tick or exit state from
/// `D_actor_350500_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350500_80162360(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350500_80161E24;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and the three cleared words the work's own init expects, republishes the
/// light and colour matrices onto the display object, then installs the
/// message table and the exit handler. An allocation failure ends the
/// task instead of leaving a half-built actor behind.
void func_actor_350500_801623CC(Task* arg0)
{
    Actor350500Work* work;

    work = memCalloc(sizeof(Actor350500Work), false);
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

    func_actor_350500_8016247C(arg0);

    arg0->msgTable     = D_actor_350500_80168EB0;
    arg0->exitCallback = func_actor_350500_8016245C;
    arg0->state        = arg0->state + 1;
}
