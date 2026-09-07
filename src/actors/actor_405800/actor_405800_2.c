#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_405800.h"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where a
 * `SOFT_BARRIER()` was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

/* Model streams in this overlay's own data. */
extern u8 D_actor_405800_8013FB18[];
extern u8 D_actor_405800_8014086C[];
extern u8 D_actor_405800_80140F10[];
extern u8 D_actor_405800_80141430[];

/* Defined in another unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_405800_801390FC(Task* arg0);
void func_actor_405800_80139188(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80136388);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80136A1C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80136B94);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80136CE0);

void func_actor_405800_80136E14(Task* task)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    GpEffWork* eff4;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* dst4;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;
    TmdObject* src4;

    D_800678F0[0] = D_actor_405800_8013FB18;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[5], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)task->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_405800_8014086C;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)task->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_405800_80140F10;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)task->extra;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    D_800678F0[0] = D_actor_405800_80141430;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4           = (TmdObject*)task->extra;
        dst4           = (TmdObject*)eff4->field_0->extra;
        dst4->field_24 = src4->field_24;
        dst4->field_25 = src4->field_25;
        if (dst4->field_18 != NULL) {
            Tmd_ProcessStream(dst4);
            Tmd_ProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)task->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_8013706C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_8013728C);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_801373E0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_801375C4);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80137908);

void func_actor_405800_80137948(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_88E = 0;
    work->field_88D = 0;
}

void func_actor_405800_8013795C(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->idMap;
    if (work->field_85C > 0) {
        work->field_85C = work->field_85C - 1;
    }
    if (work->field_85E > 0) {
        work->field_85E = work->field_85E - 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80137994);

void func_actor_405800_801379F8(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_742 = work->field_742 & 0xBFFF;
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80137A14);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80137A60);

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects. The
/// two-entry table is small enough that GCC materialises each callback with its
/// own `lui`/`addiu` pair instead of copying a `.rodata` pool.
void func_actor_405800_80137B34(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->idMap;
    TaskFunc         states[2] = { func_actor_405800_801390FC, func_actor_405800_80139188 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_2", func_actor_405800_80137B9C);
