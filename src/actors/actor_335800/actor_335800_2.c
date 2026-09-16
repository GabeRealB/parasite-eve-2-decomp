#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/3CD8.h"

#include "gameplay/D4.h"

#include "gameplay/gameplay.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/unknown_syms.h"

extern TaskDesc D_actor_335800_80164DE0;

extern TaskDesc D_80182834;

extern s8 D_8007272D;

extern u8 D_8007216D;

extern u8 D_8007216C;

extern GpRec14 D_actor_335800_80164E7C;

extern s32 D_80165FC0;

extern s32 D_80166098;

extern s32 D_actor_335800_80164EBC;

extern s32 D_actor_335800_80164ED4;

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 0, 0, 0);
}

void func_actor_335800_801620F0(u8 arg0)
{
    Game_Session->field_5 = D_8007216D = arg0;
    Game_Session->field_76             = 1;
}

void func_actor_335800_80162114(void)
{
    Task*          slot;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    slot = Game_GetPtrSlot(3);
    if (slot != NULL) {
        extra = slot->extra;
        coord = extra->field_8;
        if ((u32)(coord->coord.t[2] - 0xC53) < 0x96F) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164ED4, 0);
        }
        if ((u32)(coord->coord.t[2] - 0x3E9) < 0x86A) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164EBC, 0);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801621B4);

void func_actor_335800_8016224C(void)
{
    register u8    areaId asm("a0");
    Task*          slot;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    slot = Game_GetPtrSlot(3);
    if (slot != NULL) {
        areaId = 6;
        extra  = slot->extra;
        coord  = extra->field_8;
        if (coord->coord.t[2] >= 0xC53) {
            areaId = 5;
        }
        D_8007216C             = areaId;
        Game_Session->field_4  = areaId;
        Game_Session->field_52 = 1;
        Game_Session->field_76 = 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_801622C0);

void func_actor_335800_80162364(Task* arg0)
{
    if (arg0->state == 0) {
        if (arg0->spawnArg1 != 0) {
            func_800E8614((s32)&D_80166098, 0);
        } else {
            func_800E8614((s32)&D_80165FC0, 0);
        }
        arg0->state += 1;
        return;
    }
    Task_Kill(arg0);
}

void func_actor_335800_801623D8(void)
{
    Task_SpawnFromTable(&D_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    SndEvt_EnqueueType2(D_80062735, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_actor_335800_801624B8(s32 arg0)
{
    GameFlag_SetNibble(0x108, arg0);
}

void func_actor_335800_801624DC(Task* arg0)
{
    Task* slot;

    if (Game_Session->field_126 != 0) {
        slot = Game_GetPtrSlot(3);
        Gp_PlayerWeaponId(&D_actor_335800_80164E7C.field_0);
        Gp_DispatchMsg(slot, 0x3E8, (s32)&D_actor_335800_80164E7C, 0);
        Task_Kill(arg0);
    }
}

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 3, 0, 0);
}

void func_actor_335800_80162588(Task* arg0)
{
    s8  var_a0;
    u8  temp_v1;
    s32 count;

    if ((Game_Session->field_1 != 0) && (Game_Session->field_5F == 0)) {
        temp_v1 = Game_Session->field_13B;
        var_a0  = 0;
        if (temp_v1 & 1) {
            count  = (u16)arg0->killCountdown;
            var_a0 = count & 1;
        }
        if ((temp_v1 & 2) && !(arg0->killCountdown & 1)) {
            var_a0 = -1;
        }
        Display_ClampField126(var_a0);
        arg0->killCountdown += 1;
        return;
    }
    Display_ClampField126(0);
    Task_Kill(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162640);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162844);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162B3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_2", func_actor_335800_80162C80);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E30);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", ActorsShared80132920Offset);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E5C);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E68);
INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800_2", D_actor_335800_80161E78);
