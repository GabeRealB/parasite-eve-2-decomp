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

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801337EC);

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

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013971C);

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

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013DCBC);
