#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

void func_mist_parking_801837A4(s32 arg0);
void func_mist_parking_8018471C(s32 arg0);

extern u8 D_8007216D;
extern u8 D_80072170;

extern s32 D_mist_parking_80186BB8;
extern s32 D_mist_parking_80186C5C;
extern s32 D_mist_parking_80186DC4;
extern s32 D_mist_parking_8018DF34;
extern s32 D_mist_parking_8018EDBC;
extern s32 D_mist_parking_8018EFE4;

s32 func_mist_parking_801826E8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        func_800E8614((s32)&D_mist_parking_80186C5C, 1);
    }
    if (arg2->field_2 == 2) {
        func_800E8614((s32)&D_mist_parking_80186DC4, 1);
        GameFlag_SetNibble(0xED, 1);
    }
    return 1;
}

void func_mist_parking_80182750(s32 arg0)
{
    if (GameFlag_GetNibble(0x7A) != 0) {
        arg0 += 2;
    }
    D_8007216D             = arg0;
    Game_Session->field_5  = arg0;
    Game_Session->field_76 = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    Gp_SpawnIfCapIdle(arg0, 0);
}

void func_mist_parking_801827C0(Task* arg0)
{
    arg0->field_24 = &D_mist_parking_80186BB8;
    Game_SetPtrSlot(arg0, 7);
    if ((Game_Session->field_9 == 2) && (GameFlag_GetNibble(0xF1) == 0)) {
        if (D_80072170 == 3) {
            func_800E3FAC(0xA2, 0x3C);
            func_mist_parking_801837A4(0);
            func_800E8634((s32)&D_mist_parking_8018DF34, 0, (s32)&D_mist_parking_8018EDBC);
        } else {
            func_mist_parking_8018471C(0);
            func_800E8614((s32)&D_mist_parking_8018EFE4, 1);
        }
    }
    arg0->state = arg0->state + 1;
}
