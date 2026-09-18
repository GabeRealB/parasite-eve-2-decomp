#include "common.h"

#include "actors/actor_141000.h"
#include "actors/actors_shared_801327b4.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
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

    work          = (Actor141000Work*)arg0->work;
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

    work          = (Actor141000Work*)arg0->work;
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

    work         = (Actor141000CtrlWork*)arg0->work;
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

/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;

/// Callback of the model actor the controller's state 2 spawns every eighth
/// frame -- index 2 of `D_actor_141000_801348D8`, pointed at the controller's
/// own position. The first frame splats an identity matrix over the task's root
/// coordinate and clears its `flg`: the rotation part only, so the translation
/// the spawner copied in survives. Every frame then runs the 5-frame countdown
/// in `killCountdown`, spawning effect 0x60070 from that same coordinate
/// (spawn arg 0x14200, no offset vector) each time it completes. The countdown
/// is held while the freeze byte is set, and a room change or a script event
/// taking over kills the task outright.
void func_actor_141000_801330C0(Task* arg0)
{
    GsCOORDINATE2*       coord;
    Actor141000MatWords* words;
    u16                  count;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (arg0->state == 0) {
        words          = (Actor141000MatWords*)&coord->coord;
        words->m00_m01 = 0x1000;
        words->m02_m10 = 0;
        words->m11_m12 = 0x1000;
        words->m20_m21 = 0;
        words->m22     = 0x1000;
        coord->flg     = 0;
        arg0->state   += 1;
    }
    if (D_801153F4 == 0) {
        count               = arg0->killCountdown + 1;
        arg0->killCountdown = count;
        if ((s16)count >= 5) {
            arg0->killCountdown = 0;
            Gp_SpawnEff(0x60070, coord, 0x14200, NULL);
        }
    }
    if ((gGameSession->field_4D != 0) || (gGameSession->field_5F != 0)) {
        Task_Kill(arg0);
    }
}

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

void func_actor_141000_801339F8(void);
void func_actor_141000_80133A00(Task* arg0);
void func_actor_141000_801335D4(GpActorWork* arg0);

/// Per-frame tick of the model actor, the twin of `func_actor_335800_80163568`:
/// runs the motion handler `field_4C0` selects, steps the 16.16 accumulators
/// by `step` and moves the coordinate by their integer part, ticks the
/// animation slots and ground shadow while visible, runs the texture-upload
/// state, and counts `field_4C9` down to the buffer free.
void func_actor_141000_801332A0(Task* task)
{
    TmdObject*       ext      = task->extra;
    Actor141000Work* work     = (Actor141000Work*)task->work;
    TaskFunc         funcs[2] = { (TaskFunc)func_actor_141000_801339F8, func_actor_141000_80133A00 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    funcs[(s16)work->field_4C0](task);
    coord              = ((TmdObject*)task->extra)->field_8;
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
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
        ((TmdObject*)task->extra)->field_8[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
    }
    func_actor_141000_801335D4((GpActorWork*)task);
    if (work->field_4C9 >= 0) {
        if (work->field_4C9 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C9--;
    }
}

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

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_141000_8013D778[];

/// Placement handler: stores the spawn position and rotation, resets the body
/// state, then applies a start preset exactly as `func_actor_141000_80133CD8`
/// does (inlined here). The default anim id is chosen by the `field_4C8`
/// variant; writing it as an if/else into the preset (not a ternary) is what
/// keeps CSE from reusing the earlier constant 1 for the `field_43F` store.
s32 func_actor_141000_801336DC(Task* task, s32 arg1, Actor141000Placement* place, Actor141000SpawnAnim* anim)
{
    Actor141000Work*       work;
    Actor141000Work*       w;
    Actor141000AnimPreset  preset;
    Actor141000AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor141000Work*)task->work;
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
        if (w->field_4C8 != 0) {
            preset.field_4 = 2;
        } else {
            preset.field_4 = 0xA;
        }
        w->field_43F = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor141000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84((GpAnimCtx*)work, D_actor_141000_8013D778[work->field_43E], (GpAnimObj*)ext, work->poses,
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

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801338C0);

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
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

    arg0->work      = (TaskIdMap*)work;
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
