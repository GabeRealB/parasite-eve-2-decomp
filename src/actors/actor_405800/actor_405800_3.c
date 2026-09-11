#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8009EA50(s16 arg0);

/* Defined in another unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_405800_80135E28(Task* arg0);
s32  func_actor_405800_80136A1C(Task* arg0);
s32  func_actor_405800_80136B94(Task* arg0);
void func_actor_405800_80137948(Task* task);
void func_actor_405800_801383CC(Task* arg0, SVECTOR* arg1, s16 arg2);
s32  func_actor_405800_801385F4(Task* arg0);
void func_actor_405800_80139358(Task* arg0);
void func_actor_405800_801393E8(Task* arg0);
void func_actor_405800_801394E4(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);

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

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137C78);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137CEC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137D60);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137DE4);

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
        func_actor_405800_801383CC(task, &work->field_A8, 0x18);
        func_actor_405800_80135E28(task);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137EF0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137F58);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80137FCC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80138040);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_801380C0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80138154);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_801381BC);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80138224);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_80138294);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_8013836C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_3", func_actor_405800_801383CC);
