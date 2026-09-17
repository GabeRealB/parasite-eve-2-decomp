#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/sound.h"

extern u16                 D_actor_205200_8014C9CC[];
extern TaskDesc            D_actor_205200_8014CA44;
extern Actor205200SpawnRec D_actor_205200_8015B458;

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_80149E54);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", ActorsShared80131e24Sub0);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", ActorsShared80131e24Sub1);

void func_actor_205200_8014AB98(Actor205200* arg0)
{
    Actor205200Work* work  = arg0->field_1C;
    s32              state = work->field_26;

    switch (state) {
        case 0:
            if ((s16)--work->field_22 <= 0) {
                if (D_actor_205200_8015B458.field_4 == 2) {
                    D_actor_205200_8015B458.field_0 = 0xF;
                    D_actor_205200_8015B458.field_2 = 0xA0;
                    Task_SpawnFromTable(&D_actor_205200_8014CA44, 0, 0, (s32)&D_actor_205200_8015B458);
                    Gp_ArmStateF0(1);
                    work->field_28 = 1;
                    SndEvt_EnqueueType6(((arg0->field_20->field_8 >> 12) << 8) | 0x40340002, 0, 0);
                }
                work->field_22 = 20;
                work->field_26 = 1;
            }
            break;
        case 1:
            if ((s16)--work->field_22 <= 0) {
                D_actor_205200_8015B458.field_4 = state;
                work->field_22                  = D_actor_205200_8014C9CC[work->field_20];
                work->field_26                  = 0;
                Gp_SpendMp(1);
                work->field_28 = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014ACD4);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014AE0C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B048);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B484);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E24);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E30);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", jtbl_actor_205200_80149E40);
