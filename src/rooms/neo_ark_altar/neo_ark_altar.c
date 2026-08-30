#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/libgpu.h"

/// `Mc_SaveData.field_4`, the area id forced for the duration of the cutscene.
extern u8 D_8007216C;
extern s8 D_801153F4;
/// Spawn table for the task started once the altar choice is committed.
extern TaskDesc D_neo_ark_altar_8017EFC0;
/// 0xFF-terminated area-record list applied the first time the altar fires.
extern GpAreaApplyRec D_neo_ark_altar_801800A0;

void func_neo_ark_altar_8017DC40(s32 arg0);

/// Altar cutscene driver: silences the player's weapon, runs cap command 2,
/// then branches on the cap event key to record the altar choice in game flag
/// 0xD9 before spawning the follow-up task and restoring control.
void func_neo_ark_altar_8017D668(Task* task)
{
    switch (task->state) {
        case 0:
            D_8007216C             = 5;
            Game_Session->field_68 = 1;
            Game_Session->field_1  = 1;
            D_801153F4             = 2;
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            Gp_RunCapCmd(2, 0);
            task->state++;
            break;
        case 3:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 4:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    GameFlag_SetNibble(0xD9, 0);
                    task->state++;
                    break;
                case 21:
                    GameFlag_SetNibble(0xD9, 1);
                    task->state++;
                    break;
                case 12:
                    task->state = 0xA;
                    break;
            }
            break;
        case 5:
            if ((GameFlag_GetNibble(0xF9) == 0) && (GameFlag_GetNibble(0xDF) == 0)) {
                GameFlag_SetNibble(0xF9, 1);
                Gp_ApplyAreaRecs(&D_neo_ark_altar_801800A0);
            }
            SndEvt_EnqueueType6(0x5514000D, 0, 0);
            task->killCountdown = 0x1E;
            task->state++;
            break;
        case 6:
            func_neo_ark_altar_8017DC40(GameFlag_GetNibble(0xD9) & 0xFF);
            task->killCountdown--;
            if (task->killCountdown <= 0) {
                task->state++;
            }
            break;
        case 7:
            if (GameFlag_GetNibble(0xD9) != 0) {
                Task_SpawnFromTable(&D_neo_ark_altar_8017EFC0, 0, 0, 0);
            } else {
                Task_SpawnFromTable(&D_neo_ark_altar_8017EFC0, 0, 1, 0);
            }
            task->state++;
            break;
        case 8:
            task->state = 0xA;
            break;
        case 10:
            SetDispMask(1);
            D_8007216C             = 2;
            Game_Session->field_68 = 0;
            Game_Session->field_1  = 0;
            D_801153F4             = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            Task_Kill(task);
            break;
    }
}
