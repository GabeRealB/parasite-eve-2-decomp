#include "common.h"
#include <psyq/libgte.h>
#include "main/display.h"
#include "main/gamemain.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

extern u8           D_mist_parking_80186464[];
extern UiObjectDesc D_mist_parking_80186590;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017DF68);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E3F4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E540);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E90C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017EB5C);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D6D8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017ED7C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017EF24);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F108);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F31C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F49C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F764);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F938);

void func_mist_parking_8017FDB8(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_80186464);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017FE74);

void func_mist_parking_8017FF9C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Display_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_parking_80186590, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        Game_Session->field_2 = 1;
        task->spawnArg2       = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            Game_Session->field_2 = 0;
            Task_Kill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_801800D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D748);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8018089C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80180C98);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80180F94);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_801812B4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80181468);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80181760);
