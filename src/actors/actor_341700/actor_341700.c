#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 func_actor_341700_80168178();
s32 func_actor_341700_80168468();

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80162070);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801624F8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801626C4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80162974);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80162B8C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E24);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E3C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80162DCC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163268);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801633D4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163600);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801639C0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163AF0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163C58);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163E58);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80163FBC);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E90);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161E9C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161EA8);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161ED0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801640F8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801649DC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80164B68);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80164CDC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80164E9C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165008);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801651E0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165388);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016583C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165984);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165AF0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165C70);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161F70);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161F7C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161F88);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161F94);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80165DDC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80166114);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801663F0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80166568);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801666F0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016688C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801669F8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80166B94);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80166D2C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80166E90);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801670B0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016724C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80167744);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80167890);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700", D_actor_341700_80161FCC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80167C30);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80167E18);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168004);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168124);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168178);

void func_actor_341700_801681C4(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168234);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801682DC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016833C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168370);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168444);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168468);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801684A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016852C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016859C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801685F0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168684);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168698);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801686AC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801686C0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168748);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801687B4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168820);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168874);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801688C8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016891C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801689A0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168A14);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168A48);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168AC0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168B40);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168BE0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168C4C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168D3C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168DA0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168EA0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168F5C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80168F9C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169018);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016908C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801691B0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169218);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169254);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169380);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169440);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169520);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801695A0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016966C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801696C8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801696E0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169724);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801697B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_801697D4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169888);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016999C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169AB0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169B40);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169BC8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169C50);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169CC4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169D54);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169DBC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169E20);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169EE4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169F38);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_80169FB0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A058);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A08C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A0E0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A130);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A1A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A21C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A2CC);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A460);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A568);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A630);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A6C0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A758);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A810);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A890);

void func_actor_341700_8016A8EC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A8F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016A98C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016AA58);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016AAB4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016ABF4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016AC0C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016AC64);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016C0F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016CC9C);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2)
{
    TmdObject* obj = arg0->field_2C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->field_C |= 4;
            break;
        case 3:
            obj->field_C = 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016CEB4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016CF48);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016D018);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016D130);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016D2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016D2E8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700", func_actor_341700_8016D32C);
