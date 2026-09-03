#include "common.h"

#include <psyq/stdio.h>

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_security_room.h"

extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

extern GpMsgEntry D_acropolis_security_room_801825DC[];

s32 func_acropolis_security_room_8017D6DC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 ret;

    if (arg0 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(arg0, arg1, arg2, arg3);
    }
    return ret;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D708);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D740);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C4);

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = Task_Spawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Task_Kill(arg0);
            }
            return;
    }
}

/* "power supply\n" plus the two bytes of alignment padding the original
 * object left in .rodata, so the block stays 16 bytes of the same content. */
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = Task_Spawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                Task_Kill(arg0);
            }
            return;
    }
}

void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->field_24 = D_acropolis_security_room_801825DC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

void func_acropolis_security_room_8017D97C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D984);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D9DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DB30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DC7C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DE80);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E0C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E37C);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E490);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E8F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E9D8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EA28);

/// Confirms the camera the player picked on the security monitor: clears the
/// action prompt, redraws the panel for the selected camera plus its cursor
/// overlay, spawns the prompt at the panel's coordinates and advances the task.
void func_acropolis_security_room_8017EA5C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    AsrMonitorWork*   work   = (AsrMonitorWork*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    func_800D4E78(prompt->screenX, prompt->screenY, work->promptKind);
    task->state = 4;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EADC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EB9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017ECB4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017ED68);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EDE4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EE44);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D63C);
