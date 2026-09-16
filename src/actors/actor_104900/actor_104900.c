#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "main/gfx.h"

extern u32 Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80131F08);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132250);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", D_actor_104900_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", D_actor_104900_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_8013279C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132B10);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132D78);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80133BB8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80134780);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135404);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135560);

/// Spin-about handler: on the frame the latch at 0xBA8 is still clear it draws
/// a nibble from `Gp_LcgState` and arms one of the six spin rates - the
/// 0x200 / 0x400 / 0x600 triple, negative on odd draws - into the countdown at
/// 0xB8C, then acts its motion 4. Every later frame turns the model's yaw at
/// 0x46 by 0x10 towards that countdown, rebuilds the Y rotation over the pose
/// and clears `flg`, and when the countdown reaches zero it drops the state at
/// 0xBA7 and the latch, ending the spin about.
void func_actor_104900_801356BC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    GpCoordPose* pose;
    s32          idx;
    u32          rng;
    u16          angle;

    pose = (GpCoordPose*)((TmdObject*)task->extra)->field_8;
    if (work->field_BA8 == 0) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        idx         = (rng >> 0x10) & 0xF;
        if (idx < 3) {
            work->field_B8C = 0x200;
        } else if (idx < 6) {
            work->field_B8C = -0x200;
        } else if (idx < 9) {
            work->field_B8C = 0x400;
        } else if (idx < 0xC) {
            work->field_B8C = -0x400;
        } else if (idx < 0xE) {
            work->field_B8C = 0x600;
        } else {
            work->field_B8C = -0x600;
        }
        work->field_BA4 = 4;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (work->field_B8C > 0) {
        angle          = ((u16)pose->field_46 - 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C - 0x10;
    } else {
        angle          = ((u16)pose->field_46 + 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C + 0x10;
    }
    if (work->field_B8C == 0) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801357F0);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801359CC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135FDC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137B1C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137C88);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137FB8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", ActorsShared8013845cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80138374);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", ActorsShared80138404Table);
