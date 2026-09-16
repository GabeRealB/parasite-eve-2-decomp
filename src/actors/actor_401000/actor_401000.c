#include "common.h"

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/session.h"

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801323EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132590);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132824);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132BB0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132EF0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133274);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133940);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133D50);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80134DB4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80134F98);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801352DC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135374);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135704);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135AA4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801365C8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80136E20);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801374D4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801378DC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801380B8);

void func_actor_401000_801383F0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpAnimArg*       msg;
    GpEnemy*         enemy;
    Task*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401000_80154F1C;
        msg->field_4    = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = Game_GetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x100;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
        work->field_0 = 0xE;
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801385B0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801388F4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80138BB4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80138D08);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80138F50);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013922C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801394EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80139D10);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A0C8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A5F0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A930);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013B1E4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013B61C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013C46C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013CD9C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013CEF0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013D044);

void func_actor_401000_8013D68C(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401000/actor_401000", ActorsShared80135df4Table);

s32 func_actor_401000_8013D694(Actor401000* arg0, s32 arg1, Actor401000Msg* arg2)
{
    Actor401000Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// The four bytes of `.rodata` that sit between this function's jump table and
/// `func_actor_401000_8013DF6C`'s. Both tables came out of one translation unit,
/// where GCC's `.align 3` ahead of the second left this gap; the split puts the
/// second table in `actor_401000_2`, so the gap has to be materialised here to
/// keep the leading rodata the length the package says it is.
const u32 D_actor_401000_801320A0 SECTION(".rodata") = 0;
