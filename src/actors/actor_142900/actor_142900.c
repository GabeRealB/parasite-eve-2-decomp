#include "common.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"
extern TaskDesc       D_actor_142900_80137600;
extern s32            D_actor_142900_801382A8;
extern s32            D_actor_142900_801382AC;
extern s16            D_80071076;
extern GpAreaApplyRec D_8017E9F8;

void func_actor_142900_80131E24(Task* arg0)
{
    extern void Display_ClampField126();
    s32         var_a0;

    if (D_actor_142900_801382AC == 2) {
        D_actor_142900_801382AC = 3;
        D_actor_142900_801382A8 = 0x14;
    }
    var_a0 = rsin((arg0->killCountdown << 0xC) / 60) / 1024;
    if (D_actor_142900_801382AC == 1) {
        arg0->killCountdown = arg0->killCountdown + 1;
    }
    if (D_actor_142900_801382AC == 3) {
        var_a0                  = var_a0 * D_actor_142900_801382A8 / 20;
        D_actor_142900_801382A8 = D_actor_142900_801382A8 - 1;
        arg0->killCountdown     = arg0->killCountdown + 5;
        if (D_actor_142900_801382A8 == 0) {
            D_actor_142900_801382AC = 0;
        }
    }
    if (D_actor_142900_801382AC == 0) {
        Display_ClampField126(0);
        taskKill(arg0);
    } else {
        Display_ClampField126(var_a0);
    }
}

void func_actor_142900_80131F5C(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Gp_ApplyAreaRecs(&D_8017E9F8);
        GameFlag_SetNibble(0x4C, 0);
        Mc_SaveData.at4.loc.area = 0x1B;
        Mc_SaveData.at4.loc.warp = 2;
        Mc_SaveData.at4.loc.room = 1;
        D_80071076               = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_142900_80131FDC(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_142900_80137600, 1, 0, 0);
    }
    D_actor_142900_801382AC = arg0;
}
