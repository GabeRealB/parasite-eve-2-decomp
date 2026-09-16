#include "common.h"

#include "actors/actor_401800.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801323D4);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013271C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80132C68);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80132E0C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133558);

/// Per-frame animation tick: walk the actor's pose slots, copy each slot's
/// state byte down by three and, for the first ten slots, blend the pose pair
/// `func_800B3448` builds out of the pose and blend contexts — weighted by
/// `field_8AC` against its `0x1000` complement. Slots `0xB` and up only carry
/// the state byte and are advanced by `Gp_AnimTickIndex`.
/// Same body as `func_actor_401000_80132A84`.
void func_actor_401800_801337EC(Actor401800* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor401800AnimWork* work;

    work   = (Actor401800AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AA;
            work->slots[i].field_9      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133918);

INCLUDE_RODATA("actors/nonmatchings/actor_401800/actor_401800", D_actor_401800_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133B78);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133EB8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013423C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801348A8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80134C94);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80135DAC);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80135F58);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013629C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80136560);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80136EAC);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80137714);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80137DDC);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801381E4);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80138C28);

/// On the live-actor flag, raises the three animation slots, sends the `0x3FF`
/// animation record and the `0x3F9` object pair to the `Game_GetPtrSlot(3)`
/// task, then spawns the 5/0xFF/8 pad-lerp. On work flag bit 0, restarts the
/// actor's model (`field_0 = 0xE`, the 0x8B8 effect record for the second
/// coordinate) and finally copies the `field_5A` clip id into `field_894` and
/// rebuilds the four coordinate parts the actor draws from.
void func_actor_401800_80138F5C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401800_80155A0C;
        msg->field_4    = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = Game_GetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_0           = 0xE;
        work->field_8B8.field_0 = &arg0->field_2C->field_8[1];
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], 0, &work->field_8B8);
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80139118);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013945C);

/// Per-frame body of the live actor: arms the animation slots and the two
/// `field_8C8` / `field_A08` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `field_0` from the
/// enemy's HP sign and its `field_4C` bit 1. Same body as `Actor01900_Fn09BE8`.
void func_actor_401800_8013971C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0xB;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80139870);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801399C4);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80139B18);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80139D60);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013A034);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013A2E8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013AB64);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013AF1C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013B444);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013B784);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013BB10);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013BF48);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013CD98);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013D64C);

void func_actor_401800_8013DCB4(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401800/actor_401800", ActorsShared80135df4Table);

/* func_actor_401800_8013DCBC (the 0x7D3 message handler) lives in
 * actor_401800_2.c: the jump table GCC emits for its switch is what owns the
 * rodata run at 0x254, so the function belongs to the unit that starts there. */
