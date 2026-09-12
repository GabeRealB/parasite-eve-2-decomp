#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_actor_444000_80144A68;
extern s32 Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80134040);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801341C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013441C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80134688);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80135448);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801371E8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80137594);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013799C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80137D4C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801381B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138490);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801389EC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138B94);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80138FC4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013928C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139594);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139AF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139C80);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80139EE4);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A3AC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A77C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013A958);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013ACD0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013AFF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013C060);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013C4B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D128);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D810);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013D96C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8013FB74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801404C0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80140BBC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80140E28);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_8014105C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801411C8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80141DFC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80142254);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80142F28);
