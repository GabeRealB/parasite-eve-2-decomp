#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b6_nursery.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
extern TaskDesc      D_shelter_b6_nursery_80184FDC;
extern s8            D_shelter_b6_nursery_80185034[];
extern s32           D_shelter_b6_nursery_8018797C;
extern GsCOORDINATE2 D_shelter_b6_nursery_801879A0;
extern u8            D_8007216C;
extern void          func_80132028(void);
extern void          func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1);

s32 func_shelter_b6_nursery_8017FA54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 flag;

    if (arg2 == 0xA) {
        D_shelter_b6_nursery_8018797C          = 0;
        D_shelter_b6_nursery_80187980.field_4  = 0x55160002;
        D_shelter_b6_nursery_80187980.field_8  = 0x55160005;
        D_shelter_b6_nursery_80187980.field_10 = 0x55160003;
        D_shelter_b6_nursery_80187980.field_C  = 0x55160004;
        flag                                   = GameFlag_GetNibble(0xC7);
        if (flag == 1) {
            if (GameFlag_GetNibble(0x83) != 0) {
                Gp_SetBit2Flag(0x22, 1, 4);
            }
            func_800E3FAC(0xA2, 0x31);
            GameFlag_SetNibble(0xC7, 2);
            D_shelter_b6_nursery_80187980.field_0 = 6;
            D_shelter_b6_nursery_80187980.field_1 = 0xB;
            D_shelter_b6_nursery_80187980.field_3 = 0;
            D_shelter_b6_nursery_80187980.field_2 = flag;
            Task_SpawnFromTable(&D_shelter_b6_nursery_80184FDC, 0, 0x19,
                                (s32)&D_shelter_b6_nursery_80187980);
            func_80132028();
            func_shelter_b6_nursery_80182D14(0, 0);
            return 0;
        }
        if (GameFlag_GetNibble(0x160) == 0) {
            Gp_SpawnIfCapIdle(0x17, 0);
            GameFlag_SetNibble(0x160, 1);
            return 0;
        }
        D_shelter_b6_nursery_80187980.field_0 = 6;
        D_shelter_b6_nursery_80187980.field_1 = 0x16;
        D_shelter_b6_nursery_80187980.field_3 = 0;
        D_shelter_b6_nursery_80187980.field_2 = 0;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80184FDC, 0, 0xA,
                            (s32)&D_shelter_b6_nursery_80187980);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_2", func_shelter_b6_nursery_8017FBC0);
