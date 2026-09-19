#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/rand.h"

extern s8 D_8007272D;

extern s32 D_actor_460200_8013FC50;
extern s32 D_actor_460200_8013FC8C;

extern u8 D_actor_460200_8013FCCC[];

extern s32 Gp_LcgState;

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

void func_actor_460200_80131E2C(Task* task)
{
    Actor460200CaptureArgs* args;
    s32                     hoisted; /* $v0: a byte load GCC hoisted above the prologue */
    s32                     i;
    u32*                    strip;

    args = task->spawnArg2;
    if (hoisted == 0) {
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
    Actor460200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;
    VECTOR           vec;
    u8               idx;
    s32              r;
    u32              rng;
    u32              rng2;
    u32              hi;

    obj   = task->extra;
    coord = obj->coords;
    r     = rand();
    idx   = D_actor_460200_8013FCCC[(r * 11) >> 15];
    work  = (Actor460200Work*)task->work;
    sub   = ((TmdObject*)task->extra)->coords + idx;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_460200_801325FC(task);
    func_actor_460200_80132978(task);
    if ((work->field_4EE != 0) && !(obj->flags & 0x80) && (obj->buffer != NULL)) {
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

void func_actor_460200_80132AC8(Task* task);
void func_actor_460200_80132A50(Task* task);
void func_actor_460200_80132A04(Task* task);

void func_actor_460200_801325FC(Task* task)
{
    ActorsShared8014c874Work* work;
    s16                       animId;

    work = (ActorsShared8014c874Work*)task->work;
    if (work->state == 1) {
        func_actor_460200_80132AC8(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_460200_80132A50(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            ActorsShared8014c874_MoveForward(((TmdObject*)task->extra)->coords, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_460200_80132A04(task);
        return;
    }
}

void func_actor_460200_80132808(GpEnemy* enemy, Task* task);
s32  func_actor_460200_80132978(Task* task);
void func_actor_460200_80132950(Task* task);

void func_actor_460200_801327B4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132808, func_actor_460200_80132468 };

    fns[task->state](task->spawnArg2, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80132950` exit path
/// hands it back to `Gp_DestroyEnemy`: it allocates the 0x4F8 work block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`field_20`
/// plus the animation state below), parks the enemy in `Actor460200Work::enemy`
/// and starts state 2, the visibility opcode `func_actor_460200_801325FC` runs.
///
/// Same body as `func_actor_460200_801338C0` with a different exit callback,
/// animation bank and initial clip (`animId` 0xA rather than 2).
void func_actor_460200_80132808(GpEnemy* enemy, Task* task)
{
    Actor460200Work* work;
    void*            workMem;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    MATRIX*          mtx;
    VECTOR           vec;

    obj     = task->extra;
    coord   = obj->coords;
    workMem = memCalloc(0x4F8, 0);
    work    = (Actor460200Work*)workMem;
    if ((task->work = (TaskIdMap*)work) == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_460200_80132950;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    obj->flags           = 0;
    work->animId         = 0xA;
    work->enemy          = enemy;
    mtx                  = (MATRIX*)work;
    obj->lightMtx        = mtx;
    obj->colorMtx        = mtx + 1;
    vec.vx               = coord->workm.t[0];
    vec.vy               = coord->workm.t[1] - 0x320;
    vec.vz               = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, &D_actor_460200_8013FC8C, obj, &work->slots[0x14], work->slots);
    work->state    = 2;
    task->msgTable = &D_actor_460200_8013FC50;
    func_actor_460200_801325FC(task);
    task->state += 1;
}

void func_actor_460200_80132950(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132978);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132A04);

void func_actor_460200_80132A50(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->work;
    i    = 1;
    do {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132AC8);

/// Script opcode: start animation `args->animId` on this actor.
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
s32 func_actor_460200_80132B2C(Task* task, s32 arg1, Actor460200AnimArgs* args)
{
    Actor460200Work* work;

    work = (Actor460200Work*)task->work;
    if (args->animId >= 0x10) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
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
    Actor460200PairedWork* work;
    TmdObject*             self;
    TmdObject*             other;

    self = (TmdObject*)task->extra;
    work = (Actor460200PairedWork*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F0->extra;
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

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C14);

INCLUDE_RODATA("actors/nonmatchings/actor_460200/actor_460200", D_actor_460200_80131E20);
