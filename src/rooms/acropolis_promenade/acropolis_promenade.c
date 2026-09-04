#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s32      D_acropolis_promenade_80180F00;
extern s32      D_acropolis_promenade_80181068;
extern s32      D_acropolis_promenade_80181140;
extern s32      D_acropolis_promenade_80181144;
extern TaskDesc D_acropolis_promenade_80181148;

void func_acropolis_promenade_8017D5E4(void)
{
    u8 temp;
    u8 f0;

    if (D_acropolis_promenade_80181140 == 0) {
        if (Game_Session->field_8 == 4) {
            D_acropolis_promenade_80181140 = 1;
            Task_SpawnFromTable(&D_acropolis_promenade_80181148, 2, 0, 0);
        }
    }
    if (Mc_SaveData.field_5C5 == 6) {
        Mc_SaveData.field_5C5 = 5;
    }
    temp = Game_Session->field_9;
    if (temp == 1) {
        Game_Session->field_69 = 0xA;
        f0                     = Gp_StateF0.field_0;
        if (f0 == temp) {
            D_acropolis_promenade_80181144 = f0;
        }
        if ((D_acropolis_promenade_80181144 == temp) && (f0 != D_acropolis_promenade_80181144)) {
            D_acropolis_promenade_80181144 = 2;
            SndEvt_EnqueueType2(0, 0x3C);
        }
        if ((D_acropolis_promenade_80181144 == 2) && (Game_Session->field_126 != 0)) {
            D_acropolis_promenade_80181144 = 0;
            func_800E8634((s32)&D_acropolis_promenade_80180F00, 0, (s32)&D_acropolis_promenade_80181068);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", func_acropolis_promenade_8017D70C);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5DC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5E0);
