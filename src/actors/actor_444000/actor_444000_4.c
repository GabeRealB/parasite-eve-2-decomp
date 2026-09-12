#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F30`:
/// allocate its `Actor444000SpinnerWork`, parent the model object to the world
/// coordinate, give it a random orientation off `Gp_LcgState`, point it at its
/// own light and colour matrices and step the task on. Bails to
/// `Gp_DestroyEnemy` when the overlay is shutting down or the allocation fails.
void func_actor_444000_8013A1C4(GpEnemy* enemy, Actor444000Spinner* task)
{
    Actor444000SpinnerWork* work;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work           = Mem_Calloc(0xA0, 0);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->field_8->sub = &Gfx_ViewCoord;
    task->extra->field_C      = 0;

    switch ((u16)task->spawnArg1) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->field_24 = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    task->extra->field_1C     = &work->lightMtx;
    task->extra->field_20     = &work->colorMtx;
    task->extra->field_8->flg = 0;
    Gp_UpdateCoord(task->extra->field_8);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->field_8->workm.t, 0, 3);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013A3AC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013A77C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013A958);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013ACD0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013AFF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013C060);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013C4B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013D128);

/// Reset/teardown handler: when the work block is asking for a reset, arm the
/// re-spawn sequence and push the host's model flag word onto each of the seven
/// escorts' models. Otherwise run the ordinary re-arm while the sub-state
/// counter is still below 0xA, and once it reaches 2 release the host's and
/// every escort's model buffers.
void func_actor_444000_8013D810(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* dying;
    s16              i;
    s16              j;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        escorts                            = arg0->field_1C;
        work->field_7F3                    = 3;
        ((TmdObject*)arg0->extra)->field_C = 0x80;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        work->field_7B3 = 0xA;
        work->field_7B0 = 2;
        work->field_6   = 0;
        work->field_7B6 = 0x10;
        func_actor_444000_8013441C(arg0);
    } else {
        if (work->field_6 < 0xA) {
            func_actor_444000_8013441C(arg0);
        }
        if (work->field_6 == 2) {
            dying = arg0->field_1C;
            Tmd_FreeBuffers((TmdObject*)arg0->extra);
            for (j = 0; j < 7; j++) {
                if (dying->field_ECC[j] != NULL) {
                    Tmd_FreeBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
                }
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013D96C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_8013FB74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_801404C0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80140BBC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80140E28);

void func_actor_444000_8014105C(Actor444000* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              state;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        tmd               = (TmdObject*)arg0->extra;
        obj               = arg0->field_20;
        obj->node.field_4 = 8;
        tmd->field_C      = 0;
        state             = work->field_7B3;
        work->field_EF4   = 0;
        work->field_EF6   = 0;
        work->field_EFA   = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->field_8 >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    if (D_actor_444000_80144A70 >= 0x191) {
        work->field_7A4         = 0;
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 0xA;
    }
    SCRATCH_SP += 0xC;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_801411C8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80141DFC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80142254);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_80142F28);
