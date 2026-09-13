#include "common.h"

#include "actors/actor_461800.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC;

extern s16 D_80071076;
extern s8  D_8007218B;

INCLUDE_RODATA("actors/nonmatchings/actor_461800/actor_461800", D_actor_461800_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80131E38);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132048);

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

/// Exit path taken when the player leaves through this actor: two flag awards
/// first, then one of two endings depending on whether the two event flags have
/// been seen. With neither seen the session bails out (`field_128` / `field_12E`
/// are the stage-load sentinels); otherwise the save header is primed and the
/// boot loader started, with the stream RNG restored behind it. Skipped whole
/// when `D_8007218B` (the current screen id) is 9.
void func_actor_461800_8013229C(void)
{
    if (D_8007218B != 9) {
        if (GameFlag_GetNibble(0xEA) == 2) {
            Gp_SetCollectedBit(0x130);
        }
        if (GameFlag_GetNibble(0x113) != 0) {
            Gp_SetCollectedBit(0x12F);
        }
        if (GameFlag_GetNibble(0x112) == 0 && GameFlag_GetNibble(0x113) == 0) {
            Game_Session->field_128 = 0xFF;
            Game_Session->field_12E = 0xF;
            return;
        }
        Mc_SaveData.field_7 = 4;
        Mc_SaveData.field_6 = 0x24;
        Mc_SaveData.field_8 = 1;
        Mc_SaveData.field_5 = 1;
        D_80071076          = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
        Gp_RestoreStreamRng();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132390);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132660);

/// Two-state dispatcher: publishes the task's work block in
/// `D_actor_461800_80143894` on the way through, then calls the handler its
/// state selects.
void func_actor_461800_801329B0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_80132390,
        func_actor_461800_80132A0C,
    };

    D_actor_461800_80143894 = (Actor461800Work*)task->idMap;
    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132A0C);
