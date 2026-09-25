#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/rand.h"

void func_actor_460200_801325FC(Task* task);

s32 func_actor_460200_80132B2C(Task* task, s32 arg1, GpAnimArg* args);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, GpAnimArg* args);

s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags);

extern s8 D_8007272D;
extern u8 D_801156F9;

extern s32 D_actor_460200_8013FC50;
extern s32 D_actor_460200_8013FC8C;

extern u8 D_actor_460200_8013FCCC[];

extern s32 D_actor_460200_80135F14;
extern s32 D_actor_460200_8013607C;
extern s32 D_actor_460200_80136234;
extern s32 D_actor_460200_80137AA0;
extern s32 D_actor_460200_80137BA8;
extern s32 D_actor_460200_80137CB0;
extern s32 D_actor_460200_80137DA0;
extern s32 D_actor_460200_80137F98;
extern s32 D_actor_460200_80137FE0;
extern s32 D_actor_460200_80138028;
extern s32 D_actor_460200_80138070;

extern RECT D_actor_460200_80135E0C;
extern RECT D_actor_460200_80135E14;

extern TaskDesc D_actor_460200_80148118[];
extern u8       D_actor_460200_80148130[];
extern u8       D_actor_460200_801480E8[];
extern s32      D_actor_460200_801514FC;
extern s32      D_actor_460200_80151538;

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_460200_80131FB0(void);
void func_actor_460200_80132808(GpEnemy* enemy, Task* task);
void func_actor_460200_80132950(Task* task);
void func_actor_460200_80132978(Task* task);
void func_actor_460200_80132A04(Task* task);
void func_actor_460200_80132A50(Task* task);
void func_actor_460200_80132AC8(Task* task);
void func_actor_460200_80132F0C(Task* task);
void func_actor_460200_8013311C(GpEnemy* enemy, Task* task);
void func_actor_460200_8013322C(Task* task);
void func_actor_460200_80133254(Task* task);
void func_actor_460200_801332E0(Task* task);
void func_actor_460200_8013332C(Task* task);
void func_actor_460200_801333A4(Task* task);
void func_actor_460200_801338C0(GpEnemy* enemy, Task* task);
void func_actor_460200_80133A04(GpEnemy* enemy, Task* task);
void func_actor_460200_80133A88(Task* task);
void func_actor_460200_80133AB0(Task* task);
void func_actor_460200_80133B3C(Task* task);
void func_actor_460200_80133B88(Task* task);
void func_actor_460200_80133C00(Task* task);

void func_actor_460200_80131E24(Task* task)
{
    OverlayCaptureArgs* args;
    s32                 i;
    u32*                strip;

    args = task->spawnArg2;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    D_actor_460200_80135E14.y = 0;
                } else {
                    D_actor_460200_80135E14.y = 0x110;
                }
                if (gDisplayState.field_112 < 0) {
                    StoreImage(&D_actor_460200_80135E0C, Fs_ImgBuffers->buffers[0]);
                } else {
                    strip = Fs_ImgBuffers->buffers[0];
                    for (i = 0; i < 20; i++) {
                        D_actor_460200_80135E14.x = i * 16;
                        StoreImage(&D_actor_460200_80135E14, strip);
                        strip += 1920;
                    }
                }
                gDisplayState.skipDraw = 1;
                goto advance;
            case 1:
                DrawSync(0);
                func_actor_460200_80131FB0();
            advance:
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}

void func_actor_460200_80131FB0(void)
{
    s32          i;
    u32          maskR;
    u32          maskG;
    u32          maskB;
    u32          maskAll;
    u32*         p0;
    u32*         p1;
    u32          a0;
    u32          a2;
    u32          a1;
    register u32 v0 asm("v0");
    register u32 v1 asm("v1");

    p0      = Fs_ImgBuffers->buffers[0];
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    p1      = p0 + 1;

    do {
        i += 1;
        a0 = *p1;
        a2 = *p0;

        v1 = (a0 & maskR) << 8;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 1;
        a1 = v0 + v1;

        v1 = (a0 & maskG) << 3;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 2;
        a1 = a1 + v0;

        a0 = a0 >> 2;
        v1 = a0 & maskB;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        a1 = a1 + v1;

        v0 = a1 >> 3;
        a1 = v0 & maskAll;
        a1 = maskAll - a1;

        a2 = a1 & maskR;
        v0 = a2 << 10;
        v1 = a2 << 5;
        v0 = v0 | v1;
        a2 = a2 | v0;

        a0 = a1 & maskB;
        a0 = a0 >> 8;
        v0 = a0 << 10;
        v1 = a0 << 5;
        v0 = v0 | v1;
        a0 = a0 | v0;

        *p0 = a2;
        *p1 = a0;
        p1 += 2;
        p0 += 2;
    } while (i < 0x4B00);
}

void func_actor_460200_80132090(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

void func_actor_460200_801320E0(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_460200_80132124(void)
{
    s32          i;
    u32          maskR;
    u32          maskG;
    u32          maskB;
    u32          maskAll;
    u32*         p0;
    u32*         p1;
    u32          a0;
    u32          a2;
    u32          a1;
    register u32 v0 asm("v0");
    register u32 v1 asm("v1");

    p0      = Fs_ImgBuffers->buffers[0];
    i       = 0;
    maskR   = 0x001F001F;
    maskG   = 0x03E003E0;
    maskB   = 0x1F001F00;
    maskAll = 0x1F1F1F1F;
    p1      = p0 + 1;

    do {
        i += 1;
        a0 = *p1;
        a2 = *p0;

        v1 = (a0 & maskR) << 8;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 1;
        a1 = v0 + v1;

        v1 = (a0 & maskG) << 3;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        v0 = v1 << 2;
        a1 = a1 + v0;

        a0 = a0 >> 2;
        v1 = a0 & maskB;
        a2 = a2 >> 5;
        v0 = a2 & maskR;
        v1 = v1 | v0;
        a1 = a1 + v1;

        v0 = a1 >> 3;
        a1 = v0 & maskAll;
        a1 = maskAll - a1;

        a2 = a1 & maskR;
        v0 = a2 << 10;
        v1 = a2 << 5;
        v0 = v0 | v1;
        a2 = a2 | v0;

        a0 = a1 & maskB;
        a0 = a0 >> 8;
        v0 = a0 << 10;
        v1 = a0 << 5;
        v0 = v0 | v1;
        a0 = a0 | v0;

        *p0 = a2;
        *p1 = a0;
        p1 += 2;
        p0 += 2;
    } while (i < 0x4B00);
}

void func_actor_460200_80132204(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_460200_80132210(void)
{
    Task* slot;

    slot = (Task*)Gp_LookupSlot4(0);
    if (slot != NULL) {
        Gp_DispatchMsg(slot, 0x7D4, (s32)&D_actor_460200_80136234, 0);
        Gp_DispatchMsg(slot, 0x7D3, (s32)&D_actor_460200_8013607C, 0);
    }
    if (Gp_LookupSlot4(1) != 0) {
        Gp_MsgSlot4Chain(1, 2);
    }
    slot = (Task*)Gp_LookupSlot4(2);
    if (slot != NULL) {
        Gp_MsgSlot4Chain(2, 1);
        Gp_DispatchMsg(slot, 0x7D3, (s32)&D_actor_460200_80135F14, 0);
    }
}

void func_actor_460200_801322B8(void)
{
    switch (GameFlag_GetNibble(0x114)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137AA0, 0);
            GameFlag_SetNibble(0x114, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137BA8, 0);
            GameFlag_SetNibble(0x114, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80137CB0, 0);
            GameFlag_SetNibble(0x114, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80137DA0, 0);
            break;
    }
}

void func_actor_460200_80132390(void)
{
    switch (GameFlag_GetNibble(0x115)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137F98, 0);
            GameFlag_SetNibble(0x115, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137FE0, 0);
            GameFlag_SetNibble(0x115, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80138028, 0);
            GameFlag_SetNibble(0x115, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80138070, 0);
            break;
    }
}

/// Per-frame step of the actor's first state: rolls `idx` from the shared
/// 12-entry byte table, parks the model's `idx`-th sub-coordinate (`sub`) for
/// the effect spawn below, and pushes the enemy's own coordinate through
/// `Gp_UpdateCoord` so `func_800D7A9C` can place the light probe at the model's
/// updated world position (0x320 above the origin's Y).
///
/// The effect fires only while the paired work block is alive, the model is
/// visible (`field_C` bit 7 clear) and the sub-model exists, and only on every
/// other frame — `killCountdown` doubles as the parity counter and is bumped
/// after the spawn, never on the odd frames the guards reject.
///
/// Both `Gp_SpawnEff` arguments come from one step of the global LCG
/// (`Gp_LcgState`): the low half of the first state feeds `arg2`'s effect id
/// and the top bit of the second its palette selector, and the second state is
/// stored back.
void func_actor_460200_80132468(GpEnemy* enemy, Task* task)
{
    Actor160600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;
    VECTOR           vec;
    u8               idx;
    s32              r;
    u32              rng;
    u32              rng2;
    u32              hi;

    obj   = task->extra.tmd;
    coord = obj->coords;
    r     = rand();
    idx   = D_actor_460200_8013FCCC[(r * 11) >> 15];
    work  = (Actor160600Work*)task->work;
    sub   = task->extra.tmd->coords + idx;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_801325FC(task);
    func_actor_460200_80132978(task);
    if ((work->effects != 0) && !(obj->flags & 0x80) && (obj->buffer != NULL)) {
        if (task->killCountdown & 1) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            hi          = (rng >> 16) & 0x10FF;
            rng2        = rng * 5 + 0x71357911;
            Gp_LcgState = rng2;
            Gp_SpawnEff(0x60070, sub, hi + 0x800231C0 + (((rng2 >> 16) & 1) << 30), NULL);
        }
        task->killCountdown = (u16)task->killCountdown + 1;
    }
}

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 12 units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA when it runs out, then ticks the slots.
void func_actor_460200_801325FC(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        func_actor_460200_80132AC8(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_460200_80132A50(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_460200_80132A04(task);
        return;
    }
}

void func_actor_460200_801327B4(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_460200_80132808, func_actor_460200_80132468 };

    fns[task->state](task->spawnArg2, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80132950` exit path
/// hands it back to `Gp_DestroyEnemy`: it allocates the 0x4F8 work block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`field_20`
/// plus the animation state below), parks the enemy in `Actor160600Work::enemy`
/// and runs the step body `func_actor_460200_801325FC` once in state 2.
///
/// Same body as `func_actor_460200_801338C0` with a different exit callback,
/// animation bank and initial clip (`animId` 0xA rather than 2).
void func_actor_460200_80132808(GpEnemy* enemy, Task* task)
{
    Actor160600Work* work;
    void*            workMem;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    MATRIX*          mtx;
    VECTOR           vec;

    obj     = task->extra.tmd;
    coord   = obj->coords;
    workMem = memCalloc(0x4F8, 0);
    work    = (Actor160600Work*)workMem;
    if ((task->work = (TaskIdMap*)work) == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_460200_80132950;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    work->st.animId              = 0xA;
    work->enemy                  = enemy;
    mtx                          = &work->light;
    obj->lightMtx                = mtx;
    obj->colorMtx                = mtx + 1;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, &D_actor_460200_8013FC8C, obj, work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = &D_actor_460200_8013FC50;
    func_actor_460200_801325FC(task);
    task->state += 1;
}

void func_actor_460200_80132950(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack.
void func_actor_460200_80132978(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13.
void func_actor_460200_80132A04(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
void func_actor_460200_80132A50(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_460200_80132AC8(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: start animation `args->field_4` on this actor.
///
/// `withArg` selects between the two start paths `func_actor_460200_801325FC`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_460200_80132B2C(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->field_4 >= 0x10) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
    func_actor_460200_801325FC(task);
    return 0;
}

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the enemy task it was paired with. `flags` bit 0 hides both
/// models (`TmdObject::flags` = 0) and its absence restores the default
/// 0x80; bit 1 additionally ORs in 0x4, the same bit `Tmd_Create` sets for its
/// own `flags & 1`. With no enemy paired (`Task::spawnArg1` == 0) the actor
/// drives its own model twice.
s32 func_actor_460200_80132B98(Task* task, s32 arg1, s32 flags)
{
    Actor160600Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = task->extra.tmd;
    work = (Actor160600Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = work->pairTask->extra.tmd;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }
    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_460200_80132C14(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode: raise the work block's `effects`, which lets the per-frame
/// state spawn its effect, when the payload is exactly 1. Any other payload is
/// ignored and leaves the flag as it was.
s32 func_actor_460200_80132C8C(Task* task, s32 arg1, GpCmdArg* args)
{
    Actor160600Work* work;
    u16              value;

    value = args->command;
    work  = (Actor160600Work*)task->work;
    if (value == 1) {
        work->effects = value;
    }
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 12, for the step
/// body to walk off.
s32 func_actor_460200_80132CAC(Task* task, s32 arg1, GpXformArg* target)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

void func_actor_460200_80132D74(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor161500Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = (Actor161500Work*)memCalloc(0x4FC, false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_460200_8013322C;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    if (task->spawnArg1 != 0) {
        spawned = Gp_SpawnEnemyFromTable(D_actor_460200_80148118, 1, 0, enemy);
        Task_Reparent(task, spawned->task);
        work->pairTask  = spawned->task;
        work->st.animId = 2;
    } else {
        work->st.animId = 1;
    }
    work->turnUp     = 0;
    work->turnWeight = 0;
    obj->lightMtx    = &work->light;
    obj->colorMtx    = &work->color;
    vec.vx           = coord->workm.t[0];
    vec.vy           = coord->workm.t[1] - 0x320;
    vec.vz           = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_460200_80148130, obj,
                  work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = D_actor_460200_801480E8;
    func_actor_460200_80132F0C(task);
    task->state += 1;
}

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 0x1E units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA and to state 1 when it runs out, then ticks the slots.
void func_actor_460200_80132F0C(Task* task)
{
    Actor161500Work* work;
    s16              animId;

    work = (Actor161500Work*)task->work;
    if (work->st.state == 1) {
        func_actor_460200_801333A4(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_460200_8013332C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0x1E);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->st.state  = 1;
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_460200_801332E0(task);
        return;
    }
}

void func_actor_460200_801330C8(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_460200_80132D74, func_actor_460200_8013311C };

    fns[task->state](task->spawnArg2, task);
}

/// Per-tick state 1 of this actor: faces the model toward the `gameGetPtrSlot(3)`
/// task. The root coordinate of the model is updated, a copy of its translation
/// lifted by 0x320 is used as the look-at point, and the work block's
/// `turnWeight` rate is stepped +0x200 or -0x200 per tick depending on
/// `turnUp`, clamped to 0x1000 and 0 respectively.
void func_actor_460200_8013311C(GpEnemy* enemy, Task* task)
{
    Actor161500Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    VECTOR           vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = (Actor161500Work*)task->work;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_80132F0C(task);
    if (work->turnUp == 1) {
        work->turnWeight += 0x200;
        if (work->turnWeight > 0x1000) {
            work->turnWeight = 0x1000;
        }
    } else {
        work->turnWeight -= 0x200;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, work->turnWeight);
    func_actor_460200_80133254(task);
}

void func_actor_460200_8013322C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack.
void func_actor_460200_80133254(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13.
void func_actor_460200_801332E0(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
void func_actor_460200_8013332C(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_460200_801333A4(Task* task)
{
    Actor161500Work* work;
    s32              i;

    work = (Actor161500Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: start animation `args->field_4` on this actor, rejecting ids
/// of 0xC and above. State 1 (via `func_actor_460200_801333A4`) carries
/// `args->field_C`; state 2 (via `func_actor_460200_8013332C`) does not.
///
/// The `SOFT_BARRIER` is the same codegen pin as in
/// `func_actor_460200_80132B2C`: without it the delay slot of the `beqz` fills
/// from the fall-through arm (`state = 1`) instead of the else arm's
/// `state = 2`.
s32 func_actor_460200_80133408(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor161500Work* work;

    work = (Actor161500Work*)task->work;
    if (args->field_4 >= 0xC) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
    func_actor_460200_80132F0C(task);
    return 0;
}

/// Script opcode: set the visibility flags of this actor's model and of the
/// model of the partner task its spawn routine parked in `pairTask`. `flags`
/// bit 0 hides both models (`TmdObject::flags` = 0) and its absence restores
/// the default 0x80; bit 1 additionally ORs in 0x4. With no partner spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 func_actor_460200_80133474(Task* task, s32 arg1, s32 flags)
{
    Actor161500Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = task->extra.tmd;
    work = (Actor161500Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = work->pairTask->extra.tmd;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }
    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_460200_801334F0(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor161500Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Script opcode: set the work block's `turnUp`, which selects whether the
/// per-frame state blends the model toward the `gameGetPtrSlot(3)` task or away
/// from it, to the payload.
s32 func_actor_460200_80133568(Task* task, s32 arg1, GpCmdArg* args)
{
    ((Actor161500Work*)task->work)->turnUp = args->command;
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 30, for the walk
/// that follows.
s32 func_actor_460200_80133580(Task* task, s32 arg1, GpXformArg* target)
{
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor161500Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}

/// Per-frame task of this actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the matrix pair at the front of the
/// parent's work block and hangs its coordinate off the parent model's eighth
/// coordinate; every frame marks the coordinate dirty.
void func_actor_460200_8013364C(Task* task)
{
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra.tmd;
    GsCOORDINATE2* coord  = obj->coords;
    GsCOORDINATE2* sub    = &parent->extra.tmd->coords[7];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 12 units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA when it runs out, then ticks the slots.
void func_actor_460200_801336B4(Task* task)
{
    Actor160600Work* work;
    s16              animId;

    work = (Actor160600Work*)task->work;
    if (work->st.state == 1) {
        func_actor_460200_80133C00(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_460200_80133B88(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_460200_80133B3C(task);
        return;
    }
}

void func_actor_460200_8013386C(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_460200_801338C0, func_actor_460200_80133A04 };

    fns[task->state](task->spawnArg2, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80133A88` exit path
/// hands it back to `Gp_DestroyEnemy`: it allocates the 0x4F8 work block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`field_20`
/// plus the animation state below), parks the enemy in `Actor160600Work::enemy`
/// and runs the step body `func_actor_460200_801336B4` once in state 2.
void func_actor_460200_801338C0(GpEnemy* enemy, Task* task)
{
    Actor160600Work* work;
    void*            workMem;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    MATRIX*          mtx;
    VECTOR           vec;

    obj     = task->extra.tmd;
    coord   = obj->coords;
    workMem = memCalloc(0x4F8, 0);
    work    = (Actor160600Work*)workMem;
    if ((task->work = (TaskIdMap*)work) == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_460200_80133A88;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    work->st.animId              = 2;
    mtx                          = &work->light;
    obj->lightMtx                = mtx;
    obj->colorMtx                = mtx + 1;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, &D_actor_460200_80151538, obj, work->rig.poses, work->rig.slots);
    work->st.state = 2;
    task->msgTable = &D_actor_460200_801514FC;
    func_actor_460200_801336B4(task);
    task->state += 1;
}

void func_actor_460200_80133A04(GpEnemy* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_801336B4(task);
    func_actor_460200_80133AB0(task);
}

void func_actor_460200_80133A88(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack.
void func_actor_460200_80133AB0(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x13.
void func_actor_460200_80133B3C(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets animation slots 1..0x13 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
void func_actor_460200_80133B88(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x13 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
void func_actor_460200_80133C00(Task* task)
{
    Actor160600Work* work;
    s32              i;

    work = (Actor160600Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

s32 func_actor_460200_80133C64(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor160600Work* work;

    work = (Actor160600Work*)task->work;
    if (args->field_4 >= 0x12) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
    func_actor_460200_801336B4(task);
    return 0;
}

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the enemy task it was paired with. `flags` bit 0 hides both
/// models (`TmdObject::flags` = 0) and its absence restores the default
/// 0x80; bit 1 additionally ORs in 0x4. With no enemy paired
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
///
/// Same body as `func_actor_460200_80132B98`, the other 0x4F8-block actor's
/// visibility opcode.
s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags)
{
    Actor160600Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = task->extra.tmd;
    work = (Actor160600Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = work->pairTask->extra.tmd;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }
    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_460200_80133D4C(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_460200_80133DC4(void)
{
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 12, for the step
/// body to walk off.
s32 func_actor_460200_80133DCC(Task* task, s32 arg1, GpXformArg* target)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor160600Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
