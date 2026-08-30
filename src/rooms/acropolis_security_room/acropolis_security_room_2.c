#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

/// Scratch state of the security-room cap script, stored at `Task::idMap`.
typedef struct {
    /* 0x0 */ s32 field_0; // sub-step picked by the previous cap event
} AcropolisSecurityRoomState;

void func_acropolis_security_room_8017FD64(s32 arg0);

/// 0xFF-terminated (empty) area-record list applied once the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];

void func_acropolis_security_room_8017F1BC(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->idMap;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 2)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(3, 1, 0);
        } else if (step == 1) {
            Gp_ClearCollectedBit(0x104);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 1);
            GameFlag_SetNibble(1, 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0 = 0;
            task->state = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            Task_Kill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(3, 1, 1);
        } else {
            Gp_StartCapSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F300);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F480);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F8E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017F9C8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FA18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FB20);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FB54);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FBA4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FC30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FCB0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FD64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FE6C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FF0C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FF84);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8017FFD0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180010);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180030);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801800A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_8018014C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801801C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180218);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180294);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180308);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180368);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801804CC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801805A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180A78);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80180E34);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181108);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_801817A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181C84);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80181E28);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_2", func_acropolis_security_room_80182574);
