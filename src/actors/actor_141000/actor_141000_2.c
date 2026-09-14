#include "common.h"

#include "actors/actor_141000.h"
#include "actors/actors_shared_801327b4.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E24;

/// The descriptor table the controller spawns from; index 2 is the model
/// actor the state at 0x80132EF4 spawns every eighth frame.
extern TaskDesc D_actor_141000_801348D8[];

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_141000_8013392C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_141000_8013D788[];

/// State 0 of the handler table at 0x80131E3C: ramps the actor's Z scale by
/// 1/16 a frame and, on reaching 1.0, clamps it there and advances the state
/// index `field_C` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132E24(Task* arg0)
{
    Actor141000Work* work;
    u16              scale;

    work          = (Actor141000Work*)arg0->idMap;
    scale         = work->field_A + 0x100;
    work->field_A = scale;
    if ((s16)scale >= 0x1000) {
        work->field_A = 0x1000;
        work->field_C = work->field_C + 1;
    }
    func_actor_141000_80132FD0(((TmdObject*)arg0->extra)->field_8, 0);
    func_actor_141000_8013308C(((TmdObject*)arg0->extra)->field_8, (s16)work->field_A);
}

/// State 1 of the handler table at 0x80131E3C: holds for 0x1F frames, then
/// advances the state index `field_C` the dispatcher at 0x80132D3C walks.
void func_actor_141000_80132EB0(Task* arg0)
{
    Actor141000Work* work;
    u16              ticks;

    work          = (Actor141000Work*)arg0->idMap;
    ticks         = work->field_E + 1;
    work->field_E = ticks;
    if ((s16)ticks >= 0x1F) {
        work->field_C = work->field_C + 1;
    }
}

/// State 2 of the handler table at 0x80131E3C: drives the model's rotation
/// through `func_actor_141000_80132FD0` and, on the frame that runs the ramp's
/// 0x5A entries out, advances the state index `field_C` the dispatcher at
/// 0x80132D3C walks. Every eighth frame it spawns another actor from index 2
/// of `D_actor_141000_801348D8` and copies this actor's world position onto
/// the new one.
void func_actor_141000_80132EF4(Task* arg0)
{
    Actor141000CtrlWork* work;
    TmdObject*           obj;
    Task*                spawned;
    GsCOORDINATE2*       src;
    GsCOORDINATE2*       dst;
    u16                  frames;

    work         = (Actor141000CtrlWork*)arg0->idMap;
    obj          = arg0->extra;
    frames       = work->frames + 1;
    work->frames = frames;

    if (func_actor_141000_80132FD0(obj->field_8, (s16)frames) != 0) {
        work->state = work->state + 1;
        return;
    }

    if (!(work->frames & 7)) {
        spawned = Task_SpawnFromTable(D_actor_141000_801348D8, 2, 0, 0);
        if (spawned != NULL) {
            src             = ((TmdObject*)arg0->extra)->field_8;
            dst             = ((TmdObject*)spawned->extra)->field_8;
            dst->coord.t[0] = src->coord.t[0];
            dst->coord.t[1] = src->coord.t[1];
            dst->coord.t[2] = src->coord.t[2];
        }
    }
}

void func_actor_141000_80132FC8(void)
{
}

/// Drives the model root one frame along the ramp the rotation table at
/// 0x80134228 and its position table at 0x801344F8 hold: splat an identity
/// matrix, let `RotMatrix` replace it with the frame's triple -- entry 0x59
/// once `arg1` runs past the table's 0x5A entries -- copy that entry's
/// position into the root's translation, drop X by 40 and clear `flg`.
/// Returns non-zero on the frame that ran past the table, which is what the
/// state-2 handler at 0x80132EF4 advances `field_C` on.
s32 func_actor_141000_80132FD0(GsCOORDINATE2* arg0, s32 arg1)
{
    Actor141000MatWords* words;
    SVECTOR*             pos;
    s32                  idx;
    s32                  ret;

    if (arg1 < 0x5A) {
        idx = arg1;
        ret = 0;
    } else {
        idx = 0x59;
        ret = 1;
    }
    words          = (Actor141000MatWords*)&arg0->coord;
    words->m00_m01 = 0x1000;
    words->m02_m10 = 0;
    words->m11_m12 = 0x1000;
    words->m20_m21 = 0;
    words->m22     = 0x1000;
    RotMatrix(&D_actor_141000_80134228[idx], &arg0->coord);
    pos               = &D_actor_141000_801344F8[idx];
    arg0->coord.t[0]  = pos->vx;
    arg0->coord.t[1]  = pos->vy;
    arg0->coord.t[2]  = pos->vz;
    arg0->coord.t[0] -= 0x28;
    arg0->flg         = 0;
    return ret;
}

void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1)
{
    VECTOR scale;

    scale.vz = arg1;
    scale.vx = 0x1000;
    scale.vy = 0x1000;
    ScaleMatrix(&arg0->coord, &scale);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801330C0);

void func_actor_141000_801331AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E24;
    sp.funcs[task->state](task);
}

/// Chains this actor's root coordinate under the spawner's, then hands the task
/// to it. Same attach shape as `ActorsShared80132450`, but the parent's part is
/// always its root coordinate and the actor is flagged for immediate unlink
/// (`killCountdown`) instead of surviving to a later state.
void func_actor_141000_80133204(Task* task)
{
    ((TmdObject*)task->extra)->field_8->sub = ((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)task->spawnArg2, task);
    task->killCountdown = 0x7FF;
    task->state        += 1;
}

void func_actor_141000_80133260(Actor141000* arg0)
{
    Actor141000Point sp10[24];
    s32              spD0;
    s32              spD4;

    func_actor_141000_801323F0(arg0, sp10, &spD0, &spD4);
    func_actor_141000_80131E94(arg0, sp10, spD0);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801332A0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80133490);

/// The three texture uploads `func_actor_141000_801335D4` walks: one per value
/// of `Actor141000Work::field_4CA`, each a `GpImgRec` whose own `rect` carries
/// the 0x19x0x14 upload size the state's scratch `RECT` repeats and whose
/// `data` points at the pixel blob. `func_actor_141000_80133FA8` picks from
/// the same table.
extern GpImgRec D_actor_141000_8013CA7C;
extern GpImgRec D_actor_141000_8013CE84;
extern GpImgRec D_actor_141000_8013D28C;

/// Texture-upload state of the enemy actor: runs the countdown at
/// `Actor141000Work::field_4C6` down one a frame while `field_4CA` names the
/// upload in progress, and on the frame it underflows posts that step's image
/// over the 0x19x0x14 rect at 0x10 -- reloading the countdown from `field_4C4`
/// and advancing `field_4CA` for steps 1 and 2, or clearing `field_4CA` and
/// starting over for step 3. Steps 1 and 2 share their whole tail, which is
/// what makes the compiler emit one copy of it that step 1 jumps into; step 3
/// only differs in clearing the step instead of advancing it.
void func_actor_141000_801335D4(GpActorWork* arg0)
{
    Actor141000Work* work;
    RECT             rect;

    work   = (Actor141000Work*)arg0->actor;
    rect.x = 0;
    rect.y = 0x40;
    rect.w = 0x19;
    rect.h = 0x14;

    switch (work->field_4CA) {
        case 1:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013D28C, &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 2:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CE84, &rect);
                work->field_4C6 = work->field_4C4;
                work->field_4CA = work->field_4CA + 1;
            }
            break;
        case 3:
            work->field_4C6 = work->field_4C6 - 1;
            if ((s16)work->field_4C6 < 0) {
                Gp_LoadActorImage(arg0, &D_actor_141000_8013CA7C, &rect);
                work->field_4CA = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801336DC);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801338C0);

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block that
/// every later handler reads through `Task::idMap`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the shared exit handler. An allocation failure ends the task
/// instead of leaving a half-built actor behind.
void func_actor_141000_8013392C(Task* arg0)
{
    Actor141000Work* work;

    work = Mem_Calloc(sizeof(Actor141000Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C9 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_141000_801339DC(arg0);

    arg0->field_24     = D_actor_141000_8013D788;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state++;
}
