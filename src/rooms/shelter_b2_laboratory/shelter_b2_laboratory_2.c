#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"

/// Parameter block passed as the fourth argument of `Task_SpawnFromTable` to
/// the room's task table. The room's other units declare the same layout.
typedef struct {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} ShelterLaboratoryCapParams;

extern TaskDesc                   D_shelter_b2_laboratory_80182A08[];
extern TaskDesc                   D_shelter_b2_laboratory_80182A6C[];
extern s32                        D_shelter_b2_laboratory_801864B8;
extern ShelterLaboratoryCapParams D_shelter_b2_laboratory_801864BC;
extern GsCOORDINATE2              D_shelter_b2_laboratory_801864DC;
extern s8                         D_shelter_b2_laboratory_80182A90[];
extern u8                         D_8007216C;

void func_shelter_b2_laboratory_801820F4(s16 arg0);

s32 func_shelter_b2_laboratory_8017FD18(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        D_shelter_b2_laboratory_801864B8 = 0;
        if (GameFlag_GetNibble(0xD0) == 2) {
            func_shelter_b2_laboratory_801820F4(0);
            GameFlag_SetNibble(0xD0, 3);
            GameFlag_SetNibble(0xB3, 1);
            GameFlag_SetNibble(0x1C2, 0);
            GameFlag_SetNibble(0x17, 1);
            GameFlag_SetNibble(0x18, 1);
            if (GameFlag_GetNibble(0x83) != 0) {
                func_800E3FAC(0xA2, 0x28);
            } else {
                func_800E3FAC(0xA2, 0x29);
            }
            D_shelter_b2_laboratory_801864BC.field_0  = 0xD;
            D_shelter_b2_laboratory_801864BC.field_1  = 4;
            D_shelter_b2_laboratory_801864BC.field_3  = 3;
            D_shelter_b2_laboratory_801864BC.field_2  = 1;
            D_shelter_b2_laboratory_801864BC.field_14 = 0x180;
            D_shelter_b2_laboratory_801864BC.field_16 = 0x100;
            D_shelter_b2_laboratory_801864BC.field_4  = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.field_8  = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.field_10 = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.field_C  = 0x541F0007;
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 0, 1, (s32)&D_shelter_b2_laboratory_801864BC);
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 2, 0, 0);
        } else {
            D_shelter_b2_laboratory_801864BC.field_0  = 0xD;
            D_shelter_b2_laboratory_801864BC.field_1  = 1;
            D_shelter_b2_laboratory_801864BC.field_3  = 2;
            D_shelter_b2_laboratory_801864BC.field_2  = 0;
            D_shelter_b2_laboratory_801864BC.field_14 = 0;
            D_shelter_b2_laboratory_801864BC.field_4  = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.field_8  = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.field_10 = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.field_C  = 0x541F0007;
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 0, 8, (s32)&D_shelter_b2_laboratory_801864BC);
        }
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", RoomsShared8017d878Table);

void func_shelter_b2_laboratory_8017FEB8(Task* arg0)
{
    s8  pan;
    s8  depth;
    s32 vol;

    D_shelter_b2_laboratory_801864DC.coord.t[0] = 0xC1C;
    D_shelter_b2_laboratory_801864DC.coord.t[1] = -0x5DC;
    D_shelter_b2_laboratory_801864DC.coord.t[2] = -0xC80;
    D_shelter_b2_laboratory_801864DC.sub        = &gGfxViewCoord;
    D_shelter_b2_laboratory_801864DC.flg        = 0;
    Gp_UpdateCoord(&D_shelter_b2_laboratory_801864DC);
    pan   = Gp_GetObjPan(&D_shelter_b2_laboratory_801864DC);
    depth = gpGetObjDepth(&D_shelter_b2_laboratory_801864DC);
    switch (arg0->state) {
        case 0:
            SndEvt_EnqueueType6(0x541F000E, pan, depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b2_laboratory_801864B8 == 0) {
                SndEvt_EnqueueType7(0x541F000E, 1);
                taskKill(arg0);
                return;
            }
            if (D_8007216C != gGameSession->at4.loc.view) {
                arg0->state++;
            }
            break;
        case 2:
        case 3:
        case 4:
            arg0->state++;
            break;
        case 5:
            vol = 0x7F - D_shelter_b2_laboratory_80182A90[gGameSession->at4.loc.view] * 0x7F / 100;
            if (vol >= 0x80) {
                vol = 0x7F;
            }
            SndEvt_EnqueueTypeA(0x541F000E, pan, (s8)vol);
            arg0->state = 1;
            break;
    }
}
