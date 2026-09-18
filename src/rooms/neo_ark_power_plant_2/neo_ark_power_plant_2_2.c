#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80071075;
extern s8 D_8007272D;
extern s8 D_80114C12;

extern s32            D_neo_ark_power_plant_2_801802A8;
extern s32            D_neo_ark_power_plant_2_80180560;
extern GpMsgEntry     D_neo_ark_power_plant_2_801801F8[];
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F70[];
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F94[];

void func_neo_ark_power_plant_2_8017D6F4(Task* arg0)
{
    u8 temp_v1;

    arg0->msgTable = D_neo_ark_power_plant_2_801801F8;
    Game_SetPtrSlot(arg0, 7);
    temp_v1 = gGameSession->at4.loc.place;
    if (temp_v1 == 1) {
        gGameSession->flowFlags = temp_v1;
    }
    arg0->state = arg0->state + 1;
}

void func_neo_ark_power_plant_2_8017D758(void)
{
    s32 temp_v0;

    if (GameFlag_GetNibble(0xDF) == 0) {
        temp_v0 = Gp_LookupSlot4(0);
        if ((temp_v0 != 0) && (Gp_DispatchMsg((Task*)temp_v0, 0x7D6, 0, 0) == 0) && (D_80114C12 != 1) &&
            (D_80071075 == 0)) {
            GameFlag_SetNibble(0xDF, 1);
            GameFlag_SetNibble(0xB9, 1);
            GameFlag_SetNibble(0x1BC, 0);
            Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F70);
            if (GameFlag_GetNibble(0xF3) != 0) {
                Gp_ApplyAreaRecs(D_neo_ark_power_plant_2_80182F94);
            }
            D_8007272D = 0x17;
            func_800E3FAC(0xA2, 0x2E);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            func_800E8634((s32)&D_neo_ark_power_plant_2_801802A8, 0, (s32)&D_neo_ark_power_plant_2_80180560);
        }
    }
}
