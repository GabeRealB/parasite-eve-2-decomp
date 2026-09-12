#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"
#include "actors/actors_shared_80139c00.h"

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8009EA50(s16 arg0);

/* Defined in another unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_405800_80133DB0(Task* arg0);
void func_actor_405800_80133F48(Task* arg0);
void func_actor_405800_80134C00(Task* arg0);
void func_actor_405800_801356A8(Task* arg0);
void func_actor_405800_80135E28(Task* arg0);
s32  func_actor_405800_80136A1C(Task* arg0);
s32  func_actor_405800_80136B94(Task* arg0);
s32  func_actor_405800_80136CE0(Task* arg0);
void func_actor_405800_80137948(Task* task);
void func_actor_405800_80139358(Task* arg0);
void func_actor_405800_801393E8(Task* arg0);
void func_actor_405800_801394E4(Task* arg0);
void func_actor_405800_80139550(Task* arg0);
void func_actor_405800_801395E8(Task* arg0);
void func_actor_405800_80139844(Task* arg0);
void func_actor_405800_80139E2C(Task* arg0);
void func_actor_405800_80139E48(Task* arg0);
void func_actor_405800_80139EAC(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);

/* Three-entry tables for `func_actor_405800_80137F58` / `80137FCC`, the
 * five-entry table for `80137D60`, and the four-entry tables for `80137DE4` /
 * `80138040`. A local initializer would emit another compiler pool packed
 * against `80137C04`'s leading table; copying the splat-owned pools keeps
 * them at 0x98 / 0xAC / 0xBC / 0xC8 / 0xD4. */
extern const TaskFuncTable5 D_actor_405800_80131EB8;
extern const TaskFuncTable4 D_actor_405800_80131ECC;
extern const TaskFuncTable3 D_actor_405800_80131EDC;
extern const TaskFuncTable3 D_actor_405800_80131EE8;
extern const TaskFuncTable4 D_actor_405800_80131EF4;

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects. Three
/// callbacks are one too many for GCC to materialise with `lui`/`addiu` pairs,
/// so the initializer becomes this unit's leading `.rodata` pool and the
/// prologue block-copies it onto the stack.
void func_actor_405800_80137C04(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[3] = { func_actor_405800_80139358, func_actor_405800_801393E8,
                                   func_actor_405800_801394E4 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80137C78(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_80139550, func_actor_405800_80133DB0 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->field_848](task);
    }
}

void func_actor_405800_80137CEC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_801395E8, func_actor_405800_80133F48 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->field_848](task);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800_3", D_actor_405800_80131EB8);

void func_actor_405800_80137D60(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->idMap;
    TaskFuncTable5   states = D_actor_405800_80131EB8;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800_3", D_actor_405800_80131ECC);

void func_actor_405800_80137DE4(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->idMap;
    TaskFuncTable4   states = D_actor_405800_80131ECC;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80137E64(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    s16              count;

    work = (Actor405800Work*)task->idMap;
    func_actor_405800_80137948(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0) {
        count           = work->field_87C - 1;
        work->field_87C = count;
        if (count == 0) {
            work2            = (Actor405800Work*)task->idMap;
            work2->field_846 = 0xB;
            work2->field_848 = 0;
            return;
        }
        ActorsShared80139c00(task, &work->field_A8, 0x18);
        func_actor_405800_80135E28(task);
    }
}

void func_actor_405800_80137EF0(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_80139844, func_actor_405800_80134C00 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800_3", D_actor_405800_80131EDC);

void func_actor_405800_80137F58(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->idMap;
    TaskFuncTable3   states = D_actor_405800_80131EDC;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800_3", D_actor_405800_80131EE8);

void func_actor_405800_80137FCC(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->idMap;
    TaskFuncTable3   states = D_actor_405800_80131EE8;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800_3", D_actor_405800_80131EF4);

void func_actor_405800_80138040(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->idMap;
    TaskFuncTable4   states = D_actor_405800_80131EF4;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_801380C0);

void func_actor_405800_80138154(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_80139E2C, func_actor_405800_80139E48 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_801381BC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_80139EAC, func_actor_405800_801356A8 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80138224(Task* task)
{
    Actor405800Work* work;
    s32              i;

    work = (Actor405800Work*)task->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = work->field_850;
        Gp_AnimResetSlot(&work->anim, i, work->field_872);
        i++;
    } while (i < 0x12);
    work->field_870 = work->field_872;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80138294);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_8013836C);
